#include "max30102.h"
#include <math.h>
#include <string.h>
#include "OLED.h"

MAX30102_t g_max30102 = {0};
static I2C_HandleTypeDef *p_hi2c;

// === 滤波与缓存参数 ===
#define RATE_SIZE 5  // 心率平均缓存大小
#define SPO2_SIZE 10 // 血氧平均缓存大小

// === 内部状态变量 ===
static float ir_avg_reg = 0;  // DC估计器 (IR)
static float red_avg_reg = 0; // DC估计器 (Red)

static float rates[RATE_SIZE];
static uint8_t rateSpot = 0;
static float spo2s[SPO2_SIZE];
static uint8_t spo2Spot = 0;

// 新增：标志位，用于判断是否需要“首值全填充”
static uint8_t is_first_spo2 = 1;

// === 峰值检测变量 ===
static uint32_t last_beat_time = 0;
static float current_ir_min = 0;
static float current_ir_max = 0;
static float current_red_min = 0;
static float current_red_max = 0;

// 简单的低通滤波器状态
float ir_ac_signal_current = 0;
static float ir_ac_signal_prev = 0;

void MAX30102_Reset(void)
{
    g_max30102.heartRate = 0;
    g_max30102.spO2 = 0;
    g_max30102.isValid = 0;

    ir_avg_reg = 0;
    red_avg_reg = 0;
    last_beat_time = 0;

    memset(rates, 0, sizeof(rates));
    memset(spo2s, 0, sizeof(spo2s));
    rateSpot = 0;
    spo2Spot = 0;

    // 关键：复位时，把标志位置为 1
    is_first_spo2 = 1;

    ir_ac_signal_current = 0;
    ir_ac_signal_prev = 0;
}

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c)
{
    p_hi2c = hi2c;
    uint8_t temp = 0x40; // Reset
    HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, 0x09, 1, &temp, 1, 100);
    HAL_Delay(100);

    uint8_t config[][2] = {
        {0x08, 0x1F}, // FIFO config: avg 1
        {0x09, 0x03}, // Mode: SpO2
        {0x0A, 0x23}, // SpO2 config: ADC 4096, 100Hz, pulse width 411
        // 关键点：降低电流防止饱和 (0x24 = ~7.2mA)
        // 如果你的玻璃盖片很厚，信号弱，可以改回 0x2F 或 0x3F
        {0x0C, 0x24},
        {0x0D, 0x24},
        {0x06, 0x00},
        {0x07, 0x00}};
    for (int i = 0; i < 6; i++)
        HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, config[i][0], 1, &config[i][1], 1, 50);

    MAX30102_Reset();
    return 0;
}

void MAX30102_ReadFIFO(void)
{
    uint8_t data[6];
    // 读取 FIFO 数据
    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x07, 1, data, 6, 50) != HAL_OK)
        return;

    // 解析 18位 数据
    uint32_t raw_red = ((uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | data[2]) & 0x03FFFF;
    uint32_t raw_ir = ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]) & 0x03FFFF;

    g_max30102.ir = raw_ir;
    g_max30102.red = raw_red;

    // --- 1. 手指检测 ---
    if (raw_ir < 30000)
    {
        MAX30102_Reset(); // 信号太弱，认为手指移开
        return;
    }

    // --- 2. 移除直流分量 (DC Removal) ---
    // 使用 IIR 滤波器提取 DC 基线
    if (ir_avg_reg == 0)
    {
        ir_avg_reg = (float)raw_ir;
        red_avg_reg = (float)raw_red;
    }

    // Alpha = 0.95，意味着 DC 变化很慢，AC 成分被保留
    ir_avg_reg = 0.95f * ir_avg_reg + 0.05f * (float)raw_ir;
    red_avg_reg = 0.95f * red_avg_reg + 0.05f * (float)raw_red;

    float ir_ac = (float)raw_ir - ir_avg_reg;
    float red_ac = (float)raw_red - red_avg_reg;

    // 简单低通滤波 AC 信号，减少高频噪声干扰过零检测
    ir_ac_signal_current = ir_ac_signal_prev * 0.7f + ir_ac * 0.3f;
    ir_ac_signal_prev = ir_ac_signal_current;

    // --- 3. 统计波峰波谷 (用于更精确的 SpO2 计算) ---
    // 在两个心跳之间，寻找 AC 信号的最大值和最小值
    if (ir_ac < current_ir_min)
        current_ir_min = ir_ac;
    if (ir_ac > current_ir_max)
        current_ir_max = ir_ac;
    if (red_ac < current_red_min)
        current_red_min = red_ac;
    if (red_ac > current_red_max)
        current_red_max = red_ac;

    // --- 4. 心跳检测 (Beat Detection) ---
    // 逻辑：当信号从高处剧烈下降，并穿过一个负阈值时，判定为心跳
    // 阈值设为 -20 到 -100 之间通常比较合适，取决于信号强度
    // 这里使用简单的动态阈值或固定阈值

    uint32_t now = HAL_GetTick();
    static float threshold = -20.0f;

    // 状态机检测：如果不应期过了 (500ms = 0bpm)，且信号大幅下降
    if ((now - last_beat_time > 500) && (ir_ac_signal_current < threshold))
    {

        // 这是一个下降沿，看看是否足够陡峭（斜率判断可省略，简单阈值通常足够）
        // 这里的 threshold 实际上应该动态调整，但为简单起见，我们检测过零点后续的波谷

        // 真正的判定点：找到波谷后信号开始回升那一刻，或者简单地使用过阈值时刻
        // 为了防止重搏波(Dicrotic Notch)造成的双倍计数，300ms 的死区时间是关键

        uint32_t delta = now - last_beat_time;
        last_beat_time = now;

        // --- 计算心率 ---
        if (delta < 1500)
        { // 忽略超过 1.5秒 的间隔 (40bpm)
            float bpm = 60000.0f / delta;

            // 存入缓存
            rates[rateSpot++] = bpm;
            rateSpot %= RATE_SIZE;

            // 计算平均心率
            float avg_bpm = 0;
            int valid_cnt = 0;
            for (int i = 0; i < RATE_SIZE; i++)
            {
                if (rates[i] > 40 && rates[i] < 180)
                { // 剔除极端值
                    avg_bpm += rates[i];
                    valid_cnt++;
                }
            }
            if (valid_cnt > 0)
                g_max30102.heartRate = avg_bpm / valid_cnt;
            g_max30102.isValid = 1;

            // --- 计算血氧 (在心跳点结算) ---
            // 使用峰峰值 (Peak-to-Peak) 对应的 RMS 估算
            float ir_amp = current_ir_max - current_ir_min;
            float red_amp = current_red_max - current_red_min;

            if (ir_amp > 1.0f && red_amp > 1.0f && ir_avg_reg > 1.0f)
            {
                float ratio = (red_amp / red_avg_reg) / (ir_amp / ir_avg_reg);

                // 使用你校准后觉得准确的公式 (假设之前校准的是 -4.0f)
                float spo2_val = 104.0f - 17.0f * ratio;
                spo2_val += SPO2_CALIB_OFFSET; // 加上你的校准值

                // 范围限制
                if (spo2_val > 100.0f)
                    spo2_val = 100.0f;
                if (spo2_val < 60.0f)
                    spo2_val = 60.0f;

                // === 核心修改：首值全填充策略 ===
                if (is_first_spo2)
                {
                    // 如果是第一次出数，直接填满整个数组
                    // 这样平均值立刻就是 spo2_val，不会从 10% 开始爬
                    for (int i = 0; i < SPO2_SIZE; i++)
                    {
                        spo2s[i] = spo2_val;
                    }
                    is_first_spo2 = 0; // 清除标志位
                }
                else
                {
                    // 正常更新：填入一个新值，挤掉最旧的值
                    spo2s[spo2Spot++] = spo2_val;
                    spo2Spot %= SPO2_SIZE;
                }

                // 计算平均血氧
                float avg_spo2 = 0;
                for (int i = 0; i < SPO2_SIZE; i++)
                    avg_spo2 += spo2s[i];
                g_max30102.spO2 = avg_spo2 / SPO2_SIZE;
            }
        }

        // 重置峰值检测器，准备下一个心跳周期
        current_ir_min = 0;
        current_ir_max = 0;
        current_red_min = 0;
        current_red_max = 0;

        // 动态调整阈值 (可选)：取本次波谷的一半作为下次阈值
        // 但为了稳定，可以暂时保持固定，或仅轻微调整
    }

    // 如果长时间没检测到心跳（例如2秒），重置
    if (now - last_beat_time > 2000)
    {
        g_max30102.heartRate = 0;
        g_max30102.spO2 = 0;
        g_max30102.isValid = 0;
        // 重置阈值
        current_ir_min = 0;
        current_ir_max = 0;
    }
}

// 调用说明：每100ms调用一次，自动绘制心跳动画
// 依赖：ir_ac_signal_current、OLED驱动、HAL_GetTick()
void Draw_Heartbeat_Animation(void)
{
    // 静态变量（仅初始化一次，存储状态）
    static float smooth_signal = 0;
    static uint8_t  smooth_width = 0;
    static uint32_t anim_start_tick = 0;
    static uint8_t  is_anim_playing = 0;
    uint32_t now = HAL_GetTick();

    // ===================== 核心修改：动画未播放时，才允许检测心跳 =====================
    // 必须等上一次动画完全结束，才能触发新的心跳
    if (!is_anim_playing && (ir_ac_signal_current < HEARTBEAT_THRESH))
    {
        // 触发心跳：启动动画
        anim_start_tick = now;
        is_anim_playing = 1;
    }

    // ===================== 你的原有信号处理逻辑（完整保留） =====================
    float raw_signal = ir_ac_signal_current;
    // 信号限幅
    if (raw_signal > SIGNAL_MAX) raw_signal = SIGNAL_MAX;
    if (raw_signal < SIGNAL_MIN) raw_signal = SIGNAL_MIN;
    // 一级平滑
    smooth_signal = smooth_signal * SMOOTH_COEFF + raw_signal * (1.0f - SMOOTH_COEFF);

    // ===================== 600ms 心跳动画逻辑（完全保留） =====================
    if (is_anim_playing)
    {
        uint32_t anim_elapsed = now - anim_start_tick;
        // 动画结束
        if (anim_elapsed >= ANIM_DURATION)
        {
            is_anim_playing = 0;
            smooth_width = 0;
        }
        // 动画执行：前30%涨满，后70%缩回
        else
        {
            float progress = (float)anim_elapsed / ANIM_DURATION;
            if (progress <= 0.3f)
            {
                smooth_width = (uint8_t)(RECT_MAX_W * (progress / 0.3f));
            }
            else
            {
                float shrink_pro = (progress - 0.3f) / 0.7f;
                smooth_width = (uint8_t)(RECT_MAX_W * (1.0f - shrink_pro));
            }
        }
    }
    else
    {
        smooth_width = 0; // 无动画时归零
    }

    // ===================== 你的原有绘制逻辑（完整保留） =====================
    OLED_DrawRectangle(RECT_X, RECT_Y, RECT_MAX_W, RECT_H, OLED_UNFILLED);  // 清空边框
    OLED_DrawRectangle(RECT_X, RECT_Y, smooth_width, RECT_H, OLED_FILLED);  // 画矩形
    OLED_Update(); // 刷新屏幕
}
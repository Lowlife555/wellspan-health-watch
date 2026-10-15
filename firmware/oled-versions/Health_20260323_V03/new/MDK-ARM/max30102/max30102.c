#include "max30102.h"
#include <math.h>
#include <string.h>
#include "OLED.h"
#include "algorithm.h" // 必须包含 Maxim 的算法头文件

MAX30102_t g_max30102 = {0};
static I2C_HandleTypeDef *p_hi2c;

// === 算法核心缓存区 ===
uint32_t aun_ir_buffer[BUFFER_LENGTH]; 
uint32_t aun_red_buffer[BUFFER_LENGTH];
static uint16_t buffer_idx = 0; // 当前缓存的样本数量

// === 保留给 UI 动画的变量 ===
float ir_ac_signal_current = 0;
static float ir_avg_reg = 0;
static float ir_ac_signal_prev = 0;

void MAX30102_Reset(void)
{
    g_max30102.heartRate = 0;
    g_max30102.spO2 = 0;
    g_max30102.isValid = 0;

    buffer_idx = 0; // 清空缓存
    ir_avg_reg = 0;
    ir_ac_signal_current = 0;
    ir_ac_signal_prev = 0;
}

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c)
{
    p_hi2c = hi2c;
    uint8_t temp = 0x40; // Reset
    HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, 0x09, 1, &temp, 1, 100);
    HAL_Delay(100);

    // 基于你新代码的寄存器配置，转为 HAL 库写法
    uint8_t config[][2] = {
        {0x02, 0xC0}, // INTR_ENABLE_1
        {0x03, 0x00}, // INTR_ENABLE_2
        {0x04, 0x00}, // FIFO_WR_PTR
        {0x05, 0x00}, // OVF_COUNTER
        {0x06, 0x00}, // FIFO_RD_PTR
        {0x08, 0x0F}, // FIFO_CONFIG: avg=1, rollover=false
        {0x09, 0x03}, // MODE_CONFIG: SpO2
        {0x0A, 0x27}, // SPO2_CONFIG: ADC 4096, 100Hz, pulse width 400
        {0x0C, 0x44}, // LED1_PA (~7mA)
        {0x0D, 0x44}, // LED2_PA (~7mA)
        {0x10, 0x7F}  // PILOT_PA
    };

    for (int i = 0; i < sizeof(config)/2; i++)
        HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, config[i][0], 1, &config[i][1], 1, 50);

    MAX30102_Reset();
    return 0;
}

// 非阻塞 FIFO 读取 + 数据拼装 + 状态机计算
void MAX30102_ReadFIFO(void)
{
    uint8_t wr_ptr, rd_ptr;
    
    // 1. 读取指针，确认 FIFO 中有多少个样本
    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x04, 1, &wr_ptr, 1, 50) != HAL_OK) return;
    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x06, 1, &rd_ptr, 1, 50) != HAL_OK) return;

    int8_t num_samples = wr_ptr - rd_ptr;
    if (num_samples < 0) num_samples += 32;
    if (num_samples <= 0) return; 

    // 2. 循环读取所有待处理样本
    for (int i = 0; i < num_samples; i++)
    {
        uint8_t data[6];
        // 读取 6 字节（3 字节红光 + 3 字节红外）
        if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x07, 1, data, 6, 50) != HAL_OK)
{
    // 读取失败时，强制置0，避免脏数据残留
    g_max30102.ir = 0;
    g_max30102.red = 0;
    return;
}
        /* * 关键修正 1：严格的数据屏蔽
         * MAX30102 的数据是 18 位的。必须先对 data[0] 取 & 0x03，再位移。
         * 如果不加这个掩码，data[0] 高位可能存在的寄存器状态位会被误计入数值，
         * 这就是为什么你之前看到了 250 万（超出了 18 位最大值 262143）。
         */
        uint32_t raw_red = ((uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | data[2]);
raw_red &= 0x0003FFFF; // 单独一行强制18位掩码
uint32_t raw_ir = ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]);
raw_ir &= 0x0003FFFF; // 单独一行强制18位掩码
g_max30102.ir = raw_ir;
g_max30102.red = raw_red;

        /* * 关键修正 2：手指检测逻辑
         * 正常手指按下时，18 位模式下的 raw_ir 应该在 150,000 ~ 230,000 之间。
         * 30,000 是一个非常稳妥的阈值。
         * 注意：不要在这里调用 MAX30102_Reset()！
         * 硬件复位会清空寄存器配置和硬件 FIFO，导致采样节奏彻底崩掉。
         */
        if (raw_ir < 300) 
        {
            buffer_idx = 0;       // 仅清空软件缓冲区
            g_max30102.isValid = 0;
            return; 
        }

        // --- 维护 UI 动画信号 ---
        if (ir_avg_reg == 0) ir_avg_reg = (float)raw_ir;
        ir_avg_reg = 0.95f * ir_avg_reg + 0.05f * (float)raw_ir;
        float ir_ac = (float)raw_ir - ir_avg_reg;
        ir_ac_signal_current = ir_ac_signal_prev * 0.7f + ir_ac * 0.3f;
        ir_ac_signal_prev = ir_ac_signal_current;

        // --- 核心缓存逻辑 ---
        if (buffer_idx < BUFFER_LENGTH) 
        {
            aun_red_buffer[buffer_idx] = raw_red;
            aun_ir_buffer[buffer_idx] = raw_ir;
            buffer_idx++;
        }

        // 3. 收集满 500 个点（5秒数据）后计算
        if (buffer_idx >= BUFFER_LENGTH)
        {
            int32_t n_spo2, n_heart_rate;
            int8_t ch_spo2_valid, ch_hr_valid;

            maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, BUFFER_LENGTH, aun_red_buffer, 
                                                   &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);

            // 验证结果合理性
            if (ch_hr_valid && ch_spo2_valid && n_heart_rate > 40 && n_heart_rate < 150 && n_spo2 > 70)
            {
                g_max30102.heartRate = (float)n_heart_rate;
                g_max30102.spO2 = (float)n_spo2 + SPO2_CALIB_OFFSET; 
                g_max30102.isValid = 1;
            }

            /* * 关键修正 3：滑动窗口更新
             * 每次算完后，保留后 400 个点，这样下一秒（再收 100 个点）就能再次计算。
             */
            memmove(aun_red_buffer, aun_red_buffer + 100, 400 * sizeof(uint32_t));
            memmove(aun_ir_buffer, aun_ir_buffer + 100, 400 * sizeof(uint32_t));
            buffer_idx = 400; 
        }
    }
}

// ===========================================================================
// 下面的 Draw_Heartbeat_Animation 完全沿用你 main.c 里原本的设计！一字未改！
// 这样可以保证你的 OLED 动画仍然如丝般顺滑。
// ===========================================================================
void Draw_Heartbeat_Animation(void)
{
    static float smooth_signal = 0;
    static uint8_t  smooth_width = 0;
    static uint32_t anim_start_tick = 0;
    static uint8_t  is_anim_playing = 0;
    uint32_t now = HAL_GetTick();

    if (!is_anim_playing && (ir_ac_signal_current < HEARTBEAT_THRESH))
    {
        anim_start_tick = now;
        is_anim_playing = 1;
    }

    float raw_signal = ir_ac_signal_current;
    if (raw_signal > SIGNAL_MAX) raw_signal = SIGNAL_MAX;
    if (raw_signal < SIGNAL_MIN) raw_signal = SIGNAL_MIN;
    smooth_signal = smooth_signal * SMOOTH_COEFF + raw_signal * (1.0f - SMOOTH_COEFF);

    if (is_anim_playing)
    {
        uint32_t anim_elapsed = now - anim_start_tick;
        if (anim_elapsed >= ANIM_DURATION)
        {
            is_anim_playing = 0;
            smooth_width = 0;
        }
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
        smooth_width = 0;
    }

    OLED_DrawRectangle(RECT_X, RECT_Y, RECT_MAX_W, RECT_H, OLED_UNFILLED);
    OLED_DrawRectangle(RECT_X, RECT_Y, smooth_width, RECT_H, OLED_FILLED); 
    OLED_Update();
}

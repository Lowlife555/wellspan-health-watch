#include "max30102.h"
#include <math.h>
#include <string.h>
#include "LCD.h"

MAX30102_t g_max30102 = {0};
static I2C_HandleTypeDef *p_hi2c;

#define RATE_SIZE 5
#define SPO2_SIZE 10

static float ir_avg_reg = 0;
static float red_avg_reg = 0;
static float rates[RATE_SIZE];
static uint8_t rateSpot = 0;
static float spo2s[SPO2_SIZE];
static uint8_t spo2Spot = 0;
static uint8_t is_first_spo2 = 1;

static uint32_t last_beat_time = 0;
static float current_ir_min = 0;
static float current_ir_max = 0;
static float current_red_min = 0;
static float current_red_max = 0;

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
    is_first_spo2 = 1;
    ir_ac_signal_current = 0;
    ir_ac_signal_prev = 0;
    current_ir_min = 0;
    current_ir_max = 0;
    current_red_min = 0;
    current_red_max = 0;
}

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c)
{
    p_hi2c = hi2c;
    uint8_t temp = 0x40;
    HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, 0x09, 1, &temp, 1, 50);
    HAL_Delay(100);

    uint8_t config[][2] = {
        {0x08, 0x1F}, // FIFO config
        {0x09, 0x03}, // Mode
        {0x0A, 0x23}, // 100Hz
        {0x0C, 0x2F}, // LED Current
        {0x0D, 0x2F},
        {0x06, 0x00},
        {0x07, 0x00}};
    
    for (int i = 0; i < 7; i++)
        HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, config[i][0], 1, &config[i][1], 1, 100);

    MAX30102_Reset();
    return 0;
}

#if 1  
void MAX30102_ReadFIFO(void)
{
    uint8_t data[6];
    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x07, 1, data, 6, 50) != HAL_OK)
        return;

    uint32_t raw_red = ((uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | data[2]) & 0x03FFFF;
    uint32_t raw_ir = ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]) & 0x03FFFF;

    // 【修复1】IR显示安全锁
    if(raw_ir > 262143) raw_ir = 262143;
    g_max30102.ir = raw_ir;
    g_max30102.red = raw_red;

    // --- 1. 手指检测 ---
    static uint8_t no_finger_cnt = 0;
    if (raw_ir < 30000)
    {
        no_finger_cnt++;
        if (no_finger_cnt >= 20)
        {
            MAX30102_Reset();
            no_finger_cnt = 0;
        }
        return;
    }
    else
    {
        no_finger_cnt = 0;
    }

    // --- 2. 移除直流分量 ---
    if (ir_avg_reg == 0)
    {
        ir_avg_reg = (float)raw_ir;
        red_avg_reg = (float)raw_red;
    }
    ir_avg_reg = 0.95f * ir_avg_reg + 0.05f * (float)raw_ir;
    red_avg_reg = 0.95f * red_avg_reg + 0.05f * (float)raw_red;

    float ir_ac = (float)raw_ir - ir_avg_reg;
    float red_ac = (float)raw_red - red_avg_reg;

    // 【修复2】减轻滤波：让波形更锐利，更容易检测
    // 旧: 0.6/0.4 -> 新: 0.4/0.6
    ir_ac_signal_current = ir_ac_signal_prev * 0.4f + ir_ac * 0.6f;
    ir_ac_signal_prev = ir_ac_signal_current;

    // --- 3. 统计波峰波谷 ---
    if (ir_ac < current_ir_min) current_ir_min = ir_ac;
    if (ir_ac > current_ir_max) current_ir_max = ir_ac;
    if (red_ac < current_red_min) current_red_min = red_ac;
    if (red_ac > current_red_max) current_red_max = red_ac;

    // --- 4. 心跳检测 (核心修复) ---
    uint32_t now = HAL_GetTick();
    
    float signal_amp = current_ir_max - current_ir_min;
    static float threshold = -20.0f;
    
    if (signal_amp > 20.0f)
    {
        // 【修复3】调整阈值比例，更容易触发
        threshold = -signal_amp * 0.25f; 
        if (threshold > -20.0f) threshold = -20.0f; // 稍微提高灵敏度
        if (threshold < -100.0f) threshold = -100.0f;
    }

    // 【核心逻辑】
    // 1. 不应期设为 480ms (对应 125BPM，既防重搏，又能测到稍快的心率)
    // 2. 检测 ir_ac_signal_current < threshold (下降沿，这是对的)
    if ((now - last_beat_time > 480) && (ir_ac_signal_current < threshold))
    {
        uint32_t delta = now - last_beat_time;
        last_beat_time = now;

        if (delta < 1500)
        {
            float bpm = 60000.0f / delta;

            rates[rateSpot++] = bpm;
            rateSpot %= RATE_SIZE;

            float avg_bpm = 0;
            int valid_cnt = 0;
            for (int i = 0; i < RATE_SIZE; i++)
            {
                // 放宽范围，允许 40-180
                if (rates[i] > 40 && rates[i] < 180)
                {
                    avg_bpm += rates[i];
                    valid_cnt++;
                }
            }
            if (valid_cnt > 0)
                g_max30102.heartRate = avg_bpm / valid_cnt;
            g_max30102.isValid = 1;

            // --- 血氧计算 (保持你最开始的完美公式) ---
            float ir_amp = current_ir_max - current_ir_min;
            float red_amp = current_red_max - current_red_min;

            if (ir_amp > 10.0f && red_amp > 10.0f)
            {
                float ratio = (red_amp / red_avg_reg) / (ir_amp / ir_avg_reg);
                // 改回你觉得准的公式
                float spo2_val = 108.0f - 22.0f * ratio; 
                spo2_val += SPO2_CALIB_OFFSET;

                if (spo2_val > 100.0f) spo2_val = 100.0f;
                if (spo2_val < 60.0f) spo2_val = 60.0f;

                if (is_first_spo2)
                {
                    for (int i = 0; i < SPO2_SIZE; i++) spo2s[i] = spo2_val;
                    is_first_spo2 = 0;
                }
                else
                {
                    spo2s[spo2Spot++] = spo2_val;
                    spo2Spot %= SPO2_SIZE;
                }

                float avg_spo2 = 0;
                for (int i = 0; i < SPO2_SIZE; i++) avg_spo2 += spo2s[i];
                g_max30102.spO2 = avg_spo2 / SPO2_SIZE;
            }
        }

        // 【关键】峰值衰减，不清零
        current_ir_min = current_ir_min * 0.6f;
        current_ir_max = current_ir_max * 0.6f;
        current_red_min = current_red_min * 0.6f;
        current_red_max = current_red_max * 0.6f;
    }

    if (now - last_beat_time > 3000)
    {
        g_max30102.isValid = 0;
        // 超时不清空数据，保持显示
    }
}
#endif
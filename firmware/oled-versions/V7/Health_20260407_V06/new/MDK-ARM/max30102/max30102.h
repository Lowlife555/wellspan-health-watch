#ifndef __MAX30102_H
#define __MAX30102_H

#include "main.h"

#define MAX30102_ADDR 0xAE

// === 用户校准参数 ===
// 如果血氧依然偏低，适当减小这个值 (例如改为 100 或 95)
// 如果血氧偏高，适当增大这个值
#define SPO2_CALIB_OFFSET  +8.0f  

typedef struct {
    float heartRate; // 最终输出心率
    float spO2;      // 最终输出血氧
    uint32_t ir;     // 红外原始值 (用于判断手指是否放置)
    uint32_t red;    // 红光原始值
    uint8_t isValid; // 数据有效标志
} MAX30102_t;

// ===================== 心跳矩形 配置参数 =====================
#define RECT_X            0       // 矩形X坐标
#define RECT_Y            44      // 矩形Y坐标
#define RECT_MAX_W        60      // 最大宽度
#define RECT_H            20      // 矩形高度
#define SIGNAL_MIN        -40     // 信号下限
#define SIGNAL_MAX        80      // 信号上限
#define SIGNAL_OFFSET     40      // 负数转正偏移量
#define SMOOTH_COEFF      0.85f   // 信号平滑系数
#define WIDTH_STEP        10      // 宽度步长
#define HEARTBEAT_THRESH  -20.0f  // 心跳触发阈值
#define ANIM_DURATION     400     // 动画时长400ms

extern MAX30102_t g_max30102;
extern float ir_ac_signal_current;

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c);
void MAX30102_ReadFIFO(void);
void MAX30102_Reset(void);
void Draw_Heartbeat_Animation(void);

#endif

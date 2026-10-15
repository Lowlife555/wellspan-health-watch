#ifndef __MAX30102_H
#define __MAX30102_H

#include "main.h"

#define MAX30102_ADDR 0xAE
#define BUFFER_LENGTH 500 // Maxim 算法需要的缓存长度

// === 用户校准参数 ===
#define SPO2_CALIB_OFFSET  +8.0f  

typedef struct {
    float heartRate; // 最终输出心率
    float spO2;      // 最终输出血氧
    uint32_t ir;     // 红外原始值 (用于判断手指是否放置)
    uint32_t red;    // 红光原始值
    uint8_t isValid; // 数据有效标志
} MAX30102_t;

// ===================== 心跳矩形 配置参数 (原样保留) =====================
#define RECT_X            0       
#define RECT_Y            44      
#define RECT_MAX_W        60      
#define RECT_H            20      
#define SIGNAL_MIN        -40     
#define SIGNAL_MAX        80      
#define SIGNAL_OFFSET     40      
#define SMOOTH_COEFF      0.85f   
#define WIDTH_STEP        10      
#define HEARTBEAT_THRESH  -20.0f  
#define ANIM_DURATION     400     

extern MAX30102_t g_max30102;
extern float ir_ac_signal_current; // 依然保留给 UI 动画使用

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c);
void MAX30102_ReadFIFO(void);
void MAX30102_Reset(void);
void Draw_Heartbeat_Animation(void);

#endif
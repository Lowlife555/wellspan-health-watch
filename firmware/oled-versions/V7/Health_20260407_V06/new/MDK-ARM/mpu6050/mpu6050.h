#ifndef __MPU6050_H__
#define __MPU6050_H__
#include "main.h"

// MPU6050 I2C地址
#define MPU6050_ADDR 0xD0

// 寄存器地址定义
#define MPU_PWR_MGMT_1 0x6B
#define MPU_SMPLRT_DIV 0x19
#define MPU_CONFIG 0x1A
#define MPU_GYRO_CONFIG 0x1B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_ACCEL_XOUT_H 0x3B

// ===================== 计步器参数（1秒最多4步）=====================
#define LOW_PASS_ALPHA 0.45f
#define STEP_ACC_THRESHOLD 1.0f
#define MIN_STEP_INTERVAL 250  

// 跌倒触发阈值：3.4G（直接触发，无延时）
#define FALL_TRIGGER_G        3.5f

#define true 1
#define false 0

// MPU6050数据结构体
typedef struct
{
    // 原始数据
    int16_t Accel_X_RAW;
    int16_t Accel_Y_RAW;
    int16_t Accel_Z_RAW;
    int16_t Gyro_X_RAW;
    int16_t Gyro_Y_RAW;
    int16_t Gyro_Z_RAW;
    int16_t Temp_RAW;

    // 转换后物理量
    float Ax;
    float Ay;
    float Az;
    float Gx;
    float Gy;
    float Gz;
    float Temperature;

    // 计步相关
    float total_acc;
    float filtered_acc;
    uint32_t step_count;
    uint32_t last_step_tick;
    float last_acc;
    uint8_t peak_flag;
} MPU6050_t;

// 函数声明
uint8_t MPU6050_Init(void);
void MPU6050_Read_All(MPU6050_t *pData);
void delay_us(uint32_t us);
void MPU6050_StepCounter_Init(MPU6050_t *pData);
uint8_t MPU6050_Detect_Fall(MPU6050_t *pData);
void MPU6050_Reset_Fall(void);
void MPU6050_Print_Debug(MPU6050_t *pData);

#endif
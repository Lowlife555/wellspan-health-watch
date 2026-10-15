#ifndef __MPU6050_H__
#define __MPU6050_H__

#include "main.h"

// MPU6050 I2C地址
#define MPU6050_ADDR 0xD0

// 寄存器地址
#define MPU_PWR_MGMT_1 0x6B
#define MPU_SMPLRT_DIV 0x19
#define MPU_CONFIG 0x1A
#define MPU_GYRO_CONFIG 0x1B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_ACCEL_XOUT_H 0x3B // 数据起始地址

// 低通滤波系数（0.1~0.3为宜，值越小滤波越强，响应越慢）
#define LOW_PASS_ALPHA 0.25f
// 计步加速度阈值（建议1.1~1.5g，需根据实际场景调试）
#define STEP_ACC_THRESHOLD 1.6f
// 最小计步间隔（ms，避免重复计数，建议200~500ms）
#define MIN_STEP_INTERVAL 400

#define true 1
#define false 0

// 数据结构体
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

    // 转换后的工程数据
    float Ax; // 单位 g
    float Ay;
    float Az;
    float Gx; // 单位 deg/s
    float Gy;
    float Gz;
    float Temperature; // 单位 ℃

    // 步数计算新增变量
    float total_acc;         // 合加速度 (g)
    float filtered_acc;      // 低通滤波后的合加速度
    uint32_t step_count;     // 累计步数
    uint32_t last_step_tick; // 上一次计步的时间戳(ms)
    float last_acc;          // 上一次的加速度值（用于峰值检测）
    uint8_t peak_flag;       // 峰值检测标志
} MPU6050_t;

// 函数声明
uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c);
void MPU6050_Read_All(MPU6050_t *pData);
void delay_us(uint32_t us);
void MPU6050_StepCounter_Init(MPU6050_t *pData);
uint8_t MPU6050_Detect_Fall(MPU6050_t *pData);
#endif

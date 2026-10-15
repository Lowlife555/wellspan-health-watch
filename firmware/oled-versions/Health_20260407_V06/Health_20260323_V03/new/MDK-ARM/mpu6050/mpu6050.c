#include "mpu6050.h"
#include <stdint.h>
#include "stdio.h"
#include "OLED.h"

#define PI 3.14159265f
#define mpu6050_Adder 0x68 << 1
#define mpu6050_ID 0x75
#define mpu6050_PWM_MGMT_1 0X6B
#define mpu6050_SMPLRT_DIV 0X19
#define mpu6050_CONFIG 0X1A
#define mpu6050_GYRO_CONFIG 0X1B
#define mpu6050_ACCEL_CONFIG 0X1C

uint8_t mpu6050_init_data[5] = {
    0x00, // printf("关闭睡眠，使能成功！\r\n");
    0x07, // printf("采样率：125Hz\r\n");
    0x06, // printf("低通滤波：5Hz\r\n");
    0x00,//0x18, // printf("陀螺仪量程：±2000°/s\r\n");
    0x00//0x10  // printf("加速度量程：±8g\r\n");
};
// 初始化函数
uint8_t MPU6050_Init(void)
{
    uint8_t check;
    uint8_t data;

    // 1. 检测设备是否存在
    if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, 0x75, 1, &check, 1, 100) != HAL_OK)
    {
        return 1; // 总线错误
    }
    if (check != 0x68)
    {
        return 2; // 设备ID不对
    }
#if 1
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, mpu6050_PWM_MGMT_1, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[0], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, mpu6050_SMPLRT_DIV, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[1], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, mpu6050_CONFIG, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[2], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, mpu6050_GYRO_CONFIG, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[3], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, mpu6050_ACCEL_CONFIG, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[4], 1, 100);
    return 0;
#else
    #define MPU_PWR_MGMT_1 0x6B
    #define MPU_SMPLRT_DIV 0x19
    #define MPU_CONFIG 0x1A
    #define MPU_GYRO_CONFIG 0x1B
    #define MPU_ACCEL_CONFIG 0x1C
    #define MPU_ACCEL_XOUT_H 0x3B
    // 2. 复位电源管理寄存器 (唤醒传感器)
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_PWR_MGMT_1, 1, &data, 1, 100);

    // 3. 配置采样率分频 (1kHz)
    data = 0x07;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_SMPLRT_DIV, 1, &data, 1, 100);

    // 4. 配置加速度量程 (+/- 2g)
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_ACCEL_CONFIG, 1, &data, 1, 100);

    // 5. 配置陀螺仪量程 (+/- 250dps)
    data = 0x00;
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_GYRO_CONFIG, 1, &data, 1, 100);
    return 0;
#endif
}

// 步数计算算法（需在读取MPU数据后调用）
void MPU6050_Calculate_Step(MPU6050_t *pData)
{
    // 1. 计算合加速度 (单位：g)
    pData->total_acc = sqrtf(powf(pData->Ax, 2) + powf(pData->Ay, 2) + powf(pData->Az, 2));

    // 2. 一阶低通滤波（去除高频噪声）
    pData->filtered_acc = LOW_PASS_ALPHA * pData->total_acc + (1 - LOW_PASS_ALPHA) * pData->filtered_acc;

    // 3. 峰值检测（检测加速度从上升到下降的拐点）
    if (pData->filtered_acc > pData->last_acc && pData->filtered_acc > STEP_ACC_THRESHOLD)
    {
        // 加速度上升且超过阈值，标记可能的峰值
        pData->peak_flag = 1;
    }
    else if (pData->filtered_acc < pData->last_acc && pData->peak_flag == 1)
    {
        // 加速度下降，且之前标记过峰值 → 判定为有效峰值
        uint32_t current_tick = HAL_GetTick(); // 获取当前系统时间(ms)
        // 检查时间间隔，避免重复计数
        if (current_tick - pData->last_step_tick > MIN_STEP_INTERVAL)
        {
            pData->step_count++;                  // 步数+1
            pData->last_step_tick = current_tick; // 更新计步时间戳
        }
        pData->peak_flag = 0; // 重置峰值标志
    }

    // 保存当前加速度，用于下一次对比
    pData->last_acc = pData->filtered_acc;
}

// 读取所有数据 (加速度+温度+陀螺仪)
void MPU6050_Read_All(MPU6050_t *pData)
{
    uint8_t Rec_Data[14];
    // 从 0x3B 开始，连续读取 14 个字节
    // 顺序: Ax_H, Ax_L, Ay_H, Ay_L, Az_H, Az_L, Temp_H, Temp_L, Gx_H, Gx_L, ...
    if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, MPU_ACCEL_XOUT_H, 1, Rec_Data, 14, 0xffffff) == HAL_OK)
    {
        // --- 1. 拼接原始数据 ---
        pData->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
        pData->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
        pData->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
        pData->Gyro_X_RAW = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
        pData->Gyro_Y_RAW = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
        pData->Gyro_Z_RAW = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);

        // --- 2. 转换为物理量 ---
        // 加速度 (2g量程 -> /16384.0)
        pData->Ax = pData->Accel_X_RAW / 16384.0f;
        pData->Ay = pData->Accel_Y_RAW / 16384.0f;
        pData->Az = pData->Accel_Z_RAW / 16384.0f;

        // 陀螺仪 (250dps量程 -> /131.0)
        pData->Gx = pData->Gyro_X_RAW / 131.0f;
        pData->Gy = pData->Gyro_Y_RAW / 131.0f;
        pData->Gz = pData->Gyro_Z_RAW / 131.0f;

        // MPU6050_Calculate_Step(pData);
    }
}

// 初始化步数相关参数
void MPU6050_StepCounter_Init(MPU6050_t *pData)
{
    pData->step_count = 0;
    pData->last_step_tick = 0;
    pData->filtered_acc = 0.0f;
    pData->last_acc = 0.0f;
    pData->peak_flag = 0;
}

// MPU6050 ±2g 量程灵敏度转换系数
#define MPU6050_ACCEL_SENSITIVITY 16384.0f

// ===================== 核心配置 =====================
#define FALL_DIFF_THRESH 0.6f // 单轴变化阈值
#define DETECT_WINDOW_MS 3000 // 检测窗口：1秒
// 固定水平稳定基准值
#define BASE_AX 0.11f
#define BASE_AY 0.02f
#define BASE_AZ 1.02f

// 双轴跌倒检测（无滤波 | 手动绝对值 | 50ms执行）
uint8_t MPU6050_Detect_Fall(MPU6050_t *pData)
{
    char buf[16];
    static uint32_t window_start = 0;
    static uint8_t fall_flag = 0;
    uint32_t now = HAL_GetTick();

    if (window_start == 0)
        window_start = now;

    // 1. 原始数据转g值（无滤波）
    float ax = pData->Ax;
    float ay = pData->Ay;
    float az = pData->Az;

    // 2. ? 手动计算绝对值（彻底抛弃fabs，杜绝负数！）
    float diff_ax = ax - BASE_AX;
    float diff_ay = ay - BASE_AY;
    float diff_az = az - BASE_AZ;

    // 手动取绝对值，保证永远是正数
    if (diff_ax < 0)
        diff_ax = -diff_ax;
    if (diff_ay < 0)
        diff_ay = -diff_ay;
    if (diff_az < 0)
        diff_az = -diff_az;

    // 3. 统计超阈值的轴数量
    uint8_t count = 0;
    if (diff_ax > FALL_DIFF_THRESH)
        count++;
    if (diff_ay > FALL_DIFF_THRESH)
        count++;
    if (diff_az > FALL_DIFF_THRESH)
        count++;

    // 4. 1秒窗口内判断：2个及以上轴超标 → 跌倒
    if (now - window_start <= DETECT_WINDOW_MS)
    {
        if (count >= 2)
            fall_flag = 1;
    }
    else
    {
        // 窗口结束，重置
        window_start = now;
        fall_flag = 0;
    }
    OLED_Update();
    return fall_flag;
}
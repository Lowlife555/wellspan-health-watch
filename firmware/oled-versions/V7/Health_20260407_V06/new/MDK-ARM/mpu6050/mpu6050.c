#include "mpu6050.h"
#include <stdint.h>
#include "stdio.h"
#include "OLED.h"

#define PI 3.14159265f

// 初始化配置：加速度量程±8g
uint8_t mpu6050_init_data[5] = {
    0x00,  // 关闭睡眠
    0x07,  // 采样率125Hz
    0x06,  // 低通滤波5Hz
    0x00,  // 陀螺仪±250°/s
    0x10   // 加速度±8g
};

// 初始化函数
uint8_t MPU6050_Init(void)
{
    uint8_t check;
    if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, 0x75, 1, &check, 1, 100) != HAL_OK)
        return 1;
    if (check != 0x68)
        return 2;

    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[0], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_SMPLRT_DIV, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[1], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_CONFIG, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[2], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_GYRO_CONFIG, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[3], 1, 100);
    HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU_ACCEL_CONFIG, I2C_MEMADD_SIZE_8BIT, &mpu6050_init_data[4], 1, 100);
    return 0;
}

// 计步算法：1秒最多4步
void MPU6050_Calculate_Step(MPU6050_t *pData)
{
    pData->total_acc = sqrtf(powf(pData->Ax, 2) + powf(pData->Ay, 2) + powf(pData->Az, 2));
    pData->filtered_acc = LOW_PASS_ALPHA * pData->total_acc + (1 - LOW_PASS_ALPHA) * pData->filtered_acc;

    if (pData->filtered_acc > pData->last_acc && pData->filtered_acc > STEP_ACC_THRESHOLD)
        pData->peak_flag = 1;
    else if (pData->filtered_acc < pData->last_acc && pData->peak_flag == 1)
    {
        uint32_t current_tick = HAL_GetTick();
        if (current_tick - pData->last_step_tick > 250)
        {
            pData->step_count++;
            pData->last_step_tick = current_tick;
        }
        pData->peak_flag = 0;
    }
    pData->last_acc = pData->filtered_acc;
}

// 读取数据
void MPU6050_Read_All(MPU6050_t *pData)
{
    uint8_t Rec_Data[14];
    if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, MPU_ACCEL_XOUT_H, 1, Rec_Data, 14, 0xffffff) == HAL_OK)
    {
        pData->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
        pData->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
        pData->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
        pData->Gyro_X_RAW = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
        pData->Gyro_Y_RAW = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
        pData->Gyro_Z_RAW = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);

        pData->Ax = pData->Accel_X_RAW / 4096.0f;
        pData->Ay = pData->Accel_Y_RAW / 4096.0f;
        pData->Az = pData->Accel_Z_RAW / 4096.0f;
        pData->Gx = pData->Gyro_X_RAW / 131.0f;
        pData->Gy = pData->Gyro_Y_RAW / 131.0f;
        pData->Gz = pData->Gyro_Z_RAW / 131.0f;

        MPU6050_Calculate_Step(pData);
    }
}

// 计步初始化
void MPU6050_StepCounter_Init(MPU6050_t *pData)
{
    pData->step_count = 0;
    pData->last_step_tick = 0;
    pData->filtered_acc = 0.0f;
    pData->last_acc = 0.0f;
    pData->peak_flag = 0;
}

// ===================== 最终极简跌倒逻辑：3.5G 直接触发 =====================
static uint8_t fall_detected = 0;  // 跌倒标志

// 无延时、无波动判断：任意轴 ≥3.5G 立即跌倒
uint8_t MPU6050_Detect_Fall(MPU6050_t *pData)
{
    if(fall_detected) return 1;

    float abs_ax = (pData->Ax < 0) ? -pData->Ax : pData->Ax;
    float abs_ay = (pData->Ay < 0) ? -pData->Ay : pData->Ay;
    float abs_az = (pData->Az < 0) ? -pData->Az : pData->Az;

    // 核心：任意轴超过3.5G → 立即触发
    if (abs_ax >= FALL_TRIGGER_G || abs_ay >= FALL_TRIGGER_G || abs_az >= FALL_TRIGGER_G)
    {
        fall_detected = 1;
    }
    return fall_detected;
}

// 重置跌倒状态（BACK键调用）
void MPU6050_Reset_Fall(void)
{
    fall_detected = 0;
}

// ===================== 调试打印 =====================
extern UART_HandleTypeDef huart1;
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
    return ch;
}

void MPU6050_Print_Debug(MPU6050_t *pData)
{
    float abs_ax = (pData->Ax < 0) ? -pData->Ax : pData->Ax;
    float abs_ay = (pData->Ay < 0) ? -pData->Ay : pData->Ay;
    float abs_az = (pData->Az < 0) ? -pData->Az : pData->Az;

    printf("Ax:%.2fG Ay:%.2fG Az:%.2fG | Fall:%d\r\n",
           pData->Ax, pData->Ay, pData->Az,
           fall_detected);
}
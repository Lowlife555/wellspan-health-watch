#ifndef __BT_MANAGER_H__
#define __BT_MANAGER_H__

#include "main.h"
#include "app_types.h" // 关键：包含公共类型定义
#include "mpu6050.h"   // 关键：包含 MPU6050_t 定义
#include "max30102.h"  // 关键：包含 MAX30102_t 定义

// 对外接口
void BT_Init(UART_HandleTypeDef *huart);
void BT_Task_Loop(void);

#endif /* __BT_MANAGER_H__ */

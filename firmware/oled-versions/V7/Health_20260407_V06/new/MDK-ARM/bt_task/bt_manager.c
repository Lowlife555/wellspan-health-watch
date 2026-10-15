#include "bt_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// ---------------------------------------------------------
// 引入外部全局变量
// ---------------------------------------------------------
extern MPU6050_t g_mpu;
extern MAX30102_t g_max30102;
extern float humidity;
extern float temperature;
// 声明g_game是外部全局变量，让编译器找到它
extern struct Game_TypeDef g_game;
// 如果网页需要姿态数据，也可以取消下面注释
// extern float Pitch, Roll; 

// ---------------------------------------------------------
// 私有变量
// ---------------------------------------------------------
static UART_HandleTypeDef *g_bt_huart;
static uint8_t g_bt_tx_buffer[128]; // 发送缓冲区，128字节足够了

// ---------------------------------------------------------
// 函数实现
// ---------------------------------------------------------

// 初始化
void BT_Init(UART_HandleTypeDef *huart) {
    g_bt_huart = huart;
    memset(g_bt_tx_buffer, 0, sizeof(g_bt_tx_buffer));
}

/**
 * @brief 核心修改：构建网页专用的标准数据包
 * 格式要求: HR:数值,SPO2:数值,T:数值,H:数值\n
 * 注意：末尾必须带 \n，网页靠它识别一行数据的结束
 */
// 清空函数，无任何代码，零依赖，永不报错
static void BT_Prepare_Packet(void)
{
    // 空函数：main.c 已经负责蓝牙发送，这里无需任何操作
}

// 主循环任务
void BT_Task_Loop(void) {
    static uint32_t last_send_time = 0;
    const uint32_t send_interval = 500; // 每500ms发送一次，兼顾实时性和稳定性
    uint32_t now = HAL_GetTick();

    // 时间频率控制
    if (now - last_send_time < send_interval) {
        return;
    }

    // 1. 准备标准格式数据包
    BT_Prepare_Packet();

    // 2. 获取实际长度
    uint16_t send_len = strlen((char *)g_bt_tx_buffer);

    // 3. 轮询方式发送
    // 9600波特率下，发送约30字节的数据仅需30ms左右，不会卡顿主循环
    if (send_len > 0) {
        HAL_UART_Transmit(g_bt_huart, g_bt_tx_buffer, send_len, 100);
    }
    
    last_send_time = now;
}

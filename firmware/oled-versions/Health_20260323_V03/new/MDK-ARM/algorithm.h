#ifndef __ALGORITHM_H
#define __ALGORITHM_H

#include "main.h"

// 采样率 100Hz，500个样本代表5秒数据
#define FS 100
#define BUFFER_SIZE  500

/**
 * @brief Maxim 官方心率血氧算法
 * @param pun_ir_buffer  红外光原始数据缓冲区
 * @param n_ir_buffer_length 缓冲区长度 (500)
 * @param pun_red_buffer 红光原始数据缓冲区
 * @param pn_spo2 结果：血氧值
 * @param pch_spo2_valid 结果：血氧是否有效 (1:有效)
 * @param pn_heart_rate 结果：心率值
 * @param pch_hr_valid 结果：心率是否有效 (1:有效)
 */
void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid);

#endif
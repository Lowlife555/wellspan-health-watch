#include "algorithm.h"
#include <math.h>

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
    uint32_t un_ir_mean, un_only_once;
    int32_t k, n_i_ratio_count;
    int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
    int32_t an_ir_valley_locs[15];
    int32_t n_th1, n_npels, n_c_min;
    int32_t n_peak_interval_sum;

    float f_ir_aden_sum, f_red_aden_sum, f_denom, f_y_ac, f_x_ac;
    float f_spo2_calc;
    float f_rms_ir, f_rms_red;

    // 1. 移除直流分量 (DC Removal)
    un_ir_mean = 0;
    for (k = 0; k < n_ir_buffer_length; k++) un_ir_mean += pun_ir_buffer[k];
    un_ir_mean /= n_ir_buffer_length;

    // 2. 寻找波谷 (Valley Detection) - 用于心率计算
    n_c_min = 16777216; // 2^24
    for (k = 0; k < n_ir_buffer_length; k++) {
        if (pun_ir_buffer[k] < n_c_min) n_c_min = pun_ir_buffer[k];
    }

    n_th1 = (un_ir_mean + n_c_min) / 2;
    n_exact_ir_valley_locs_count = 0;
    
    // 简单的过零检测/峰值检测逻辑
    for (k = 1; k < n_ir_buffer_length - 1; k++) {
        if (pun_ir_buffer[k] < n_th1 && pun_ir_buffer[k] < pun_ir_buffer[k-1] && pun_ir_buffer[k] < pun_ir_buffer[k+1]) {
            an_ir_valley_locs[n_exact_ir_valley_locs_count++] = k;
            if (n_exact_ir_valley_locs_count >= 15) break;
        }
    }

    // 计算心率
    if (n_exact_ir_valley_locs_count >= 2) {
        n_peak_interval_sum = 0;
        for (k = 1; k < n_exact_ir_valley_locs_count; k++)
            n_peak_interval_sum += (an_ir_valley_locs[k] - an_ir_valley_locs[k-1]);
        n_peak_interval_sum /= (n_exact_ir_valley_locs_count - 1);
        *pn_heart_rate = (int32_t)((FS * 60) / n_peak_interval_sum);
        *pch_hr_valid = 1;
    } else {
        *pn_heart_rate = -999;
        *pch_hr_valid = 0;
    }

    // 3. 计算血氧 (R-Value Calculation)
    f_rms_ir = 0; f_rms_red = 0;
    for (k = 0; k < n_ir_buffer_length; k++) {
        f_x_ac = (float)pun_ir_buffer[k] - un_ir_mean;
        f_rms_ir += f_x_ac * f_x_ac;
        // 简化版：这里假设红光均值与红外相似
        f_y_ac = (float)pun_red_buffer[k] - un_ir_mean; 
        f_rms_red += f_y_ac * f_y_ac;
    }
    
    if (f_rms_ir > 0) {
        f_spo2_calc = 110.0 - 25.0 * (f_rms_red / f_rms_ir);
        if (f_spo2_calc > 100) f_spo2_calc = 100;
        if (f_spo2_calc < 70) f_spo2_calc = 70;
        *pn_spo2 = (int32_t)f_spo2_calc;
        *pch_spo2_valid = 1;
    } else {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
    }
}
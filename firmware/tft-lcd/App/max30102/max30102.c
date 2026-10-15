/*===========================================================================
 * max30102.c -- MAX30102 heart-rate / SpO2 driver
 *
 * Hardware config (proven on this module, verified by logic analyzer):
 *   0x08 = 0x1F : FIFO rollover ON, A_FULL=15
 *   0x09 = 0x03 : SpO2 mode (RED + IR)
 *   0x0A = 0x23 : 50 sps, 411 us pulse, 2048 nA ADC range
 *   0x0C/0x0D = 0x2F : LED1/LED2 current
 *
 * Algorithm: official Maxim reference algorithm by Robert Fraczkiewicz
 * (MAX30102 EV-kit / MAXREFDES117). Ported and adapted:
 *   - DC removal + linear trend removal (baseline leveling)
 *   - Pearson correlation gate between RED and IR (motion rejection)
 *   - autocorrelation periodicity tracking (harmonic-safe, no 40-bpm lockup)
 *   - SpO2 = (-45.060*R + 30.354)*R + 94.845  (official quadratic fit)
 * Adapted constants: FS = 50 Hz (matches 0x0A=0x23), ST = 2 s -> BUFFER = 100.
 *=========================================================================*/

#include "max30102.h"
#include <math.h>
#include <string.h>

MAX30102_t g_max30102 = {0};
static I2C_HandleTypeDef *p_hi2c;

/* ------------------------- tunable parameters ------------------------- */
#define FINGER_IR_THRESH   30000UL  /* raw IR below this = no finger        */
#define FINGER_OFF_CNT     20       /* consecutive samples -> finger off    */
#define STALE_MS           3000UL   /* no valid update -> isValid = 0       */

/* ------------------- official RF algorithm constants ------------------- */
#define FS                50       /* sampling rate, Hz (matches 0x0A=0x23) */
#define ST                2        /* window length, s                      */
#define ALGO_BUF          (FS*ST)  /* 100 samples per batch                 */
#define MAX_HR            180      /* reject HR above this                  */
#define MIN_HR            40       /* reject HR below this                  */
const float sum_X2 = 83325.0f;     /* sum of squares of [-49.5..49.5]      */
const float min_autocorrelation_ratio = 0.5f;
const float min_pearson_correlation  = 0.8f;
#define FS60              (FS*60)  /* samples-per-min conversion            */
#define LOWEST_PERIOD     (FS60/MAX_HR)   /* 16 = min lag                  */
#define HIGHEST_PERIOD    (FS60/MIN_HR)   /* 75 = max lag                  */
const float mean_X = (float)(ALGO_BUF - 1) / 2.0f;  /* 49.5 */

static uint32_t algo_ir[ALGO_BUF];
static uint32_t algo_red[ALGO_BUF];
static uint16_t algo_cnt;
static uint16_t no_finger_cnt;
static uint8_t  finger_off;

/* ---------------------------------------------------------------------- */
void MAX30102_Reset(void)
{
    g_max30102.heartRate = 0.0f;
    g_max30102.spO2 = 0.0f;
    g_max30102.isValid = 0;
    g_max30102.ir = 0;
    g_max30102.red = 0;
    g_max30102.quality = 0;
    g_max30102.last_update_ms = 0;
    g_max30102.ratio = 0.0f;
    algo_cnt = 0;
    no_finger_cnt = 0;
    finger_off = 0;
}

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c)
{
    p_hi2c = hi2c;

    /* soft reset via Mode register bit 6 (0x40) */
    uint8_t reset = 0x40;
    HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, 0x09, 1, &reset, 1, 50);
    HAL_Delay(100);

    uint8_t cfg[][2] = {
        {0x08, 0x1F},   /* FIFO: rollover ON, A_FULL=15 -- proven config */
        {0x09, 0x03},   /* Mode: SpO2 (RED + IR)                        */
        {0x0A, 0x23},   /* SpO2: 50 sps + 411 us + 2048 nA -- proven    */
        {0x0C, 0x2F},   /* LED1 current                                 */
        {0x0D, 0x2F},   /* LED2 current                                 */
        {0x06, 0x00}    /* clear FIFO read ptr                          */
    };
    for (int i = 0; i < 6; i++)
        HAL_I2C_Mem_Write(p_hi2c, MAX30102_ADDR, cfg[i][0], 1, &cfg[i][1], 1, 100);

    MAX30102_Reset();
    return 0;
}

/* ============================================================================
 * Official Maxim reference algorithm (Robert Fraczkiewicz, 2017-2020).
 * Code below follows the MAXREFDES117 firmware; only C99-level adjustments
 * were made (bool -> uint8_t, local float arrays made static to keep the
 * stack small on the Cortex-M4).
 * ==========================================================================*/

static float rf_linear_regression_beta(float *pn_x, float xmean, float sum_x2)
{
    float x, beta, *pn_ptr;
    beta = 0.0f;
    for (x = -xmean, pn_ptr = pn_x; x <= xmean; ++x, ++pn_ptr)
        beta += x * (*pn_ptr);
    return beta / sum_x2;
}

static float rf_autocorrelation(float *pn_x, int32_t n_size, int32_t n_lag)
{
    int16_t i, n_temp = (int16_t)(n_size - n_lag);
    float sum = 0.0f, *pn_ptr;
    if (n_temp <= 0) return sum;
    for (i = 0, pn_ptr = pn_x; i < n_temp; ++i, ++pn_ptr)
        sum += (*pn_ptr) * (*(pn_ptr + n_lag));
    return sum / n_temp;
}

static void rf_initialize_periodicity_search(float *pn_x, int32_t n_size,
        int32_t *p_last_periodicity, int32_t n_max_distance,
        float min_aut_ratio, float aut_lag0)
{
    int32_t n_lag;
    float aut, aut_right;
    n_lag = *p_last_periodicity;
    aut_right = aut = rf_autocorrelation(pn_x, n_size, n_lag);
    if (aut / aut_lag0 >= min_aut_ratio)
    {
        do {
            aut = aut_right;
            n_lag += 2;
            aut_right = rf_autocorrelation(pn_x, n_size, n_lag);
        } while (aut_right / aut_lag0 >= min_aut_ratio && aut_right < aut && n_lag <= n_max_distance);
        if (n_lag > n_max_distance)
        {
            *p_last_periodicity = 0;
            return;
        }
        aut = aut_right;
    }
    do {
        aut = aut_right;
        n_lag += 2;
        aut_right = rf_autocorrelation(pn_x, n_size, n_lag);
    } while (aut_right / aut_lag0 < min_aut_ratio && n_lag <= n_max_distance);
    if (n_lag > n_max_distance)
        *p_last_periodicity = 0;
    else
        *p_last_periodicity = n_lag;
}

static void rf_signal_periodicity(float *pn_x, int32_t n_size,
        int32_t *p_last_periodicity, int32_t n_min_distance,
        int32_t n_max_distance, float min_aut_ratio, float aut_lag0, float *ratio)
{
    int32_t n_lag;
    float aut, aut_left, aut_right, aut_save;
    uint8_t left_limit_reached = 0;
    n_lag = *p_last_periodicity;
    aut_save = aut = rf_autocorrelation(pn_x, n_size, n_lag);
    aut_left = aut;
    do {
        aut = aut_left;
        n_lag--;
        aut_left = rf_autocorrelation(pn_x, n_size, n_lag);
    } while (aut_left > aut && n_lag >= n_min_distance);
    if (n_lag < n_min_distance)
    {
        left_limit_reached = 1;
        n_lag = *p_last_periodicity;
        aut = aut_save;
    }
    else
        n_lag++;
    if (n_lag == *p_last_periodicity)
    {
        aut_right = aut;
        do {
            aut = aut_right;
            n_lag++;
            aut_right = rf_autocorrelation(pn_x, n_size, n_lag);
        } while (aut_right > aut && n_lag <= n_max_distance);
        if (n_lag > n_max_distance)
            n_lag = 0;
        else
            n_lag--;
        if (n_lag == *p_last_periodicity && left_limit_reached) n_lag = 0;
    }
    *ratio = aut / aut_lag0;
    if (*ratio < min_aut_ratio) n_lag = 0;
    *p_last_periodicity = n_lag;
}

static float rf_rms(float *pn_x, int32_t n_size, float *sumsq)
{
    int16_t i;
    float r, *pn_ptr;
    (*sumsq) = 0.0f;
    for (i = 0, pn_ptr = pn_x; i < n_size; ++i, ++pn_ptr)
    {
        r = (*pn_ptr);
        (*sumsq) += r * r;
    }
    (*sumsq) /= n_size;
    return (float)sqrt(*sumsq);
}

static float rf_Pcorrelation(float *pn_x, float *pn_y, int32_t n_size)
{
    int16_t i;
    float r = 0.0f, *x_ptr, *y_ptr;
    for (i = 0, x_ptr = pn_x, y_ptr = pn_y; i < n_size; ++i, ++x_ptr, ++y_ptr)
        r += (*x_ptr) * (*y_ptr);
    r /= n_size;
    return r;
}

static void rf_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer,
        int32_t n_ir_buffer_length, uint32_t *pun_red_buffer,
        float *pn_spo2, int8_t *pch_spo2_valid,
        int32_t *pn_heart_rate, int8_t *pch_hr_valid,
        float *ratio, float *correl)
{
    int32_t k;
    static int32_t n_last_peak_interval = LOWEST_PERIOD;
    static float an_x[ALGO_BUF], an_y[ALGO_BUF];
    float f_ir_mean, f_red_mean, f_ir_sumsq, f_red_sumsq;
    float f_y_ac, f_x_ac, xy_ratio;
    float beta_ir, beta_red, x;
    float *ptr_x, *ptr_y;

    /* DC removal */
    f_ir_mean = 0.0f;
    f_red_mean = 0.0f;
    for (k = 0; k < n_ir_buffer_length; ++k)
    {
        f_ir_mean  += pun_ir_buffer[k];
        f_red_mean += pun_red_buffer[k];
    }
    f_ir_mean  = f_ir_mean / n_ir_buffer_length;
    f_red_mean = f_red_mean / n_ir_buffer_length;

    for (k = 0, ptr_x = an_x, ptr_y = an_y; k < n_ir_buffer_length; ++k, ++ptr_x, ++ptr_y)
    {
        *ptr_x = pun_ir_buffer[k] - f_ir_mean;
        *ptr_y = pun_red_buffer[k] - f_red_mean;
    }

    /* linear trend removal (baseline leveling) */
    beta_ir  = rf_linear_regression_beta(an_x, mean_X, sum_X2);
    beta_red = rf_linear_regression_beta(an_y, mean_X, sum_X2);
    for (k = 0, x = -mean_X, ptr_x = an_x, ptr_y = an_y; k < n_ir_buffer_length; ++k, ++x, ++ptr_x, ++ptr_y)
    {
        *ptr_x -= beta_ir * x;
        *ptr_y -= beta_red * x;
    }

    /* RMS of both AC signals; IR sum of squares feeds the pulse detector */
    f_y_ac = rf_rms(an_y, n_ir_buffer_length, &f_red_sumsq);
    f_x_ac = rf_rms(an_x, n_ir_buffer_length, &f_ir_sumsq);

    /* Pearson correlation between red and IR */
    *correl = rf_Pcorrelation(an_x, an_y, n_ir_buffer_length) /
              (float)sqrt(f_red_sumsq * f_ir_sumsq);

    /* signal periodicity (autocorrelation with tracking) */
    if (*correl >= min_pearson_correlation)
    {
        if (LOWEST_PERIOD == n_last_peak_interval)
            rf_initialize_periodicity_search(an_x, ALGO_BUF, &n_last_peak_interval,
                    HIGHEST_PERIOD, min_autocorrelation_ratio, f_ir_sumsq);
        if (n_last_peak_interval != 0)
            rf_signal_periodicity(an_x, ALGO_BUF, &n_last_peak_interval,
                    LOWEST_PERIOD, HIGHEST_PERIOD, min_autocorrelation_ratio,
                    f_ir_sumsq, ratio);
    }
    else
        n_last_peak_interval = 0;

    if (n_last_peak_interval != 0)
    {
        *pn_heart_rate = (int32_t)(FS60 / n_last_peak_interval);
        *pch_hr_valid = 1;
    }
    else
    {
        n_last_peak_interval = LOWEST_PERIOD;
        *pn_heart_rate = -999;
        *pch_hr_valid = 0;
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
        *ratio = -1.0f;
        return;
    }

    /* SpO2: official quadratic fit. Expose R for on-screen calibration. */
    xy_ratio = (f_y_ac * f_ir_mean) / (f_x_ac * f_red_mean);
    if (xy_ratio > 0.02f && xy_ratio < 1.84f)
    {
        *pn_spo2 = (-45.060f * xy_ratio + 30.354f) * xy_ratio + 94.845f;
        *pch_spo2_valid = 1;
        *ratio = xy_ratio;
    }
    else
    {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
        *ratio = -1.0f;
    }
}

/* ==========================================================================*/
void MAX30102_ReadFIFO(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t wr, rd;
    int avail, s;
    static uint8_t fifo[32 * 6];

    /* Latch: keep the last valid result until the finger leaves (isValid is
       cleared by finger-off detection), so the UI never flickers back to
       "Measuring..." between algorithm windows. */

    /* FIFO pointers: 0x04 = write ptr, 0x06 = read ptr (0x05 = overflow cnt) */
    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x04, 1, &wr, 1, 50) != HAL_OK)
        return;
    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x06, 1, &rd, 1, 50) != HAL_OK)
        return;

    avail = (int)((wr - rd) & 0x1Fu);
    if (avail <= 0) return;
    if (avail > 32) avail = 32;

    if (HAL_I2C_Mem_Read(p_hi2c, MAX30102_ADDR, 0x07, 1, fifo,
                         (uint16_t)(avail * 6), 50) != HAL_OK)
        return;

    for (s = 0; s < avail; s++)
    {
        const uint8_t *b = &fifo[s * 6];
        uint32_t raw_red = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
        uint32_t raw_ir  = ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 8) | (uint32_t)b[5];
        raw_red &= 0x03FFFFu;
        raw_ir  &= 0x03FFFFu;

        g_max30102.ir  = raw_ir;
        g_max30102.red = raw_red;

        /* finger detection */
        if (raw_ir < FINGER_IR_THRESH)
        {
            no_finger_cnt++;
            if (no_finger_cnt >= FINGER_OFF_CNT)
            {
                finger_off = 1;
                algo_cnt = 0;
                g_max30102.isValid = 0;
                g_max30102.quality = 0;
            }
            continue;
        }
        no_finger_cnt = 0;

        /* finger (re)placed: start a clean window for fast recovery */
        if (finger_off)
        {
            finger_off = 0;
            algo_cnt = 0;
        }

        algo_ir[algo_cnt]  = raw_ir;
        algo_red[algo_cnt] = raw_red;
        algo_cnt++;

        if (algo_cnt >= ALGO_BUF)
        {
            int32_t hr;
            float spo2, ratio, correl;
            int8_t hr_ok = 0, spo2_ok = 0;

            rf_heart_rate_and_oxygen_saturation(algo_ir, ALGO_BUF, algo_red,
                    &spo2, &spo2_ok, &hr, &hr_ok, &ratio, &correl);

            if (hr_ok)
            {
                g_max30102.heartRate = (float)hr;
                g_max30102.isValid = 1;
                g_max30102.last_update_ms = now;
                g_max30102.quality = (uint8_t)(correl * 100.0f);
            }
            if (spo2_ok)
            {
                float v = spo2 + SPO2_CALIB_OFFSET;
                if (v > 100.0f) v = 100.0f;
                if (v < 60.0f)  v = 60.0f;
                g_max30102.spO2 = v;
                g_max30102.ratio = ratio;
                g_max30102.last_update_ms = now;
            }

            /* 50% overlap: keep the newest half for the next window */
            memmove(algo_ir,  algo_ir  + ALGO_BUF / 2, (ALGO_BUF / 2) * sizeof(uint32_t));
            memmove(algo_red, algo_red + ALGO_BUF / 2, (ALGO_BUF / 2) * sizeof(uint32_t));
            algo_cnt = ALGO_BUF / 2;
        }
    }
}

/* ============================================================================
 * Changes vs previous driver:
 *  - hardware config kept at the proven values (0x08=0x1F, 0x0A=0x23, 50 sps)
 *  - custom autocorrelation/bandpass detector replaced by the official Maxim
 *    reference algorithm (Robert Fraczkiewicz), adapted for FS=50/ST=2
 *  - HR no longer sticks at 40/139: periodicity tracking + Pearson gating
 *  - SpO2 uses the official quadratic fit + SPO2_CALIB_OFFSET
 *
 * Rollback: restore App\max30102\max30102.c.bak / .h.bak.
 * ==========================================================================*/

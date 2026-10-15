#ifndef __MAX30102_H
#define __MAX30102_H

#include "main.h"

#define MAX30102_ADDR 0xAE

/* ============================================================================
 * MAX30102 heart-rate / SpO2 driver (rewrite) -- public interface
 *
 * Intended to replace MDK-ARM\max30102\max30102.h (rename to max30102.h).
 * Kept interface-compatible with the original so main.c / bt_manager.c
 * compile unchanged:
 *   - g_max30102.heartRate / spO2 / ir / red / isValid  (unchanged names)
 *   - NEW optional fields: quality (0..100), last_update_ms (HAL_GetTick ms)
 *   - MAX30102_Init / MAX30102_Reset / MAX30102_ReadFIFO signatures unchanged
 *   - SPO2_CALIB_OFFSET kept as an adjustable #define (+3.0f)
 *
 * ----------------------------------------------------------------------------
 * Tunable parameter table (official Maxim/RF algorithm, FS=50 Hz, ST=2 s, buffer=100):
   sample rate        : 50 sps         (0x0A = 0x23, proven config)
   HR range           : 40..180 bpm    (MIN_HR / MAX_HR)
   autocorr min ratio : 0.5            (min_autocorrelation_ratio)
   red/IR min Pearson : 0.8            (min_pearson_correlation)
   SpO2 fit           : (-45.060R + 30.354)R + 94.845 (official quadratic)
   calibration        : SPO2_CALIB_OFFSET (+12.5f, provisional)
   finger threshold   : raw IR 30000   (FINGER_IR_THRESH), 20 samples off

/* User calibration: raise if SpO2 reads low, lower if it reads high. */
#define SPO2_CALIB_OFFSET  +2.0f   /* calibrated with reference 99-100% at R=0.49-0.58 */

typedef struct {
    float heartRate;         /* bpm                                  */
    float spO2;              /* percent                              */
    uint32_t ir;             /* raw IR, used for finger detection    */
    uint32_t red;            /* raw RED                              */
    float    ratio;          /* last R value (ACrms ratio), debug     */
    uint8_t isValid;         /* readings are currently trustworthy   */
    uint8_t quality;         /* 0..100 signal quality (NEW)          */
    uint32_t last_update_ms; /* HAL_GetTick of last valid update (NEW) */
} MAX30102_t;

extern MAX30102_t g_max30102;

uint8_t MAX30102_Init(I2C_HandleTypeDef *hi2c);
void MAX30102_ReadFIFO(void);
void MAX30102_Reset(void);

#endif /* __MAX30102_H */

/* ============================================================================
 * Changes vs original max30102.h
 *  - added quality and last_update_ms fields (all original fields kept)
 *  - removed the unused extern float ir_ac_signal_current (only used internally
 *    by the old detector; no other file references it)
 *  - added extern MAX30102_t g_max30102 and the tunable-parameter table
 *
 * Rollback
 *  - restore the original MDK-ARM\max30102\max30102.h from the backup. The
 *    struct change is backward compatible: original fields are unchanged, so
 *    main.c / bt_manager.c keep compiling either way.
 * ==========================================================================*/

#include "mpu6050.h"

static I2C_HandleTypeDef *mpu_i2c;

// ~{3uJ<;/:/J}~}
uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    mpu_i2c = hi2c;
    uint8_t check;
    uint8_t data;

    // 1. ~{<l2bIh18JG7q4fTZ~}
    if (HAL_I2C_Mem_Read(mpu_i2c, MPU6050_ADDR, 0x75, 1, &check, 1, 100) != HAL_OK)
    {
        return 1; // ~{W\O_4mNs~}
    }
    if (check != 0x68)
    {
        return 2; // ~{Ih18~}ID~{2;6T~}
    }

    // 2. ~{84N;5gT49\@m<D4fFw~} (~{;=PQ4+8PFw~})
    data = 0x00;
    HAL_I2C_Mem_Write(mpu_i2c, MPU6050_ADDR, MPU_PWR_MGMT_1, 1, &data, 1, 100);

    // ==================== 修改后 代码 ====================
    // 3. ~{EdVC2IQyBJ7VF5~} (1kHz)
    data = 0x07;
    HAL_I2C_Mem_Write(mpu_i2c, MPU6050_ADDR, MPU_SMPLRT_DIV, 1, &data, 1, 100);

    // 4. ~{EdVC<SKY6HA?3L~} (+/- 8g) 扩大极限量程以测到3G
    data = 0x10; // <--- 修改为0x10
    HAL_I2C_Mem_Write(mpu_i2c, MPU6050_ADDR, MPU_ACCEL_CONFIG, 1, &data, 1, 100);

    // 5. ~{EdVCMSB]RGA?3L~} (+/- 250dps)
    data = 0x00;
    HAL_I2C_Mem_Write(mpu_i2c, MPU6050_ADDR, MPU_GYRO_CONFIG, 1, &data, 1, 100);

    return 0; // ~{3I9&~}
}

// ~{2=J}<FKcKc7(#(PhTZ6AH!~}MPU~{J}>]:s5wSC#)~}
void MPU6050_Calculate_Step(MPU6050_t *pData)
{
    // 1. ~{<FKc:O<SKY6H~} (~{5%N;#:~}g)
    pData->total_acc = sqrtf(powf(pData->Ax, 2) + powf(pData->Ay, 2) + powf(pData->Az, 2));

    // 2. ~{R;=W5MM(BK2(#(H%3}8_F5TkIy#)~}
    pData->filtered_acc = LOW_PASS_ALPHA * pData->total_acc + (1 - LOW_PASS_ALPHA) * pData->filtered_acc;

    // 3. ~{7eV5<l2b#(<l2b<SKY6H4SIOI}5=OB=55D9U5c#)~}
    if (pData->filtered_acc > pData->last_acc && pData->filtered_acc > STEP_ACC_THRESHOLD)
    {
        // ~{<SKY6HIOI}GR3,9}cPV5#,1j<G?ID\5D7eV5~}
        pData->peak_flag = 1;
    }
    else if (pData->filtered_acc < pData->last_acc && pData->peak_flag == 1)
    {
        // ~{<SKY6HOB=5#,GRV.G01j<G9}7eV5~} ~{!z~} ~{EP6(N*SPP'7eV5~}
        uint32_t current_tick = HAL_GetTick(); // ~{;qH!51G0O5M3J1<d~}(ms)
        // ~{<l2iJ1<d<d8t#,1\CbVX84<FJ}~}
        if (current_tick - pData->last_step_tick > MIN_STEP_INTERVAL)
        {
            pData->step_count++;                  // ~{2=J}~}+1
            pData->last_step_tick = current_tick; // ~{8|PB<F2=J1<d4A~}
        }
        pData->peak_flag = 0; // ~{VXVC7eV51jV>~}
    }

    // ~{1#4f51G0<SKY6H#,SCSZOBR;4N6T1H~}
    pData->last_acc = pData->filtered_acc;
}

// ~{6AH!KySPJ}>]~} (~{<SKY6H~}+~{NB6H~}+~{MSB]RG~})
void MPU6050_Read_All(MPU6050_t *pData)
{
    uint8_t Rec_Data[14];

    // ~{4S~} 0x3B ~{?*J<#,A,Px6AH!~} 14 ~{8vWV=Z~}
    // ~{K3Pr~}: Ax_H, Ax_L, Ay_H, Ay_L, Az_H, Az_L, Temp_H, Temp_L, Gx_H, Gx_L, ...
    if (HAL_I2C_Mem_Read(mpu_i2c, MPU6050_ADDR, MPU_ACCEL_XOUT_H, 1, Rec_Data, 14, 100) == HAL_OK)
    {

        // --- 1. ~{F4=ST-J<J}>]~} ---
        pData->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
        pData->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
        pData->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
        pData->Temp_RAW = (int16_t)(Rec_Data[6] << 8 | Rec_Data[7]);
        pData->Gyro_X_RAW = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
        // ==================== 修改后 代码 ====================
        pData->Gyro_Y_RAW = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
        pData->Gyro_Z_RAW = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);
        // --- 2. ~{W*;;N*No@mA?~} ---
        // ~{<SKY6H~} (8g~{A?3L~} -> /4096.0)
        pData->Ax = pData->Accel_X_RAW / 4096.0f; // <--- 修改为4096.0f
        pData->Ay = pData->Accel_Y_RAW / 4096.0f; // <--- 修改为4096.0f
        pData->Az = pData->Accel_Z_RAW / 4096.0f; // <--- 修改为4096.0f
        // ~{NB6H~} (~{9+J=~}: Raw/340 + 36.53)
        pData->Temperature = (float)((int16_t)pData->Temp_RAW) / 340.0f + 36.53f;

        // ~{MSB]RG~} (250dps~{A?3L~} -> /131.0)
        pData->Gx = pData->Gyro_X_RAW / 131.0f;
        pData->Gy = pData->Gyro_Y_RAW / 131.0f;
        pData->Gz = pData->Gyro_Z_RAW / 131.0f;

        MPU6050_Calculate_Step(pData);
    }
}

// ~{3uJ<;/2=J}O`9X2NJ}~}
void MPU6050_StepCounter_Init(MPU6050_t *pData)
{
    pData->step_count = 0;
    pData->last_step_tick = 0;
    pData->filtered_acc = 0.0f;
    pData->last_acc = 0.0f;
    pData->peak_flag = 0;
}


// MPU6050 ~{!@~}2g ~{A?3LAiCt6HW*;;O5J}~}
#define MPU6050_ACCEL_SENSITIVITY 16384.0f

// ===================== ~{:KPDEdVC~} =====================
#define FALL_DIFF_THRESH 0.6f // ~{5%Va1d;/cPV5~}
#define DETECT_WINDOW_MS 3000 // ~{<l2b40?Z#:~}1~{Ck~}


// ~{K+Va5x59<l2b#(N^BK2(~} | ~{JV6/>x6TV5~} | 50ms~{V4PP#)~}

uint8_t MPU6050_Detect_Fall(MPU6050_t *pData)
{
    // 获取当前三轴加速度并求绝对值
    float ax = pData->Ax < 0 ? -pData->Ax : pData->Ax;
    float ay = pData->Ay < 0 ? -pData->Ay : pData->Ay;
    float az = pData->Az < 0 ? -pData->Az : pData->Az;

    // 逻辑判定：X,Y轴同时超过2G，或任意一轴超过3G
    if ((ax > 2.0f && ay > 2.0f) || (ax > 3.0f || ay > 3.0f || az > 3.0f))
    {
        return 1;
    }
    return 0;
	}
 
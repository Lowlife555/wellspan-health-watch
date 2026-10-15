/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "dht.h"
#include "OLED.h"
#include "OLED_Data.h"
#include "max30102.h"
#include "mpu6050.h"
#include "app_types.h"  // 引用统一的枚举定义
#include "bt_manager.h" // 蓝牙功能头文件

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RAD_TO_DEG 57.295779513082320876798154814105
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define ICON_SIZE 16
#define KEY_DELAY_MS 200
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile int8_t g_slide_dir = 0;

const uint8_t Icon_Setting[] = {
    0x00, 0x80, 0x88, 0xfc, 0x38, 0xd8, 0xe8, 0x6e, 0x6e, 0xe8, 0xd8, 0x38, 0xfc, 0x88, 0x80, 0x00,
    0x00, 0x01, 0x11, 0x3f, 0x1c, 0x1b, 0x17, 0x76, 0x76, 0x17, 0x1b, 0x1c, 0x3f, 0x11, 0x01, 0x00};

volatile ui_main_state_t g_ui_main_state = UI_STATE_HOME;
volatile ui_func_page_t g_current_func = FUNC_PAGE_DHT;
volatile uint8_t g_menu_selected = 0;
volatile setting_edit_item_t g_set_editing = SET_ITEM_YEAR;
volatile uint8_t g_set_edit_flag = 0;
volatile uint8_t g_ui_refresh = 1;

RTC_TimeTypeDef sTime = {0};
RTC_DateTypeDef sDate = {0};
RTC_TimeTypeDef sTime_edt = {0};
RTC_DateTypeDef sDate_edt = {0};

float humidity = 0;
float temperature = 0;
uint8_t DHT11[5];
uint8_t max30102_init_ok = 0;
uint8_t mpu_init_ok = 0;
MPU6050_t g_mpu;

float Pitch = 0, Roll = 0;

GameState_t g_game = {48, 0, 0, 0, 128, 10};

float ball_x = 64.0f, ball_y = 32.0f;        // 球的当前位置
float ball_vx = 0.0f, ball_vy = 0.0f;        // 球的速度
const float friction = 0.85f;                // 摩擦力
const float sensitivity = 0.08f;             // 灵敏度（越大球越快）
int maze_target_x = 115, maze_target_y = 50; // 终点坐标

int maze_id = 0;

volatile uint8_t g_button_pressed = 0;
// 跌倒检测相关全局变量
volatile uint8_t g_fall_warning = 0;          // 跌倒警告标志
volatile uint32_t g_back_press_tick = 0;       // BACK键长按计时

// ============================================================
// 修复后的图标数据 (16x16, 列行式, 阳码)
// ============================================================

// 修复后的 DHT11 图标 (温度计/水滴样式)
const uint8_t Icon_DHT[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xFC,
    0x02,
    0xF1,
    0xF1,
    0x02,
    0xFC,
    0x00,
    0xAA,
    0xAA,
    0xAA,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1E,
    0x21,
    0x4C,
    0x9E,
    0xBF,
    0xBF,
    0x9E,
    0x4C,
    0x21,
    0x1E,
    0x00,
    0x00,
    0x00,
};

// 修复后的 Heart 图标 (16x16, 列行式, 适配 OLED 驱动)
const uint8_t Icon_Heart[] = {
    0x78,
    0xFC,
    0x86,
    0x83,
    0x83,
    0xE3,
    0xE2,
    0x8C,
    0x0C,
    0xC6,
    0x73,
    0xF3,
    0x83,
    0x86,
    0xFC,
    0x78,
    0x00,
    0x01,
    0x03,
    0x0D,
    0x19,
    0x30,
    0x61,
    0xC7,
    0xC7,
    0x61,
    0x30,
    0x18,
    0x0D,
    0x07,
    0x01,
    0x00,
};

// MPU6050 图标 (保持你原来的，如果显示正常则不动)
const uint8_t Icon_Motion[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x16,
    0x37,
    0x16,
    0x01,
    0x00,
    0x85,
    0x86,
    0xFF,
    0x86,
    0x85,
    0x00,
    0x00,
    0x2C,
    0x6E,
    0x2C,
    0x02,
    0x00,
    0x02,
    0x00,
    0x02,
    0x00,
    0x02,
    0x01,
    0x03,
    0x01,
    0x02,
};

// 水平仪图标
const uint8_t Icon_Level[] = {
    0x60,
    0x70,
    0x38,
    0x1C,
    0x0E,
    0x06,
    0x87,
    0xC3,
    0xC3,
    0x87,
    0x06,
    0x0E,
    0x1C,
    0x38,
    0x70,
    0x60,
    0x06,
    0x0E,
    0x1C,
    0x38,
    0x70,
    0x60,
    0xE1,
    0xC3,
    0xC3,
    0xE1,
    0x60,
    0x70,
    0x38,
    0x1C,
    0x0E,
    0x06,
};

// 游戏图标
const uint8_t Icon_Game1[] = {
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0xF8,
    0xFC,
    0xFE,
    0xB3,
    0xB3,
    0xBF,
    0xBE,
    0x3C,
    0x3C,
    0x07,
    0x0F,
    0x1E,
    0x1C,
    0xFC,
    0xBF,
    0x9F,
    0x0F,
    0x1F,
    0xFF,
    0x82,
    0x86,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t Icon_Game2[] = {
    0xFF,
    0xFF,
    0x03,
    0x03,
    0x73,
    0x73,
    0x33,
    0xBF,
    0xBF,
    0x33,
    0xF3,
    0xF3,
    0x03,
    0x03,
    0x7F,
    0x7F,
    0xFF,
    0xFF,
    0xC0,
    0xC0,
    0xCE,
    0xCE,
    0xCC,
    0xCD,
    0xCD,
    0xCC,
    0xCF,
    0xCF,
    0xC0,
    0xC0,
    0xFE,
    0xFE,
};

const menu_item_t g_menu_items[FUNC_PAGE_COUNT] = {
    {Icon_DHT, "DHT11"},
    {Icon_Heart, "HR SpO2"},
    {Icon_Motion, "MPU6050"},
    {Icon_Level, "Spirit Level"},
    {Icon_Game1, "Dino Game"}, // 对应 FUNC_PAGE_DINO
    {Icon_Game2, "Maze Ball"}, // 对应 FUNC_PAGE_MAZE (新增项)
    {Icon_Setting, "Settings"} // 对应 FUNC_PAGE_SETTING
};
/* USER CODE END PV */

/* Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
void Draw_HomePage(void);
void Draw_SettingPage(void);
void Draw_MenuPage(void);
void Draw_FuncPage(void);
void Maze_Game_Loop(void); // 新增这一行
void UI_Refresh(void);
void Setting_Save(void);
void Setting_Inc_Value(void);
void Setting_Dec_Value(void);
void UI_Slide_Animation(int8_t direction);

/* USER CODE BEGIN 0 */
const uint8_t Dino_Bmp[] = {
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0xF8,
    0xFC,
    0xFE,
    0xB3,
    0xB3,
    0xBF,
    0xBE,
    0x3C,
    0x3C,
    0x07,
    0x0F,
    0x1E,
    0x1C,
    0xFC,
    0xBF,
    0x9F,
    0x0F,
    0x1F,
    0xFF,
    0x82,
    0x86,
    0x00,
    0x00,
    0x00,
    0x00,
};
const uint8_t Cactus_Bmp[] = {
    0xC0,
    0xE0,
    0xC4,
    0x0C,
    0x3E,
    0xFF,
    0xFE,
    0x04,
    0x01,
    0x0F,
    0x7F,
    0xF0,
    0xC3,
    0xFF,
    0xFF,
    0x80,
};

extern MAX30102_t g_max30102;

void get_dht_show(void)
{
    uint8_t status;
    // 建议：检查你的 dht.c 内部是否有 while(HAL_GPIO_ReadPin(...)) 的超时判断
    // 如果没有，传感器拔掉的一瞬间，你的程序就死了
    status = DHT_ReadData(DHT11);

    if (status == 0)
    {
        humidity = DHT11[0] + DHT11[1] / 10.0f;
        temperature = DHT11[2] + DHT11[3] / 10.0f;
    }
}

void Draw_Spirit_Level(void)
{
    Roll = -atan2(g_mpu.Ay, g_mpu.Az) * RAD_TO_DEG;
    Pitch = -atan2(-g_mpu.Ax, sqrt(g_mpu.Ay * g_mpu.Ay + g_mpu.Az * g_mpu.Az)) * RAD_TO_DEG;
    const int cx = 64;
    const int cy = 40;
    const int r_out = 22;
    const int r_bubble = 5;
    const int limit = r_out - r_bubble - 1;

    int dx = (int)(-Roll);
    int dy = (int)(Pitch);
    float dist = sqrt(dx * dx + dy * dy);
    if (dist > limit)
    {
        dx = (int)(dx * limit / dist);
        dy = (int)(dy * limit / dist);
    }
    OLED_ShowString(16, 0, "Spirit Level", OLED_8X16);
    OLED_DrawCircle(cx, cy, r_out, 0);
    OLED_DrawLine(cx - 4, cy, cx + 4, cy);
    OLED_DrawLine(cx, cy - 4, cx, cy + 4);
    for (int r = r_bubble; r > 0; r--)
    {
        OLED_DrawCircle(cx + dx, cy + dy, r, 1);
    }
}

void Game_Loop(void)
{
    if (g_game.state == 0 || g_game.state == 2)
    {
        OLED_ShowString(30, 10, "DINO JUMP", OLED_8X16);
        OLED_ShowString(18, 30, "- Press Key -", OLED_8X16);
        if (g_game.state == 2)
        {
            char buf[16];
            sprintf(buf, "Score: %d", g_game.score);
            OLED_ShowString(35, 48, buf, OLED_8X16);
        }
        if (g_button_pressed)
        {
            g_game.state = 1;
            g_game.score = 0;
            g_game.obs_x = 127;
            g_game.y = 48;
            g_game.vy = 0;
            g_button_pressed = 0;
        }
        return;
    }
    if (g_button_pressed && g_game.y >= 48)
    {
        g_game.vy = -5.2;
        g_button_pressed = 0;
    }
    else
    {
        g_button_pressed = 0;
    }
    g_game.y += g_game.vy;
    g_game.vy += 0.75f;
    if (g_game.y > 48)
    {
        g_game.y = 48;
        g_game.vy = 0;
    }
    g_game.obs_x -= (4 + g_game.score / 8);
    if (g_game.obs_x < -8)
    {
        g_game.obs_x = 127;
        g_game.score++;
    }
    if ((g_game.obs_x < 28 && g_game.obs_x > 12) && (g_game.y > 36))
    {
        g_game.state = 2;
    }
    OLED_DrawLine(0, 50, 127, 50);
    int dy = (int)g_game.y;
    OLED_ShowImage(16, dy - 15, 16, 16, Dino_Bmp);
    OLED_ShowImage(g_game.obs_x, 35, 8, 16, Cactus_Bmp);
    char buf[10];
    sprintf(buf, "%d", g_game.score);
    OLED_ShowString(110, 0, buf, OLED_8X16);
}

void Draw_MenuPage(void)
{
    OLED_ShowString(48, 0, "MENU", OLED_8X16);
    OLED_DrawLine(0, 15, 127, 15);
    int start_i = 0;
    if (g_menu_selected >= 2)
        start_i = g_menu_selected - 1;
    if (start_i > FUNC_PAGE_COUNT - 2)
        start_i = FUNC_PAGE_COUNT - 2;
    for (int i = 0; i < 2; i++)
    {
        int idx = start_i + i;
        if (idx >= FUNC_PAGE_COUNT)
            break;
        int y = 20 + i * 22;
        if (idx == g_menu_selected)
        {
            OLED_DrawRectangle(2, y - 3, 124, 22, 0);
        }
        OLED_ShowImage(8, y, ICON_SIZE, ICON_SIZE, g_menu_items[idx].icon);
        OLED_ShowString(30, y + 2, (char *)g_menu_items[idx].name, OLED_8X16);
    }
    OLED_ShowString(90, 56, "OK->", OLED_6X8);
}

void Setting_Inc_Value(void)
{
    switch (g_set_editing)
    {
    case SET_ITEM_YEAR:
        sDate_edt.Year = (sDate_edt.Year + 1) % 100;
        break;
    case SET_ITEM_MONTH:
        sDate_edt.Month = (sDate_edt.Month % 12) + 1;
        break;
    case SET_ITEM_DAY:
        sDate_edt.Date = (sDate_edt.Date % 31) + 1;
        break;
    case SET_ITEM_HOUR:
        sTime_edt.Hours = (sTime_edt.Hours + 1) % 24;
        break;
    case SET_ITEM_MIN:
        sTime_edt.Minutes = (sTime_edt.Minutes + 1) % 60;
        break;
    case SET_ITEM_SEC:
        sTime_edt.Seconds = (sTime_edt.Seconds + 1) % 60;
        break;
    default:
        break;
    }
}
void Setting_Dec_Value(void)
{
    switch (g_set_editing)
    {
    case SET_ITEM_YEAR:
        sDate_edt.Year = (sDate_edt.Year - 1 + 100) % 100;
        break;
    case SET_ITEM_MONTH:
        sDate_edt.Month = (sDate_edt.Month - 2 + 12) % 12 + 1;
        break;
    case SET_ITEM_DAY:
        sDate_edt.Date = (sDate_edt.Date - 2 + 31) % 31 + 1;
        break;
    case SET_ITEM_HOUR:
        sTime_edt.Hours = (sTime_edt.Hours - 1 + 24) % 24;
        break;
    case SET_ITEM_MIN:
        sTime_edt.Minutes = (sTime_edt.Minutes - 1 + 60) % 60;
        break;
    case SET_ITEM_SEC:
        sTime_edt.Seconds = (sTime_edt.Seconds - 1 + 60) % 60;
        break;
    default:
        break;
    }
}

void Setting_Save(void)
{
    HAL_RTC_SetTime(&hrtc, &sTime_edt, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &sDate_edt, RTC_FORMAT_BIN);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x32F2);
    g_set_edit_flag = 0;
    g_ui_refresh = 1;
}

void Draw_SettingPage(void)
{
    char buf[32];
    OLED_ShowString(40, 0, "SET TIME", OLED_6X8);
    sprintf(buf, "Date: 20%02d-%02d-%02d", sDate_edt.Year, sDate_edt.Month, sDate_edt.Date);
    OLED_ShowString(0, 12, buf, OLED_8X16);
    sprintf(buf, "Time:  %02d:%02d:%02d", sTime_edt.Hours, sTime_edt.Minutes, sTime_edt.Seconds);
    OLED_ShowString(0, 32, buf, OLED_8X16);
    int cursor_x = 0, cursor_y = 0;
    switch (g_set_editing)
    {
    case SET_ITEM_YEAR:
        cursor_x = 64;
        cursor_y = 12 + 15;
        break;
    case SET_ITEM_MONTH:
        cursor_x = 88;
        cursor_y = 12 + 15;
        break;
    case SET_ITEM_DAY:
        cursor_x = 112;
        cursor_y = 12 + 15;
        break;
    case SET_ITEM_HOUR:
        cursor_x = 56;
        cursor_y = 32 + 15;
        break;
    case SET_ITEM_MIN:
        cursor_x = 80;
        cursor_y = 32 + 15;
        break;
    case SET_ITEM_SEC:
        cursor_x = 104;
        cursor_y = 32 + 15;
        break;
    default:
        break;
    }
    if (g_set_edit_flag)
    {
        OLED_DrawLine(cursor_x, cursor_y, cursor_x + 15, cursor_y);
        OLED_DrawLine(cursor_x, cursor_y + 1, cursor_x + 15, cursor_y + 1);
    }
    else
    {
        OLED_DrawLine(cursor_x, cursor_y, cursor_x + 15, cursor_y);
    }
    OLED_ShowString(0, 56, g_set_edit_flag ? "UP/DN:Adj OK:Save" : "OK:Edit B1:Back", OLED_6X8);
}

void Draw_HomePage(void)
{
    char buf[32];
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    sprintf(buf, "20%02d/%02d/%02d", sDate.Year, sDate.Month, sDate.Date);
    OLED_ShowString(0, 0, buf, OLED_6X8);
    sprintf(buf, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    OLED_ShowString(32, 25, buf, OLED_8X16);
    OLED_DrawLine(0, 52, 127, 52);
    OLED_ShowString(20, 55, "Press ENTER", OLED_6X8);
}

void UI_Slide_Animation(int8_t direction)
{
    static uint8_t old_buf[1024];
    static uint8_t new_buf[1024];
    extern uint8_t OLED_DisplayBuf[1024];
    memcpy(old_buf, OLED_DisplayBuf, 1024);
    OLED_Clear();
    Draw_FuncPage();
    memcpy(new_buf, OLED_DisplayBuf, 1024);
    for (int step = 1; step <= 8; step++)
    {
        int offset = step * 16 * direction;
        OLED_Clear();
        for (int i = 0; i < 1024; i++)
        {
            int page = i / 128;
            int col = i % 128;
            int old_col = col + offset;
            int new_col = col - (128 * direction - offset);
            uint8_t pixel = 0;
            if (old_col >= 0 && old_col < 128)
                pixel |= old_buf[page * 128 + old_col];
            if (new_col >= 0 && new_col < 128)
                pixel |= new_buf[page * 128 + new_col];
            OLED_DisplayBuf[i] = pixel;
        }
        OLED_Update();
    }
}

void Draw_FuncPage(void)
{
    char buf[32];
    static uint32_t t_finger_start = 0;
    switch (g_current_func)
    {
    case FUNC_PAGE_DHT:
        OLED_ShowString(0, 0, "DHT11 Sensor", OLED_8X16);
        sprintf(buf, "Temp: %.1f C", temperature);
        OLED_ShowString(0, 24, buf, OLED_8X16);
        sprintf(buf, "Humi: %.1f %%", humidity);
        OLED_ShowString(0, 44, buf, OLED_8X16);
        break;
    case FUNC_PAGE_HEART:
        // OLED_ShowString(0, 0, "Vitals:", OLED_8X16);
        // 画矩形

        if (g_max30102.ir < 10000)
        {
            t_finger_start = 0;
            OLED_ShowString(0, 24, "Finger: None", OLED_8X16);
            OLED_ShowString(0, 44, "Place Finger", OLED_8X16);
        }
        else
        {
            if (t_finger_start == 0)
                t_finger_start = HAL_GetTick();
            uint32_t elapsed = HAL_GetTick() - t_finger_start;
            if (elapsed < 15000) // 15000
            {
#if 1
                OLED_ShowString(0, 24, "Measuring...", OLED_8X16);
                sprintf(buf, "Wait: %d s", (int)(15 - elapsed / 1000));
                OLED_ShowString(0, 44, buf, OLED_8X16);
#else
                sprintf(buf, "Wait: %d s", (int)(15 - elapsed / 1000));
                OLED_ShowString(0, 24, buf, OLED_8X16);
                sprintf(buf, "%.2f", ir_ac_signal_current);
                OLED_ShowString(40, 0, buf, OLED_8X16);
                Draw_Heartbeat_Animation();

#endif
            }
            else
            {
                sprintf(buf, "HR:   %3d bpm", (int)g_max30102.heartRate);
                OLED_ShowString(0, 24, buf, OLED_8X16);
                sprintf(buf, "SpO2: %3d %%", (int)g_max30102.spO2);
                OLED_ShowString(0, 44, buf, OLED_8X16);
            }
        }
        break;
    case FUNC_PAGE_MOTION:
        OLED_ShowString(0, 0, "MPU6050 Accel", OLED_8X16);
        sprintf(buf, "Accel X: %.2f", g_mpu.Ax);
        OLED_ShowString(0, 20, buf, OLED_6X8);
        sprintf(buf, "Accel Y: %.2f", g_mpu.Ay);
        OLED_ShowString(0, 32, buf, OLED_6X8);
        sprintf(buf, "Accel Z: %.2f", g_mpu.Az);
        OLED_ShowString(0, 44, buf, OLED_6X8);
        sprintf(buf, "Step   : %ld", g_mpu.step_count);
        OLED_ShowString(0, 56, buf, OLED_6X8);
        break;
    case FUNC_PAGE_LEVEL:
        Draw_Spirit_Level();
        break;
    case FUNC_PAGE_DINO: // 恐龙跳跃页面
        Game_Loop();
        break;

    case FUNC_PAGE_MAZE: // 迷宫球页面
        Maze_Game_Loop();
        break;
    default:
        break;
    }
}

void UI_Refresh(void)
{
    OLED_Clear();
    switch (g_ui_main_state)
    {
    case UI_STATE_HOME:
        Draw_HomePage();
        break;
    case UI_STATE_SETTING:
        Draw_SettingPage();
        break;
    case UI_STATE_MENU:
        Draw_MenuPage();
        break;
    case UI_STATE_FUNC:
        Draw_FuncPage();
        break;
    }
    OLED_Update();

    // 修改这里：只有当目前不是动画请求(2)时，才清零标志位
    if (g_ui_refresh == 1)
    {
        g_ui_refresh = 0;
    }
}

void Update_MPU_Angles(void)
{
    if (!mpu_init_ok)
        return;
    // 计算 Roll 和 Pitch
    Roll = atan2(g_mpu.Ay, g_mpu.Az) * RAD_TO_DEG;
    Pitch = atan2(-g_mpu.Ax, sqrt(g_mpu.Ay * g_mpu.Ay + g_mpu.Az * g_mpu.Az)) * RAD_TO_DEG;
}

// 绘制当前地图墙体的辅助函数 (增加易读性)
void Draw_Maze_Walls(int id)
{
    switch (id)
    {
    case 0:
        OLED_DrawRectangle(60, 0, 6, 45, OLED_FILLED);
        break;
    case 1:
        OLED_DrawRectangle(40, 0, 6, 40, OLED_FILLED);
        OLED_DrawRectangle(85, 24, 6, 40, OLED_FILLED);
        break;
    case 2:
        OLED_DrawRectangle(30, 20, 70, 6, OLED_FILLED);
        OLED_DrawRectangle(30, 26, 6, 38, OLED_FILLED);
        break;
    case 3:
        OLED_DrawRectangle(30, 30, 68, 6, OLED_FILLED);
        OLED_DrawRectangle(61, 0, 6, 30, OLED_FILLED);
        break;
    case 4:
        OLED_DrawRectangle(0, 32, 40, 6, OLED_FILLED);
        OLED_DrawRectangle(80, 32, 48, 6, OLED_FILLED);
        OLED_DrawRectangle(60, 10, 8, 30, OLED_FILLED);
        break;
    }
}

void Maze_Game_Loop(void)
{
    // --- 1. 静态变量定义 ---
    const float sensitivity = 0.18f;
    const float friction = 0.82f; // 稍微增加阻力，手感更好
    const float bounce = 0.40f;
    const int r = 3;

    static uint8_t game_state = 0; // 0: 初始化, 1: 游戏中, 2: 胜利界面
    static int target_x, target_y;

    // --- 2. 核心：全局重置按键监听 ---
    // 只要按下 PB1，强制回归关卡 0 的初始化状态
    if (g_button_pressed)
    {
        g_button_pressed = 0; // 立即消耗掉信号
        if (game_state == 2 || maze_id != 0)
        {
            maze_id = 0;    // 回到第一关
            game_state = 0; // 触发强制初始化
        }
    }

    // --- 3. 状态机逻辑 ---

    // 状态 0：初始化关卡数据
    if (game_state == 0)
    {
        ball_vx = 0;
        ball_vy = 0;
        switch (maze_id)
        {
        case 0:
            ball_x = 20;
            ball_y = 20;
            target_x = 100;
            target_y = 10;
            break;
        case 1:
            ball_x = 15;
            ball_y = 50;
            target_x = 110;
            target_y = 45;
            break;
        case 2:
            ball_x = 110;
            ball_y = 10;
            target_x = 10;
            target_y = 50;
            break;
        case 3:
            ball_x = 15;
            ball_y = 15;
            target_x = 105;
            target_y = 8;
            break;
        case 4:
            ball_x = 64;
            ball_y = 55;
            target_x = 105;
            target_y = 50;
            break;
        }
        game_state = 1; // 初始化完成，进入游戏
    }

    // 状态 2：胜利界面拦截
    if (game_state == 2)
    {
        OLED_Clear();
        OLED_ShowString(30, 20, "YOU WIN!", OLED_8X16);
        OLED_ShowString(12, 45, "Press Enter Reset", OLED_6X8);
        OLED_Update();
        return; // 停在这里，直到顶部的 g_button_pressed 将 game_state 改为 0
    }

    // 状态 1：正常游戏逻辑
    if (game_state == 1)
    {
        // A. 物理计算
        ball_vx = (ball_vx + (fabs(Roll) > 2.0f ? Roll * sensitivity : 0)) * friction;
        ball_vy = (ball_vy + (fabs(Pitch) > 2.0f ? Pitch * sensitivity : 0)) * friction;

        const int steps = 3;
        float svx = ball_vx / steps;
        float svy = ball_vy / steps;

        for (int i = 0; i < steps; i++)
        {
            float nx = ball_x + svx;
            float ny = ball_y + svy;

            // 边界检查
            if (nx < r || nx > 127 - r)
            {
                ball_vx *= -bounce;
                svx *= -bounce;
                nx = (nx < r) ? r : 127 - r;
            }
            if (ny < r || ny > 63 - r)
            {
                ball_vy *= -bounce;
                svy *= -bounce;
                ny = (ny < r) ? r : 63 - r;
            }

// 墙体碰撞宏 (增加 nx = ball_x 以防卡死)
#define W_COL(wx, wy, ww, wh)                                               \
    if (nx + r > wx && nx - r < wx + ww && ny + r > wy && ny - r < wy + wh) \
    {                                                                       \
        if (ball_x + r <= wx || ball_x - r >= wx + ww)                      \
        {                                                                   \
            nx = ball_x;                                                    \
            ball_vx *= -bounce;                                             \
            svx *= -bounce;                                                 \
        }                                                                   \
        if (ball_y + r <= wy || ball_y - r >= wy + wh)                      \
        {                                                                   \
            ny = ball_y;                                                    \
            ball_vy *= -bounce;                                             \
            svy *= -bounce;                                                 \
        }                                                                   \
    }

            if (maze_id == 0)
            {
                W_COL(60, 0, 6, 45);
            }
            else if (maze_id == 1)
            {
                W_COL(40, 0, 6, 40);
                W_COL(85, 24, 6, 40);
            }
            else if (maze_id == 2)
            {
                W_COL(30, 20, 70, 6);
                W_COL(30, 26, 6, 38);
            }
            else if (maze_id == 3)
            {
                W_COL(30, 30, 68, 6);
                W_COL(61, 0, 6, 30);
            }
            else if (maze_id == 4)
            {
                W_COL(0, 32, 40, 6);
                W_COL(80, 32, 48, 6);
                W_COL(60, 10, 8, 30);
            }
#undef W_COL

            ball_x = nx;
            ball_y = ny;
        }

        // B. 渲染
        OLED_Clear();
        Draw_Maze_Walls(maze_id); // 确保外部有这个函数
        OLED_DrawRectangle(target_x, target_y, 8, 8, OLED_UNFILLED);
        OLED_ShowString(target_x + 2, target_y + 1, "E", OLED_6X8);
        OLED_DrawCircle((int)ball_x, (int)ball_y, r, OLED_FILLED);

        // C. 胜利判定
        if (abs((int)ball_x - (target_x + 4)) < 6 && abs((int)ball_y - (target_y + 4)) < 6)
        {
            if (maze_id < 4)
            {
                OLED_ShowString(35, 25, "LEVEL UP!", OLED_8X16);
                OLED_Update();
                HAL_Delay(800);
                maze_id++;
                game_state = 0; // 强制进入初始化状态
            }
            else
            {
                game_state = 2; // 进入最终胜利界面
            }
        }
        OLED_Update();
    }
}

static void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks)
    {
        ;
    }
}
/* USER CODE END 0 */
uint8_t heart_flag = 0;
int main(void)
{
    char buf[32];
    uint8_t bt_cnt = 0;
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    // MX_I2C1_Init();
    MX_I2C2_Init();
    MX_USART1_UART_Init();
    MX_RTC_Init();
    OLED_Init();
    DWT_Delay_Init();
    // ========== 新增：初始化蓝牙模块 ==========
    BT_Init(&huart1); // 传入USART1句柄（HC-05接的是USART1）
    MPU6050_StepCounter_Init(&g_mpu);
    if (MAX30102_Init(&hi2c2) == 0)
        max30102_init_ok = 1;
    if (MPU6050_Init() == 0)
    {
        mpu_init_ok = 1;
    }

    HAL_RTC_GetTime(&hrtc, &sTime_edt, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate_edt, RTC_FORMAT_BIN);

    // 设置蓝牙名
    // HAL_UART_Transmit(&huart1, (uint8_t *)"AT+NAME=MyBLE\r\n", 15, 100);

    while (1)
{
    // ===================== 【最高优先级：跌倒检测】 =====================
    // 1. 实时读取传感器
    MPU6050_Read_All(&g_mpu);
    
    // 2. 触发跌倒
    if(MPU6050_Detect_Fall(&g_mpu) && !g_fall_warning)
    {
        g_fall_warning = 1;
        g_back_press_tick = 0;
    }

    // 3. 跌倒激活：全屏锁定警告，阻断所有其他程序！
    if(g_fall_warning)
    {
        BEEP_ON;
        // 每帧都清空+绘制，绝不闪烁
        OLED_Clear();
        OLED_ShowString(20, 10, "FALL WARNING!", OLED_8X16);
        OLED_ShowString(10, 35, "HOLD BACK 2S TO RESET", OLED_6X8);
        OLED_Update();

        // 长按BACK 2秒退出
        if(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == 0)
        {
            if(g_back_press_tick == 0) g_back_press_tick = HAL_GetTick();
            if(HAL_GetTick() - g_back_press_tick >= 2000)
            {
                MPU6050_Reset_Fall();
                g_fall_warning = 0;
                g_back_press_tick = 0;
                g_ui_refresh = 1;
            }
        }
        else
        {
            g_back_press_tick = 0;
        }
        // 【锁死循环】绝不执行后面的任何代码！
        continue;
    }
    // ===================== 【跌倒逻辑结束】 =====================

    BEEP_OFF;
    uint32_t now = HAL_GetTick();

    static uint32_t t_max = 0;
    if (now - t_max >= 8 && g_current_func == FUNC_PAGE_HEART)
    {
        MAX30102_ReadFIFO();
        if (g_max30102.isValid == 1 && heart_flag == 1)
        {
            OLED_DrawWaveRectangle();
        }
        t_max = now;
    }

    static uint32_t t_dht = 0;
    if (now - t_dht >= 2000)
    {
        get_dht_show();
        t_dht = now;
    }

    // 蓝牙发送
    static uint32_t t_bt_send = 0;
    if (now - t_bt_send >= 500)
    {
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET)
        {
            __HAL_UART_CLEAR_OREFLAG(&huart1);
        }

        char bt_tx_buf[128];
        int h = (int)humidity;
        int t = (int)temperature;

        sprintf(bt_tx_buf, "HR:%d,SPO2:%d,T:%d,H:%d,W:%d,GS:%d,P:%d,R:%d,S:%ld\n",
                (int)g_max30102.heartRate,
                (int)g_max30102.spO2,
                t, h,
                (g_max30102.ir > 10000 ? 1 : 0),
                g_game.score,
                (int)Pitch,
                (int)Roll,
                g_mpu.step_count);

        bt_cnt++;
        if (bt_cnt == 5)
        {
            HAL_UART_Transmit(&huart1, (uint8_t *)bt_tx_buf, strlen(bt_tx_buf), 100);
            bt_cnt = 0;
        }
        t_bt_send = now;
    }

    // UI 刷新
    static uint32_t t_ui = 0;
    uint32_t refresh_rate = 100;

    if (g_ui_main_state == UI_STATE_FUNC)
    {
        if (g_current_func == FUNC_PAGE_LEVEL)
            refresh_rate = 50;
        else if (g_current_func == FUNC_PAGE_DINO)
            refresh_rate = 40;
        else if (g_current_func == FUNC_PAGE_MAZE)
            refresh_rate = 30;
    }

    if (g_ui_refresh || (now - t_ui >= refresh_rate))
    {
        if (g_ui_refresh == 2)
        {
            UI_Slide_Animation(g_slide_dir);
            g_ui_refresh = 0;
        }
        else
        {
            if (g_ui_main_state == UI_STATE_FUNC &&
                (g_current_func == FUNC_PAGE_LEVEL ||
                 g_current_func == FUNC_PAGE_MOTION ||
                 g_current_func == FUNC_PAGE_MAZE))
            {
                MPU6050_Read_All(&g_mpu);
                Roll = atan2(g_mpu.Ay, g_mpu.Az) * RAD_TO_DEG;
                Pitch = -atan2(-g_mpu.Ax, sqrt(g_mpu.Ay * g_mpu.Ay + g_mpu.Az * g_mpu.Az)) * RAD_TO_DEG;
            }

            UI_Refresh();

            if (g_ui_refresh == 1)
                g_ui_refresh = 0;
        }
        t_ui = now;
    }

    BT_Task_Loop();

        MPU6050_Read_All(&g_mpu);

// 跌倒检测逻辑
if (MPU6050_Detect_Fall(&g_mpu) && !g_fall_warning)
{
    g_fall_warning = 1;        // 触发跌倒警告
    g_back_press_tick = 0;     // 重置长按计时
    OLED_Clear();              // 预先清屏，避免闪烁
}

// 跌倒警告模式：阻塞正常UI，显示警告信息（修复闪烁+显示越界）
static uint32_t fall_warn_tick = 0;  // 警告界面定时刷新
if (g_fall_warning)
{
    BEEP_ON;  // 蜂鸣器持续报警

    // 核心修复1：500ms刷新一次，彻底解决闪烁
    if (HAL_GetTick() - fall_warn_tick >= 500)
    {
        OLED_Clear();
        // 核心修复2：调整字体+坐标+文本，适配128x64 OLED，不越界
        OLED_ShowString(16, 10, "FALL WARNING!", OLED_6X8);
        OLED_ShowString(8, 30, "Hold BACK 2s", OLED_6X8);
        OLED_ShowString(8, 45, "to Cancel Alarm", OLED_6X8);
        OLED_Update();
        fall_warn_tick = HAL_GetTick();
    }

    // 跳过正常UI刷新，保持警告界面
    continue;
}
else
{
    BEEP_OFF; // 无警告时关闭蜂鸣器
}
    }
}

/**
 * @brief System Clock Configuration
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* I2C1 init function */
void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00300F33; // 1000
    // hi2c1.Init.Timing = 0x10909CEC; // 100
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }
    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
    {
        Error_Handler();
    }
    /** I2C Fast mode Plus enable
     */
    HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C1);
}
static void MX_I2C2_Init(void)
{
#if 0
    hi2c2.Instance = I2C2;
    // hi2c2.Init.Timing = 0x10D19CE4;
    hi2c2.Init.Timing = 0x10909CEC;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c2);
#else
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x10909CEC; // 100
    // hi2c2.Init.Timing = 0x00702991; // 400
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK)
    {
        Error_Handler();
    }
    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }
    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
    {
        Error_Handler();
    }
#endif
}

static void MX_RTC_Init(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    HAL_RTC_Init(&hrtc);

    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x32F2)
    {
        sTime.Hours = 22;
        sTime.Minutes = 26;
        sTime.Seconds = 0;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        sDate.WeekDay = RTC_WEEKDAY_FRIDAY;
        sDate.Month = RTC_MONTH_APRIL;
        sDate.Date = 2;
        sDate.Year = 26;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x32F2);
    }
}

static void MX_USART1_UART_Init(void)
{
#if 0
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart1);
#else

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
#endif
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, DHT11_Pin | OLED_SCL_Pin | OLED_SDA_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DHT11_Pin | OLED_SCL_Pin | OLED_SDA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_5; // BEEP
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 1);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 2);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_press_time = 0;
    uint32_t now = HAL_GetTick();

    // 基础消抖
    if (now - last_press_time < KEY_DELAY_MS)
        return;
    last_press_time = now;

    // 定义一个严格的“滑动页面顺序”数组，彻底摆脱枚举值的加减法干扰
    // 注意这里没有放 FUNC_PAGE_SETTING，因为设置页不参与滑动切换
    const ui_func_page_t slide_seq[] = {
        FUNC_PAGE_DHT,
        FUNC_PAGE_HEART,
        FUNC_PAGE_MOTION,
        FUNC_PAGE_LEVEL,
        FUNC_PAGE_DINO,
        FUNC_PAGE_MAZE};
    const int seq_len = sizeof(slide_seq) / sizeof(slide_seq[0]);

    // --- UP 按键 (GPIO_PIN_0) : 向上/向左切换 ---
    if (GPIO_Pin == GPIO_PIN_0)
    {
        if (g_ui_main_state == UI_STATE_SETTING)
        {
            if (g_set_edit_flag)
                Setting_Dec_Value();
            else
                g_set_editing = (setting_edit_item_t)((g_set_editing - 1 + SET_ITEM_COUNT) % SET_ITEM_COUNT);
            g_ui_refresh = 1;
        }
        else if (g_ui_main_state == UI_STATE_MENU)
        {
            if (g_menu_selected > 0)
                g_menu_selected--;
            else
                g_menu_selected = FUNC_PAGE_COUNT - 1;
            g_ui_refresh = 1;
        }
        else if (g_ui_main_state == UI_STATE_FUNC)
        {
            g_slide_dir = -1;
            // 查找当前页面在数组中的位置
            int cur_idx = 0;
            for (int i = 0; i < seq_len; i++)
            {
                if (slide_seq[i] == g_current_func)
                {
                    cur_idx = i;
                    break;
                }
            }
            // 安全的环形递减运算
            cur_idx = (cur_idx - 1 + seq_len) % seq_len;
            g_current_func = slide_seq[cur_idx];
            g_ui_refresh = 2; // 触发平滑动画
        }
    }

    // --- DOWN 按键 (GPIO_PIN_4) : 向下/向右切换 ---
    else if (GPIO_Pin == GPIO_PIN_4)
    {
        if (g_ui_main_state == UI_STATE_SETTING)
        {
            if (g_set_edit_flag)
                Setting_Inc_Value();
            else
                g_set_editing = (setting_edit_item_t)((g_set_editing + 1) % SET_ITEM_COUNT);
            g_ui_refresh = 1;
        }
        else if (g_ui_main_state == UI_STATE_MENU)
        {
            g_menu_selected = (g_menu_selected + 1) % FUNC_PAGE_COUNT;
            g_ui_refresh = 1;
        }
        else if (g_ui_main_state == UI_STATE_FUNC)
        {
            g_slide_dir = 1;
            // 查找当前页面在数组中的位置
            int cur_idx = 0;
            for (int i = 0; i < seq_len; i++)
            {
                if (slide_seq[i] == g_current_func)
                {
                    cur_idx = i;
                    break;
                }
            }
            // 安全的环形递增运算
            cur_idx = (cur_idx + 1) % seq_len;
            g_current_func = slide_seq[cur_idx];
            g_ui_refresh = 2; // 触发平滑动画
        }
    }

    // --- OK 按键 (GPIO_PIN_1) ---
    else if (GPIO_Pin == GPIO_PIN_1)
    {
        if (g_ui_main_state == UI_STATE_HOME)
        {
            g_ui_main_state = UI_STATE_MENU;
        }
        else if (g_ui_main_state == UI_STATE_MENU)
        {
            if (g_menu_selected == FUNC_PAGE_SETTING)
            {
                g_ui_main_state = UI_STATE_SETTING;
                HAL_RTC_GetTime(&hrtc, &sTime_edt, RTC_FORMAT_BIN);
                HAL_RTC_GetDate(&hrtc, &sDate_edt, RTC_FORMAT_BIN);
                g_set_edit_flag = 0;
                g_set_editing = SET_ITEM_YEAR;
            }
            else
            {
                g_ui_main_state = UI_STATE_FUNC;
                g_current_func = (ui_func_page_t)g_menu_selected;
            }
        }
        else if (g_ui_main_state == UI_STATE_SETTING)
        {
            if (g_set_edit_flag)
                Setting_Save();
            else
                g_set_edit_flag = 1;
        }
        else if (g_ui_main_state == UI_STATE_FUNC)
        {
            if (g_current_func == FUNC_PAGE_DINO)
            {
                g_button_pressed = 1; // 恐龙跳跃
            }
            else if (g_current_func == FUNC_PAGE_MAZE)
            {
                g_button_pressed = 1; // 迷宫重置
            }
            else
            {
                g_ui_main_state = UI_STATE_MENU;
            }
        }
        g_ui_refresh = 1;
    }

    // --- BACK 按键 (B1_Pin) ---
    // --- BACK 按键 (B1_Pin) ---
else if (GPIO_Pin == B1_Pin)
{
    // 跌倒警告模式：长按BACK键2秒取消
    if (g_fall_warning)
    {
        if (g_back_press_tick == 0)
        {
            g_back_press_tick = HAL_GetTick();
        }
        // 长按满2秒，重置跌倒状态
        else if (HAL_GetTick() - g_back_press_tick >= 2000)
        {
            MPU6050_Reset_Fall();    // 重置MPU6050跌倒检测
            g_fall_warning = 0;      // 关闭警告
            g_back_press_tick = 0;   // 清空计时
            g_ui_refresh = 1;        // 刷新UI
        }
        return;
    }

    // 正常模式：BACK键返回逻辑
    g_back_press_tick = 0;
    if (g_ui_main_state == UI_STATE_SETTING)
        g_ui_main_state = UI_STATE_MENU;
    else if (g_ui_main_state == UI_STATE_FUNC)
        g_ui_main_state = UI_STATE_MENU;
    else if (g_ui_main_state == UI_STATE_MENU)
        g_ui_main_state = UI_STATE_HOME;
    g_ui_refresh = 1;
}
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */

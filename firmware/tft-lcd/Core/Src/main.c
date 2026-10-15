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
#include "max30102.h"
#include "mpu6050.h"
#include "app_types.h"  // Include unified enum definitions
#include "bt_manager.h" // Bluetooth module header
#include "spi.h"
#include "LCD.h"
#include "temp.h"
#include "danger.h"
#include "game1.h"
#include "game2.h"
#include "gyro.h"
#include "step.h"
#include "setting.h"
#include "heart.h"
#include "menu_pic.h"
#include "touch.h"
#include "test code.h"

/* ============================================================================
 * main.c -- Smart Health Monitor firmware (STM32L476RG + ILI9341 240x320 TFT)
 *
 * Structure overview:
 *   1. includes + CubeMX USER CODE 0 : bitmaps, DWT delay helpers
 *   2. globals / state structs        : g_game, g_mpu, g_max30102, menu state
 *   3. page draw functions            : Draw_HomePage / Draw_MenuPage / Draw_FuncPage
 *   4. game loops                     : Game_Loop (dino), Maze_Game_Loop
 *   5. UI helpers                     : UI_Refresh, UI_Slide_Animation, Draw_date
 *   6. main()                         : CubeMX init + main while(1) UI loop
 *
 * NOTE: this file is GBK-encoded; prefer ASCII-only edits when patching.
 * ==========================================================================*/

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
I2C_HandleTypeDef hi2c2;
RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile int8_t g_slide_dir = 0;

const uint8_t Icon_Setting[] = {
    0x00, 0x80, 0x88, 0xfc, 0x38, 0xd8, 0xe8, 0x6e, 0x6e, 0xe8, 0xd8, 0x38, 0xfc, 0x88, 0x80, 0x00,
    0x00, 0x01, 0x11, 0x3f, 0x1c, 0x1b, 0x17, 0x76, 0x76, 0x17, 0x1b, 0x1c, 0x3f, 0x11, 0x01, 0x00};

typedef enum
{
    MENU_SUB_1 = 0, // DHT11
    MENU_SUB_2,     // HEART
    MENU_SUB_3,     // MPU6050
    MENU_SUB_4,     // Spriti Level
    MENU_SUB_5,     // Dano
    MENU_SUB_6,     // Maze ball
    MENU_SUB_7,     // Setting
    MENU_SUB_8      // danger
} MenuSubInterfaceType;

typedef struct
{
    MenuSubInterfaceType menu_current; // current menu sub-page
    MenuSubInterfaceType menu_last;    // previous menu sub-page
    ui_main_state_t ui_state_current;
    ui_main_state_t ui_state_last;
} MenuSubState;

MenuSubState menu_state;

volatile ui_main_state_t g_ui_main_state = UI_STATE_HOME;
volatile ui_func_page_t g_current_func = FUNC_PAGE_DHT;
volatile uint8_t g_menu_selected = 0;
volatile setting_edit_item_t g_set_editing = SET_ITEM_YEAR;
volatile uint8_t g_set_edit_flag = 0;
volatile uint8_t g_ui_refresh = 1;
volatile uint8_t g_func_start_flag = 1;

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

float ball_x = 64.0f, ball_y = 32.0f;        // current ball position
float ball_vx = 0.0f, ball_vy = 0.0f;        // ball velocity
const float friction = 0.85f;                // friction
const float sensitivity = 0.08f;             // sensitivity (larger = faster ball)
int maze_target_x = 115, maze_target_y = 50; // target coordinates

int maze_id = 0;

volatile uint8_t g_button_pressed = 0;

// ============================================================
// Fixed icon data (16x16, column-row order, positive logic)
// ============================================================

// Fixed DHT11 icon (thermometer/droplet style)
const uint8_t Icon_DHT[] = {
    0x00, 0x00, 0xF0, 0x08, 0x04, 0x02, 0x02, 0x42, 0x82, 0x02, 0x02, 0x04, 0x08, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0x30, 0x40, 0x40, 0x40, 0x43, 0x43, 0x40, 0x40, 0x40, 0x30, 0x0F, 0x00, 0x00};

// Fixed Heart icon (16x16, column-row order, adapted for OLED driver)
const uint8_t Icon_Heart[] = {
    0x38, 0x7c, 0xc6, 0x83, 0x33, 0x33, 0x73, 0x5e, 0xce, 0x84, 0x8c, 0x18, 0x30, 0x60, 0xc0, 0x80,
    0x1c, 0x3e, 0x61, 0xc1, 0xc0, 0xc6, 0xc6, 0x7f, 0x7b, 0x21, 0x11, 0x18, 0x0c, 0x06, 0x03, 0x01};

// MPU6050 icon (keep original if it displays correctly)
const uint8_t Icon_Motion[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0xFF, 0xFF, 0xFF, 0xFF, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0xFF, 0xFF, 0xFF, 0xFF, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00};

// Spirit level icon
const uint8_t Icon_Level[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00};

// Game icon
const uint8_t Icon_Game1[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x7C, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80};

// Game icon
const uint8_t Icon_Game2[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x7C, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80};

const menu_item_t g_menu_items[FUNC_PAGE_COUNT] = {
    {Icon_DHT, "DHT11"},
    {Icon_Heart, "HR SpO2"},
    {Icon_Motion, "MPU6050"},
    {Icon_Level, "Spirit Level"},
    {Icon_Game1, "DinoGame1"},
    {Icon_Game1, "DinoGame2"},
    {Icon_Setting, "Settings"}};
/* USER CODE END PV */

/* Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
void Draw_HomePage(void);
void Draw_SettingPage(void);
void Draw_MenuPage(void);
void Draw_FuncPage(void);
void Maze_Game_Loop(void); // added this line
void UI_Refresh(void);
void Setting_Save(void);
void Setting_Inc_Value(void);
void Setting_Dec_Value(void);
void UI_Slide_Animation(int8_t direction);
void Draw_date(void);

/* USER CODE BEGIN 0 */
const uint8_t Dino_Bmp[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x7C, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80};
const uint8_t Cactus_Bmp[] = {
    0x00, 0x00, 0x78, 0xFC, 0xFF, 0xFC, 0x18, 0x00, 0x00, 0x03, 0x0F, 0x3F, 0x3F, 0x3F, 0x03, 0x00};

extern MAX30102_t g_max30102;

void get_dht_show(void)
{
    uint8_t status;
    // Tip: check whether dht.c has a timeout guard for while(HAL_GPIO_ReadPin(...))
    // If not, the program hangs the moment the sensor is unplugged
    status = DHT_ReadData(DHT11);

    if (status == 0)
    {
        humidity = DHT11[0] + DHT11[1] / 10.0f;
        temperature = DHT11[2] + DHT11[3] / 10.0f;
    }
}

//  ===================== Spirit level core parameters (freely tunable)=====================
#define SPIRIT_CX 120      // spirit level center X (screen center)
#define SPIRIT_CY 160      // spirit level center Y (centered in active area)
#define SPIRIT_R_OUT 70    // outer circle radius
#define SPIRIT_BUBBLE_R 12 // bubble radius
#define SCALE 1.2f         // angle-to-pixel scale factor (larger = faster bubble movement)
// global variables: previous frame bubble position (for erasing trails)
static int last_dx = 0, last_dy = 0;

void Draw_Spirit_Level(void)
{
    // 1. Compute attitude angles (unchanged)
    float Roll = atan2(g_mpu.Ay, g_mpu.Az) * RAD_TO_DEG;
    float Pitch = atan2(-g_mpu.Ax, sqrt(g_mpu.Ay * g_mpu.Ay + g_mpu.Az * g_mpu.Az)) * RAD_TO_DEG;

    // 2. Angle -> pixel offset (unchanged)
    int dx = (int)(-Roll * SCALE);
    int dy = (int)(Pitch * SCALE);

    // 3. Clamp bubble range (unchanged)
    const int limit = SPIRIT_R_OUT - SPIRIT_BUBBLE_R - 2;
    float dist = sqrt(dx * dx + dy * dy);
    if (dist > limit)
    {
        dx = (int)(dx * limit / dist);
        dy = (int)(dy * limit / dist);
    }

    // 4. Static UI init (clear screen + draw title only on first entry)
    if (menu_state.menu_current != menu_state.menu_last || g_func_start_flag == 1)
    {
        // clear the active display area
        LCD_Fill(0, 40, 240, 279, WHITE);
        // title
        LCD_ShowString(70, 50, 100, 16, 16, (uint8_t *)"Spirit Level");
        g_func_start_flag = 0;
        last_dx = 0;
        last_dy = 0;
    }

    // ===================== Core fix: never lose lines =====================
    // Step 1: erase previous frame bubble (white)
    gui_fill_circle(SPIRIT_CX + last_dx, SPIRIT_CY + last_dy, SPIRIT_BUBBLE_R + 1, WHITE);
    // Step 2: [redraw outer circle + cross lines every time] -> fully fixes disappearing lines!
    LCD_Draw_Circle(SPIRIT_CX, SPIRIT_CY, SPIRIT_R_OUT);
    LCD_Draw_Circle(SPIRIT_CX, SPIRIT_CY, SPIRIT_R_OUT - 1);
    LCD_DrawLine(SPIRIT_CX - 40, SPIRIT_CY, SPIRIT_CX + 40, SPIRIT_CY);
    LCD_DrawLine(SPIRIT_CX, SPIRIT_CY - 40, SPIRIT_CX, SPIRIT_CY + 40);
    // Step 3: draw current new bubble (black)
    gui_fill_circle(SPIRIT_CX + dx, SPIRIT_CY + dy, SPIRIT_BUBBLE_R, BLACK);

    // update coordinates
    last_dx = dx;
    last_dy = dy;
}

// static variables: previous frame coordinates for erasing trails
static int last_dino_y = 216;
static int last_obs_x = 240;

// game fixed parameters
#define GROUND_Y 256 // ground Y coordinate
#define DINO_H 40    // dino height
#define CACTUS_W 16  // cactus width
#define CACTUS_H 32  // cactus height

void Game_Loop(void)
{
    // =====================================================================
    // Dino game - active ILI9341 (240x320) implementation.
    //
    // Both reported bugs share one root cause: the loop had no fixed time
    // base, so every main-loop pass advanced the physics (too fast at start)
    // and did a full erase + redraw of both sprites (flicker / twitch).
    //
    // Fixes applied here:
    //   1. Fixed 30 ms game tick (HAL_GetTick). Outside the tick only the
    //      button is sampled; physics and rendering run once per tick.
    //   2. Difficulty curve: 4 px/tick base, +1 px/tick per 5 points,
    //      capped at 10 px/tick. Jump vy = -8, gravity = 0.9/tick.
    //   3. Flicker removal: score redrawn only on change; a sprite is
    //      erased + redrawn only when its position changed; the previous
    //      bounding boxes of the moved sprites are merged into one white
    //      LCD_Fill; the ground line is drawn only on page entry.
    // =====================================================================
    static uint32_t s_last_tick = 0;       // tick stamp of the last physics step
    static uint8_t  s_jump_pending = 0;    // button press latched for the next tick
    static int      s_last_score = -1;     // score value currently on the screen
    static uint8_t  s_need_start_draw = 1; // start/game-over text not painted yet
    static uint8_t  s_force_repaint = 0;   // force both sprites to repaint once

    const uint32_t TICK_MS    = 30U;    // game tick period, ms (tunable)
    const float    JUMP_VY    = -8.0f;  // jump impulse, px/tick (tunable)
    const float    GRAVITY    = 0.9f;   // gravity, px/tick^2 (tunable)
    const int      SPEED_BASE = 4;      // initial obstacle speed, px/tick
    const int      SPEED_MAX  = 10;     // obstacle speed cap, px/tick
    const int      SPEED_STEP = 5;      // score points per +1 px/tick

    uint32_t now;
    int speed;
    int dino_y;
    int obs_x;
    int cactus_y;
    int dino_moved;
    int cactus_moved;
    int x1, y1, x2, y2;
    int have;
    int cx1, cy1, cx2, cy2;
    char buf[16];

    now = HAL_GetTick();

    // Page-entry initialization: run once when the game page is entered.
    if (menu_state.menu_current != menu_state.menu_last || g_func_start_flag == 1)
    {
        LCD_Fill(0, 40, 240, 280, WHITE);
        lcd_draw_bline(0, GROUND_Y, 240, GROUND_Y, 4, BLACK);
        g_func_start_flag = 0;
        last_dino_y = 216;
        last_obs_x = 240;
        s_last_tick = now;
        s_jump_pending = 0;
        s_last_score = -1;
        s_need_start_draw = 1;
        s_force_repaint = 1;
    }

    // Sample the button on every call so a press is never missed; it is
    // applied on the next game tick (or used to start the game).
    if (g_button_pressed)
    {
        s_jump_pending = 1;
        g_button_pressed = 0;
    }

    // Start / game-over screen (static; painted once, button only).
    if (g_game.state == 0 || g_game.state == 2)
    {
        if (s_need_start_draw)
        {
            LCD_ShowString(80, 60, 120, 24, 24, (uint8_t *)"DINO JUMP");
            LCD_ShowString(70, 100, 120, 16, 16, (uint8_t *)"- Press Key -");
            if (g_game.state == 2)
            {
                sprintf(buf, "Score: %d", g_game.score);
                LCD_ShowString(80, 140, 120, 24, 24, (uint8_t *)buf);
                LCD_Fill(200, 40, 240, 70, WHITE);
            }
            s_need_start_draw = 0;
        }

        if (s_jump_pending)
        {
            // Clear only the start/game-over text band. The white field and
            // the ground line stay; sprites are repainted on the next tick.
            LCD_Fill(0, 55, 240, 180, WHITE);

            g_game.state = 1;
            g_game.score = 0;
            g_game.obs_x = 240;
            g_game.y = 216;
            g_game.vy = 0;

            s_jump_pending = 0;
            s_last_score = -1;
            s_force_repaint = 1;
            s_last_tick = now;
        }
        return;
    }

    // Playing state: advance only on the fixed 30 ms tick.
    if (now - s_last_tick < TICK_MS)
    {
        return;
    }
    s_last_tick = now;

    // Jump: apply only while the dino is on the ground.
    if (s_jump_pending)
    {
        if (g_game.y >= 216)
        {
            g_game.vy = JUMP_VY;
        }
        s_jump_pending = 0;
    }

    // Vertical physics.
    g_game.y += g_game.vy;
    g_game.vy += GRAVITY;
    if (g_game.y > 216)
    {
        g_game.y = 216;
        g_game.vy = 0;
    }

    // Difficulty curve: base speed, +1 per SPEED_STEP points, capped.
    speed = SPEED_BASE + g_game.score / SPEED_STEP;
    if (speed > SPEED_MAX)
    {
        speed = SPEED_MAX;
    }
    g_game.obs_x -= speed;

    // Recycle the obstacle after it fully leaves the left edge.
    if (g_game.obs_x < -CACTUS_W)
    {
        g_game.obs_x = 240;
        g_game.score++;
    }

    // Collision between the dino box and the cactus box.
    if ((g_game.obs_x < 56 && g_game.obs_x > 20) && (g_game.y > 200))
    {
        g_game.state = 2;
        s_need_start_draw = 1;
    }

    // Rendering: repaint a sprite only when its position changed (or when a
    // full repaint is forced after a clear).
    dino_y = (int)g_game.y;
    obs_x = g_game.obs_x;
    cactus_y = 224;
    dino_moved = (dino_y != last_dino_y) || s_force_repaint;
    cactus_moved = (obs_x != last_obs_x) || s_force_repaint;

    if (dino_moved || cactus_moved)
    {
        // Union of the previous bounding boxes of the moved sprites.
        x1 = 0;
        y1 = 0;
        x2 = 0;
        y2 = 0;
        have = 0;

        if (dino_moved)
        {
            x1 = 16;
            y1 = last_dino_y;
            x2 = 16 + 39;
            y2 = last_dino_y + 39;
            have = 1;
        }
        if (cactus_moved)
        {
            cx1 = last_obs_x;
            cy1 = cactus_y;
            cx2 = last_obs_x + CACTUS_W - 1;
            cy2 = cactus_y + CACTUS_H - 1;
            if (!have)
            {
                x1 = cx1;
                y1 = cy1;
                x2 = cx2;
                y2 = cy2;
                have = 1;
            }
            else
            {
                if (cx1 < x1) x1 = cx1;
                if (cy1 < y1) y1 = cy1;
                if (cx2 > x2) x2 = cx2;
                if (cy2 > y2) y2 = cy2;
            }
        }

        // Clip to the visible screen.
        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > 239) x2 = 239;
        if (y2 > 279) y2 = 279;

        if (x1 <= x2 && y1 <= y2)
        {
            LCD_Fill(x1, y1, x2, y2, WHITE);
        }

        if (dino_moved)
        {
            Gui_Drawpic(16, dino_y, 40, 40, gImage_dino);
            last_dino_y = dino_y;
        }
        if (cactus_moved)
        {
            Gui_Drawpic(obs_x, cactus_y, CACTUS_W, CACTUS_H, gImage_flower);
            last_obs_x = obs_x;
        }
    }
    s_force_repaint = 0;

    // Score: repaint only when the value changed.
    if (g_game.score != s_last_score)
    {
        sprintf(buf, "%d", g_game.score);
        LCD_Fill(200, 40, 240, 70, WHITE);
        LCD_ShowString(200, 44, 40, 24, 24, (uint8_t *)buf);
        s_last_score = g_game.score;
    }

    /* ====================================================================
       PATCH NOTES
       --------------------------------------------------------------------
       Change list:
       1. Added a fixed 30 ms game tick (HAL_GetTick). Between ticks only
          the button is sampled; physics and rendering run once per tick.
       2. Difficulty curve: SPEED_BASE=4 px/tick, +1 px/tick every
          SPEED_STEP=5 points, capped at SPEED_MAX=10 px/tick.
       3. Jump impulse JUMP_VY=-8 px/tick and gravity GRAVITY=0.9 px/tick^2.
       4. Anti-flicker: score repainted only on change; each sprite is
          erased and redrawn only when its position changed; the previous
          bounding boxes of the moved sprites are merged into one white
          LCD_Fill.
       5. Ground line drawn only on page entry (no longer redrawn on restart).
       6. Start/game-over text is painted once and only the text band is
          cleared on restart, instead of a full-screen fill.

       Tunable parameters (all declared at the top of this function):
       - TICK_MS    : frame period in ms (30 -> ~33 fps). Larger = slower game.
       - JUMP_VY    : jump vertical impulse in px/tick (-8). More negative =
                      higher jump.
       - GRAVITY    : gravity per tick in px/tick^2 (0.9). Larger = shorter
                      air time.
       - SPEED_BASE : initial obstacle speed in px/tick (4).
       - SPEED_STEP : score points needed for each +1 px/tick (5).
       - SPEED_MAX  : obstacle speed cap in px/tick (10).

       Rollback: restore the previous content of the #else branch of
       Game_Loop() in Core/Src/main.c (the block between "#else" and "#endif"
       of that function). The #if 0 OLED branch and the function signature
       were not modified.
       ==================================================================== */

}

void Draw_MenuPage(void)
{

    if (menu_state.ui_state_current != menu_state.ui_state_last)
    {
        Pic_top();
        Draw_date();
        LCD_ShowString(88, 40, 64, 16, 16, "MENU");
        // TFT
        Gui_Drawpic(40, 56, 32, 32, gImage_dht11);
        LCD_ShowString(80, 64, 120, 24, 24, "DHT11");

        Gui_Drawpic(40, 88, 32, 32, gImage_Heart);
        LCD_ShowString(80, 96, 168, 24, 24, "HR Sp02");

        Gui_Drawpic(40, 120, 32, 32, gImage_mpu6050);
        LCD_ShowString(80, 128, 168, 24, 24, "MPU6050");

        Gui_Drawpic(40, 152, 32, 32, gImage_SpiritLevel);
        LCD_ShowString(80, 160, 216, 24, 24, "Spirit Level");

        Gui_Drawpic(40, 184, 32, 32, gImage_Dino);
        LCD_ShowString(80, 192, 168, 24, 24, "Dino Game");

        Gui_Drawpic(40, 216, 32, 32, gImage_maze);
        LCD_ShowString(80, 224, 168, 24, 24, "Maze Ball");

        Gui_Drawpic(40, 248, 32, 32, gImage_Set);
        LCD_ShowString(80, 256, 168, 24, 24, "Setting");

        gui_fill_circle(20, 72, 12, WHITE);
        gui_fill_circle(20, 104, 12, WHITE);
        gui_fill_circle(20, 136, 12, WHITE);
        gui_fill_circle(20, 168, 12, WHITE);
        gui_fill_circle(20, 202, 12, WHITE);
        gui_fill_circle(20, 232, 12, WHITE);
        gui_fill_circle(20, 264, 12, WHITE);

        switch (menu_state.menu_current)
        {
        case MENU_SUB_1:
            gui_fill_circle(20, 72, 12, GREEN);
            break;

        case MENU_SUB_2:
            gui_fill_circle(20, 104, 12, GREEN);
            break;

        case MENU_SUB_3:
            gui_fill_circle(20, 136, 12, GREEN);
            break;

        case MENU_SUB_4:
            gui_fill_circle(20, 168, 12, GREEN);
            break;

        case MENU_SUB_5:
            gui_fill_circle(20, 202, 12, GREEN);
            break;

        case MENU_SUB_6:
            gui_fill_circle(20, 232, 12, GREEN);
            break;

        case MENU_SUB_7:
            gui_fill_circle(20, 264, 12, GREEN);
            break;
        }
        menu_state.ui_state_last = menu_state.ui_state_current;
    }
    Draw_date();
    menu_state.menu_current = g_menu_selected;
    if (menu_state.menu_current != menu_state.menu_last)
    {
        switch (menu_state.menu_last)
        {
        case MENU_SUB_1:
            gui_fill_circle(20, 72, 12, WHITE);
            break;
        case MENU_SUB_2:
            gui_fill_circle(20, 104, 12, WHITE);
            break;

        case MENU_SUB_3:
            gui_fill_circle(20, 136, 12, WHITE);
            break;

        case MENU_SUB_4:
            gui_fill_circle(20, 168, 12, WHITE);
            break;

        case MENU_SUB_5:
            gui_fill_circle(20, 202, 12, WHITE);
            break;

        case MENU_SUB_6:
            gui_fill_circle(20, 232, 12, WHITE);
            break;

        case MENU_SUB_7:
            gui_fill_circle(20, 264, 12, WHITE);
            break;
        }

        switch (menu_state.menu_current)
        {
        case MENU_SUB_1:
            gui_fill_circle(20, 72, 12, GREEN);
            break;

        case MENU_SUB_2:
            gui_fill_circle(20, 104, 12, GREEN);
            break;

        case MENU_SUB_3:
            gui_fill_circle(20, 136, 12, GREEN);
            break;

        case MENU_SUB_4:
            gui_fill_circle(20, 168, 12, GREEN);
            break;

        case MENU_SUB_5:
            gui_fill_circle(20, 202, 12, GREEN);
            break;

        case MENU_SUB_6:
            gui_fill_circle(20, 232, 12, GREEN);
            break;

        case MENU_SUB_7:
            gui_fill_circle(20, 264, 12, GREEN);
            break;
        }
    }
    menu_state.menu_last = g_menu_selected;
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
    // static variable: previous edit item index (for erasing the old underline)
    static uint8_t last_edit_idx = 0;
    // array: X coordinates of the 6 settings [year, month, day, hour, minute, second] (shifted right as required)
    const uint16_t cursor_pos_x[6] = {96, 156, 192, 96, 139, 175};
    // array: Y coordinates unchanged (date 104 / time 144)
    const uint16_t cursor_pos_y[6] = {104, 104, 104, 144, 144, 144};
    uint8_t curr_len = 0; // current underline length
    uint8_t curr_idx = 0;

    if (menu_state.ui_state_current != menu_state.ui_state_last)
    {
        g_func_start_flag = 1;
        LCD_Fill(0, 40, 240, 280, WHITE);
        LCD_ShowString(80, 40, 240, 24, 24, (uint8_t *)"SET TIME");
    }

    // display date and time (original coordinates)
    sprintf(buf, "Date: 20%02d-%02d-%02d", sDate_edt.Year, sDate_edt.Month, sDate_edt.Date);
    LCD_ShowString(20, 80, 240, 24, 24, (uint8_t *)buf);
    sprintf(buf, "Time:  %02d:%02d:%02d", sTime_edt.Hours, sTime_edt.Minutes, sTime_edt.Seconds);
    LCD_ShowString(20, 120, 240, 24, 24, (uint8_t *)buf);

    // convert edit item to index
    switch (g_set_editing)
    {
    case SET_ITEM_YEAR:
        curr_idx = 0;
        break;
    case SET_ITEM_MONTH:
        curr_idx = 1;
        break;
    case SET_ITEM_DAY:
        curr_idx = 2;
        break;
    case SET_ITEM_HOUR:
        curr_idx = 3;
        break;
    case SET_ITEM_MIN:
        curr_idx = 4;
        break;
    case SET_ITEM_SEC:
        curr_idx = 5;
        break;
    default:
        curr_idx = 0;
        break;
    }

    // ===================== Set underline length as required =====================
    // year, hour: original length 32
    if (curr_idx == 0 || curr_idx == 3)
        curr_len = 32;
    // month, day: halved -> 16
    else if (curr_idx == 1 || curr_idx == 2)
        curr_len = 16;
    // minute, second: reduced to 1/3 -> 21
    else
        curr_len = 21;

    // ===================== Erase the [previous] underline (full erase) =====================
    uint8_t last_len;
    if (last_edit_idx == 0 || last_edit_idx == 3)
        last_len = 32;
    else if (last_edit_idx == 1 || last_edit_idx == 2)
        last_len = 16;
    else
        last_len = 21;
    LCD_DrawWhiteLine(cursor_pos_x[last_edit_idx], cursor_pos_y[last_edit_idx],
                      cursor_pos_x[last_edit_idx] + last_len, cursor_pos_y[last_edit_idx]);
    LCD_DrawWhiteLine(cursor_pos_x[last_edit_idx], cursor_pos_y[last_edit_idx] + 1,
                      cursor_pos_x[last_edit_idx] + last_len, cursor_pos_y[last_edit_idx] + 1);

    // ===================== Draw the [current] underline =====================
    if (g_set_edit_flag)
    {
        LCD_DrawLine(cursor_pos_x[curr_idx], cursor_pos_y[curr_idx],
                     cursor_pos_x[curr_idx] + curr_len, cursor_pos_y[curr_idx]);
        LCD_DrawLine(cursor_pos_x[curr_idx], cursor_pos_y[curr_idx] + 1,
                     cursor_pos_x[curr_idx] + curr_len, cursor_pos_y[curr_idx] + 1);
    }
    else
    {
        LCD_DrawLine(cursor_pos_x[curr_idx], cursor_pos_y[curr_idx],
                     cursor_pos_x[curr_idx] + curr_len, cursor_pos_y[curr_idx]);
    }

    // update previous index
    last_edit_idx = curr_idx;

    // bottom hint
    LCD_ShowString(20, 170, 240, 24, 24, (uint8_t *)(g_set_edit_flag ? "UP/DN:Adj OK:Save" : "OK:Edit B1:Back  "));
}

void Draw_HomePage(void)
{
    char buf[32];
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    sprintf(buf, "20%02d/%02d/%02d", sDate.Year, sDate.Month, sDate.Date);
    LCD_ShowString(0, 8, 200, 24, 24, buf);
    // OLED_ShowString(0, 0, buf, OLED_6X8);
    sprintf(buf, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    LCD_ShowString(140, 8, 200, 24, 24, buf);
    // OLED_ShowString(32, 25, buf, OLED_8X16);
    // OLED_DrawLine(0, 52, 127, 52);
    // OLED_ShowString(20, 55, "Press ENTER", OLED_6X8);
    LCD_ShowString(0, 148, 240, 16, 16, "Press any Button to start");
}

void Draw_date(void)
{
    char buf[32];
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    sprintf(buf, "20%02d/%02d/%02d", sDate.Year, sDate.Month, sDate.Date);
    LCD_ShowString(0, 8, 200, 24, 24, buf);
    sprintf(buf, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    LCD_ShowString(140, 8, 200, 24, 24, buf);
}

void UI_Slide_Animation(int8_t direction)
{
    static uint8_t old_buf[1024];
    static uint8_t new_buf[1024];
    extern uint8_t OLED_DisplayBuf[1024];
    // memcpy(old_buf, OLED_DisplayBuf, 1024);
    //  OLED_Clear();
    Draw_FuncPage();
    /*memcpy(new_buf, OLED_DisplayBuf, 1024);
    for (int step = 1; step <= 8; step++)
    {
        int offset = step * 16 * direction;
        // OLED_Clear();
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
        // OLED_Update();
    }*/
}

void Draw_FuncPage(void)
{
    char buf[32];
    static uint32_t t_finger_start = 0;
    menu_state.menu_current = g_current_func;
    switch (g_current_func)
    {
    case FUNC_PAGE_DHT:
        // TFT
        if (menu_state.menu_current != menu_state.menu_last || g_func_start_flag == 1)
        {
            Gui_Drawpic(0, 40, 240, 240, gImage_temp);
            g_func_start_flag = 0;
        }

        // OLED_ShowString(0, 0, "DHT11 Sensor", OLED_8X16);
        sprintf(buf, "Temp: %5.1f C", temperature);
        // OLED_ShowString(0, 24, buf, OLED_8X16);
        LCD_ShowString(120, 216, 120, 16, 16, buf);
        sprintf(buf, "Humi: %5.1f %%", humidity);
        // OLED_ShowString(0, 44, buf, OLED_8X16);
        LCD_ShowString(120, 248, 120, 16, 16, buf);
        break;
        case FUNC_PAGE_HEART:
    if (menu_state.menu_current != menu_state.menu_last || g_func_start_flag == 1)
    {
        Gui_Drawpic(0, 40, 240, 240, gImage_heart);
        g_func_start_flag = 0;
    }
    // First check whether the sensor initialized successfully
    if (!max30102_init_ok)
    {
        LCD_ShowString(0, 44, 120, 12, 12, "Sensor Error  ");
        LCD_ShowString(0, 64, 120, 12, 12, "MAX30102 Init Fail");
        break;
    }
    
    // [New] debug: show raw IR value to confirm it rises significantly when a finger is placed
    char dbg_buf[32];
    sprintf(dbg_buf, "IR:%6lu R:%.2f", g_max30102.ir, g_max30102.ratio);
    LCD_ShowString(0, 80, 120, 12, 12, dbg_buf);

    // Finger detection: threshold matches max30102.c (30000)
    if (g_max30102.ir < 30000)
    {
        t_finger_start = 0;
        LCD_ShowString(0, 44, 120, 12, 12, "Finger: None  ");
        LCD_ShowString(0, 64, 120, 12, 12, "Place Finger     ");
        break;
    }
    else
    {
        if (t_finger_start == 0)
            t_finger_start = HAL_GetTick();
        uint32_t elapsed = HAL_GetTick() - t_finger_start;
        char buf[32];
        
        // [Changed] 3 s -> 5 s, gives the algorithm more time to stabilize
        if (elapsed < 5000)
        {
            LCD_ShowString(0, 44, 120, 12, 12, "Stabilizing...");
            sprintf(buf, "Wait: %d s     ", (int)(5 - elapsed / 1000));
            LCD_ShowString(0, 64, 120, 12, 12, buf);
        }
        else
        {
            if (g_max30102.isValid)
            {
                sprintf(buf, "HR:   %3d bpm ", (int)g_max30102.heartRate);
                LCD_ShowString(0, 44, 120, 12, 12, buf);
                sprintf(buf, "SpO2: %3d %%      ", (int)g_max30102.spO2);
                LCD_ShowString(0, 64, 120, 12, 12, buf);
            }
            else
            {
                LCD_ShowString(0, 44, 120, 12, 12, "Measuring...  ");
                LCD_ShowString(0, 64, 120, 12, 12, "Detecting Heartbeat");
            }
        }
    }
    break;
    case FUNC_PAGE_MOTION:
        if (menu_state.menu_current != menu_state.menu_last || g_func_start_flag == 1)
        {
            Gui_Drawpic(0, 40, 240, 240, gImage_step);
            LCD_ShowString(0, 44, 120, 12, 12, "MPU6050 Accel");
            g_func_start_flag = 0;
        }

        // OLED_ShowString(0, 0, "MPU6050 Accel", OLED_8X16);
        sprintf(buf, "Accel X: % 6.2f", g_mpu.Ax);
        // OLED_ShowString(0, 20, buf, OLED_6X8);
        LCD_ShowString(0, 56, 120, 12, 12, buf);
        sprintf(buf, "Accel Y: % 6.2f", g_mpu.Ay);
        // OLED_ShowString(0, 32, buf, OLED_6X8);
        LCD_ShowString(0, 68, 120, 12, 12, buf);
        sprintf(buf, "Accel Z: % 6.2f", g_mpu.Az);
        // OLED_ShowString(0, 44, buf, OLED_6X8);
        LCD_ShowString(0, 80, 120, 12, 12, buf);
        sprintf(buf, "Step   : %6d", g_mpu.step_count);
        LCD_ShowString(0, 92, 120, 12, 12, buf);

        break;
    case FUNC_PAGE_LEVEL:
        Draw_Spirit_Level();
        break;
    case FUNC_PAGE_GAME1:
        Game_Loop();
        // TFT
        break;

    case FUNC_PAGE_GAME2:
        Maze_Game_Loop();
        // TFT
        break;

    default:
        break;
    }
    menu_state.menu_last = menu_state.menu_current;
}

void UI_Refresh(void)
{
    // OLED_Clear();
    menu_state.ui_state_current = g_ui_main_state;
    switch (g_ui_main_state)
    {
    case UI_STATE_HOME: // home page
        if (menu_state.ui_state_current != menu_state.ui_state_last)
        {
            g_func_start_flag = 1;
            Pic_top();
            Draw_HomePage();
        }
        menu_state.ui_state_last = menu_state.ui_state_current;
        break;
    case UI_STATE_SETTING: // settings
        Draw_SettingPage();
        Draw_date();
        menu_state.ui_state_last = menu_state.ui_state_current;
        break;
    case UI_STATE_MENU: // menu
        Draw_MenuPage();
        Draw_date();
        break;
    case UI_STATE_FUNC: // function
        if (menu_state.ui_state_current != menu_state.ui_state_last)
        {
            g_func_start_flag = 1;
        }
        Draw_FuncPage();
        menu_state.ui_state_last = menu_state.ui_state_current;
        Draw_date();
        break;
    }
    // OLED_Update();

    // Modified: clear the flag only when no animation request (2) is pending
    if (g_ui_refresh == 1)
    {
        g_ui_refresh = 0;
    }
}

void Update_MPU_Angles(void)
{
    if (!mpu_init_ok)
        return;
    // compute Roll and Pitch
    Roll = atan2(g_mpu.Ay, g_mpu.Az) * RAD_TO_DEG;
    Pitch = atan2(-g_mpu.Ax, sqrt(g_mpu.Ay * g_mpu.Ay + g_mpu.Az * g_mpu.Az)) * RAD_TO_DEG;
}

// ===================== Core config =====================
// LCD active display area (diagonal coordinates, fully enclosed maze region)
#define MAZE_X1 0
#define MAZE_Y1 40
#define MAZE_X2 240
#define MAZE_Y2 280
// OLED (128x64) -> LCD (240x240) scale factors
#define SCALE_X 1.875f
#define SCALE_Y 3.75f
#define BALL_R 8            // ball radius
const float bounce = 0.40f; // bounce coefficient

// helper to draw the current map walls (for readability)
void Draw_Maze_Walls(int id)
{
    /*switch (id)
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
    }*/
}

void Draw_Maze(uint8_t id)
{
    uint16_t lx, ly, rx, dy;
    // draw the enclosing 240x240 wall in the middle
    LCD_DrawRectangle(0, 40, 240, 280);

    // draw inner walls (direct values, no new macros)
#define WALL(x, y, w, h)    \
    lx = 0 + (x * 1.875f);  \
    ly = 40 + (y * 3.75f);  \
    rx = lx + (w * 1.875f); \
    dy = ly + (h * 3.75f);  \
    LCD_Fill(lx, ly, rx, dy, BLACK);

    switch (id)
    {
    case 0:
        WALL(60, 0, 6, 45);
        break;
    case 1:
        WALL(40, 0, 6, 40);
        WALL(85, 24, 6, 40);
        break;
    case 2:
        WALL(30, 20, 70, 6);
        WALL(30, 26, 6, 38);
        break;
    case 3:
        WALL(30, 30, 68, 6);
        WALL(61, 0, 6, 30);
        break;
    case 4:
        WALL(0, 32, 40, 6);
        WALL(80, 32, 48, 6);
        WALL(60, 10, 8, 30);
        break;
    }
#undef WALL
}

// wall collision detection
#define W_COL(x, y, w, h)                                             \
    if (nx + r > x && nx - r < x + w && ny + r > y && ny - r < y + h) \
    {                                                                 \
        if (ball_x + r <= x || ball_x - r >= x + w)                   \
        {                                                             \
            nx = ball_x;                                              \
            ball_vx *= -bounce;                                       \
        }                                                             \
        if (ball_y + r <= y || ball_y - r >= y + h)                   \
        {                                                             \
            ny = ball_y;                                              \
            ball_vy *= -bounce;                                       \
        }                                                             \
    }

void Maze_Game_Loop(void)
{
    static uint8_t first_play = 1;
    static uint8_t game_state = 0; // 0:init 1:playing 2:win
    static uint16_t last_bal_x = 0;
    static uint16_t last_bal_y = 0;
    float nx, ny;
    // reset game on button press
    if (g_button_pressed)
    {
        g_button_pressed = 0;
        maze_id = 0;
        game_state = 0;
        first_play = 1;
    }

    // init level: set start and target per level
    if (game_state == 0)
    {
        ball_vx = 0;
        ball_vy = 0;
        // last level (4): start at [top-left], target at [top-right]
        if (maze_id == 4)
        {
            ball_x = 16.0f; // top-left
            ball_y = 10.0f;
            maze_target_x = 100.0f; // top-right
            maze_target_y = 10.0f;
        }
        // normal level: start on the left, target on the right
        else
        {
            ball_x = 16.0f;
            ball_y = 32.0f;
            maze_target_x = 110.0f;
            maze_target_y = 32.0f;
        }
        game_state = 1;
    }

    // win screen
    if (game_state == 2 && first_play == 1)
    {
        LCD_Fill(0, 40, 240, 280, WHITE);
        LCD_ShowString(70, 150, 100, 24, 24, (uint8_t *)"YOU WIN!");
        LCD_ShowString(40, 190, 160, 16, 16, (uint8_t *)"Press Key To Restart");
        first_play = 0;
        return;
    }

    // main game logic
    if (game_state == 1)
    {
        // gravity physics
        ball_vx = (ball_vx + (fabs(Roll) > 2.0f ? Roll * 0.08f : 0)) * 0.85f;
        ball_vy = (ball_vy + (fabs(Pitch) > 2.0f ? Pitch * 0.08f : 0)) * 0.85f;

        // sub-stepped movement to prevent tunneling
        const int steps = 3;
        float svx = ball_vx / steps;
        float svy = ball_vy / steps;
        float r = 8 / 1.875f;

        for (int i = 0; i < steps; i++)
        {
            nx = ball_x + svx;
            ny = ball_y + svy;

            // hard boundary clamp: ball never leaves the 240x240 game area
            if (nx < r)
            {
                nx = r;
                ball_vx = 0;
            }
            if (nx > 127 - r)
            {
                nx = 127 - r;
                ball_vx = 0;
            }
            if (ny < r)
            {
                ny = r;
                ball_vy = 0;
            }
            if (ny > 63 - r)
            {
                ny = 63 - r;
                ball_vy = 0;
            }

            // level wall collision
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

        if (first_play == 1)
        { // clear the central 240x240 game area
            LCD_Fill(0, 40, 240, 280, WHITE);
            // draw the target (right side) -> corrected coordinates, centered without offset
            uint16_t tar_x = 0 + maze_target_x * 1.875f;
            uint16_t tar_y = 40 + maze_target_y * 3.75f;
            // draw enclosing wall + inner walls
            Draw_Maze(maze_id);
            LCD_DrawRectangle(tar_x - 4, tar_y - 4, tar_x + 16, tar_y + 16);
            // draw the "E" centered
            LCD_ShowString(tar_x + 2, tar_y - 2, 16, 16, 16, (uint8_t *)"E");
            last_bal_x = 0 + ball_x * 1.875f;
            last_bal_y = 40 + ball_y * 3.75f;
            first_play = 0;
        }
        // draw the solid ball
        uint16_t bal_x = 0 + ball_x * 1.875f;
        uint16_t bal_y = 40 + ball_y * 3.75f;

        for (int i = 8; i > 0; i--)
            gui_fill_circle(last_bal_x, last_bal_y, i, WHITE);
        for (int i = 8; i > 0; i--)
            gui_fill_circle(bal_x, bal_y, i, BLACK);

        // 3. update previous frame coordinates
        last_bal_x = bal_x;
        last_bal_y = bal_y;

        // win detection
        // if (abs((int)ball_x - maze_target_x) < 8 && abs((int)ball_y - maze_target_y) < 8)
        // old: <8   new: <3  tightly confined inside E's small rectangle, without exceeding the border
        if (abs((int)ball_x - maze_target_x) < 3 && abs((int)ball_y - maze_target_y) < 3)
        {
            if (maze_id < 4)
            {
                LCD_ShowString(70, 140, 100, 24, 24, (uint8_t *)"LEVEL UP!");
                HAL_Delay(800);
                maze_id++;
                game_state = 0;
                first_play = 1;
            }
            else
            {
                game_state = 2;
                first_play = 1;
            }
        }
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

int main(void)
{
    uint8_t bt_cnt = 0;
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    // MX_SPI3_Init();
    MX_I2C2_Init();
    MX_USART1_UART_Init(); // USART1 init (generated by CubeMX)
    MX_RTC_Init();
    // MX_DMA_Init();           if CubeMX generated a DMA init, it must be called!

    // OLED_Init();
    DWT_Delay_Init();

    // ========== New: init Bluetooth module ==========
    BT_Init(&huart1); // pass USART1 handle (HC-05 is on USART1)
    MPU6050_StepCounter_Init(&g_mpu);
    if (MAX30102_Init(&hi2c2) == 0)
        max30102_init_ok = 1;
    if (MPU6050_Init(&hi2c2) == 0)
        mpu_init_ok = 1;

    HAL_RTC_GetTime(&hrtc, &sTime_edt, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate_edt, RTC_FORMAT_BIN);

    LCD_Init();                      // init LCD
    tp_dev.touchtype |= USE_TP_TYPE; // capacitive & resistive touch
    LCD_Display_Dir(USE_LCM_DIR);    // screen orientation
    tp_dev.init();                   // touchscreen init
    // POINT_COLOR = RED;               // set font color to red
    // Load_Drow_Dialog();
    //  rtp_test(); // resistive touchscreen test
    Pic_top();
    Draw_HomePage();
    // comment out the code below if touch is not needed
    // LCD_Display_Dir(0); // switch screen to portrait

    menu_state.ui_state_current = UI_STATE_HOME;
    menu_state.ui_state_last = UI_STATE_HOME;
    menu_state.menu_current = MENU_SUB_1;
    menu_state.menu_last = MENU_SUB_1;
    while (1)
{
    uint32_t now = HAL_GetTick();
    static uint32_t t_max = 0;
    // [Key fix] as long as the sensor initialized successfully, keep reading FIFO every 8 ms globally, on any page
    if (max30102_init_ok && (now - t_max >= 8))
    {
        MAX30102_ReadFIFO();
        t_max = now;
    }

        if (menu_state.ui_state_current == UI_STATE_HOME)
        {
            Draw_date();
        }

        static uint32_t t_dht = 0;
        if (g_current_func == FUNC_PAGE_DHT)
        {
            if (now - t_dht >= 2000)
            {
                get_dht_show();
                t_dht = now;
            }
        }

        // ==========================================
        // Fixed Bluetooth sending logic
        // ==========================================
        static uint32_t t_bt_send = 0;
if (now - t_bt_send >= 500)
{
    // 1. Clear all UART error flags completely to fix the UART hang / no send issue
    uint32_t isr_flags = huart1.Instance->ISR;
    if (isr_flags & (UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE))
    {
        __HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
    }

    char bt_tx_buf[128] = {0};
    int h = (int)humidity;
    int t = (int)temperature;

    // 2. Data validity check: send real values only for valid data, keep the last result when invalid, never send 0
    int send_hr = (g_max30102.isValid && g_max30102.heartRate > 40 && g_max30102.heartRate < 180) ? (int)g_max30102.heartRate : 0;
    int send_spo2 = (g_max30102.isValid && g_max30102.spO2 > 80 && g_max30102.spO2 <= 100) ? (int)g_max30102.spO2 : 0;
    int finger_detected = (g_max30102.ir > 30000) ? 1 : 0; // keep consistent with sensor threshold

    // 3. Build the Bluetooth packet, format fully compatible with the host
    sprintf(bt_tx_buf, "HR:%d,SPO2:%d,T:%d,H:%d,W:%d,GS:%d,P:%d,R:%d,S:%ld\r\n",
            send_hr,
            send_spo2,
            t, h,
            finger_detected,
            g_game.score,
            (int)Pitch, (int)Roll,
            g_mpu.step_count);

    // 4. Fixed send logic: send every 500 ms, remove the 5 s limit, add a UART ready check to prevent hang
    if (HAL_UART_GetState(&huart1) == HAL_UART_STATE_READY)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)bt_tx_buf, strlen(bt_tx_buf), 100);
    }

    t_bt_send = now;
}

        static uint32_t t_ui = 0;
        uint32_t refresh_rate = 100;

        if (g_ui_main_state == UI_STATE_FUNC)
        {
            if (g_current_func == FUNC_PAGE_LEVEL)
                refresh_rate = 30;
            else if (g_current_func == FUNC_PAGE_GAME1)
                refresh_rate = 0;
            else if (g_current_func == FUNC_PAGE_GAME2)
                refresh_rate = 0;
        }

        // ==================== Modified code ====================
        if (g_ui_refresh || (now - t_ui >= refresh_rate))
        {
            if (g_ui_refresh == 2)
            {
                UI_Slide_Animation(g_slide_dir);
                g_ui_refresh = 0;
            }
            else
            {
                // read and compute globally regardless of page state
                MPU6050_Read_All(&g_mpu);
                Roll = -atan2(g_mpu.Ay, g_mpu.Az) * RAD_TO_DEG;
                Pitch = atan2(-g_mpu.Ax, sqrt(g_mpu.Ay * g_mpu.Ay + g_mpu.Az * g_mpu.Az)) * RAD_TO_DEG;

                if (true == MPU6050_Detect_Fall(&g_mpu) && g_current_func != FUNC_PAGE_GAME2)
                {
                    // 1. Trigger alarm immediately: show image + turn on buzzer (PB8 output high)
                    Gui_Drawpic(0, 40, 240, 240, gImage_danger);
                    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET); // <--- changed to PC5 
                    
                    uint32_t press_start_tick = 0;
                    uint8_t is_pressing = 0;

                    // 2. Enter alarm loop until PC13 is held for 2 s
                    while (1)
                    {
                        Draw_date(); // keep the top-of-screen time display updated

                        // detect PC13 button (per STM32 behavior, usually pressed = RESET/low level)
                        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) 
                        {
                            if (!is_pressing) 
                            {
                                is_pressing = 1;
                                press_start_tick = HAL_GetTick(); // record press start time
                            }
                            else if (HAL_GetTick() - press_start_tick >= 2000)  
                            {
                                // 2 s reached, run exit logic
                                
                                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET); 
                                
                                g_ui_refresh = 1; // mark UI to refresh back to main screen
                                break; // break out of the alarm loop
                            }
                        }
                        else 
                        {
                            is_pressing = 0; // button released, reset timing state
                        }
                        
                        HAL_Delay(10); // short delay to avoid jitter from scanning the key too fast
                    }
                   
                }

                UI_Refresh();
                if (g_ui_refresh == 1)
                    g_ui_refresh = 0;
            }
            t_ui = now;
        }

        static uint32_t t_touch = 0;
        if (now - t_touch >= 50) // 20 Hz detection rate
        {
            tp_dev.scan(0);                 // scan touchscreen
            if (tp_dev.sta && TP_PRES_DOWN) // touch detected
            {
                HAL_Delay(100);
                tp_dev.sta = 0;
                uint16_t x = tp_dev.x[0];
                uint16_t y = tp_dev.y[0];
                // home page, any key
                if (g_ui_main_state == UI_STATE_HOME)
                {
                    g_ui_main_state = UI_STATE_MENU;
                }
                if (x >= UP_X_START && x <= UP_X_END && y >= UP_Y_START && y <= UP_Y_END)
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
                        // Core fix: function page cycle logic (up)
                        g_slide_dir = -1;
                        ui_func_page_t next_p = (ui_func_page_t)((g_current_func - 1 + FUNC_PAGE_COUNT) % FUNC_PAGE_COUNT);
                        // auto skip the settings page (index 5)
                        if (next_p == FUNC_PAGE_SETTING)
                        {
                            next_p = (ui_func_page_t)((next_p - 1 + FUNC_PAGE_COUNT) % FUNC_PAGE_COUNT);
                        }
                        g_current_func = next_p;
                        g_ui_refresh = 2; // explicitly trigger the smooth animation
                    }
                }
                else if (x >= DOWN_X_START && x <= DOWN_X_END && y >= DOWN_Y_START && y <= DOWN_Y_END)
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
                        // Core fix: function page cycle logic (down)
                        g_slide_dir = 1;
                        ui_func_page_t next_p = (ui_func_page_t)((g_current_func + 1) % FUNC_PAGE_COUNT);
                        // auto skip the settings page (index 5)
                        if (next_p == FUNC_PAGE_SETTING)
                        {
                            next_p = (ui_func_page_t)((next_p + 1) % FUNC_PAGE_COUNT);
                        }
                        g_current_func = next_p;
                        g_ui_refresh = 2; // explicitly trigger the smooth animation
                    }
                }
                else if (x >= CTRL_X_START && x <= CTRL_X_END && y >= CTRL_Y_START && y <= CTRL_Y_END)
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
                        if (g_current_func == FUNC_PAGE_GAME1)
                        {
                            g_button_pressed = 1; // dino jump
                        }
                        else
                        {
                            g_ui_main_state = UI_STATE_MENU; // back to menu
                        }
                    }
                    else if (g_ui_main_state == UI_STATE_FUNC)
                    {
                        if (g_current_func == FUNC_PAGE_GAME2)
                        {
                        }
                        else
                        {
                            g_ui_main_state = UI_STATE_MENU; // back to menu
                        }
                    }
                    g_ui_refresh = 1; // OK key usually uses a normal refresh
                }
            }
            t_touch = now;
        }
        BT_Task_Loop();
    }
}

/**
 * @brief System Clock Configuration
 */
void SystemClock_Config(void)
{
#if 1
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
#else
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 40;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
#endif
}

static void MX_I2C2_Init(void)
{
#if 1
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x10D19CE4;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c2);
#else
    /* USER CODE BEGIN I2C2_Init 0 */

    /* USER CODE END I2C2_Init 0 */

    /* USER CODE BEGIN I2C2_Init 1 */

    /* USER CODE END I2C2_Init 1 */
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x10D19CE4;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
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

    /** I2C Fast mode Plus enable
     */
    HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C2);
    /* USER CODE BEGIN I2C2_Init 2 */

    /* USER CODE END I2C2_Init 2 */

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
        sDate.Month = RTC_MONTH_FEBRUARY;
        sDate.Date = 13;
        sDate.Year = 26;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x32F2);
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&huart1);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, LCD_DC_Pin | LCD_RST_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOB, DHT11_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : PC14 */
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_11 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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

    GPIO_InitStruct.Pin = DHT11_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : LCD_DC_Pin LCD_RST_Pin */
    GPIO_InitStruct.Pin = LCD_DC_Pin | LCD_RST_Pin | LCD_MOSI_Pin | LCD_MISO_Pin | LCD_SCK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

    // basic debounce
    if (now - last_press_time < KEY_DELAY_MS)
        return;
    last_press_time = now;

    // --- UP key (GPIO_PIN_0) ---
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
            // Core fix: function page cycle logic (up)
            g_slide_dir = -1;
            ui_func_page_t next_p = (ui_func_page_t)((g_current_func - 1 + FUNC_PAGE_COUNT) % FUNC_PAGE_COUNT);
            // auto skip the settings page (index 5)
            if (next_p == FUNC_PAGE_SETTING)
            {
                next_p = (ui_func_page_t)((next_p - 1 + FUNC_PAGE_COUNT) % FUNC_PAGE_COUNT);
            }
            g_current_func = next_p;
            g_ui_refresh = 2; // explicitly trigger the smooth animation
        }
    }

    // --- DOWN key (GPIO_PIN_4) ---
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
            // Core fix: function page cycle logic (down)
            g_slide_dir = 1;
            ui_func_page_t next_p = (ui_func_page_t)((g_current_func + 1) % FUNC_PAGE_COUNT);
            // auto skip the settings page (index 5)
            if (next_p == FUNC_PAGE_SETTING)
            {
                next_p = (ui_func_page_t)((next_p + 1) % FUNC_PAGE_COUNT);
            }
            g_current_func = next_p;
            g_ui_refresh = 2; // explicitly trigger the smooth animation
        }
    }

    // --- OK key (GPIO_PIN_1) ---
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
            if (g_current_func == FUNC_PAGE_GAME1)
            {
                g_button_pressed = 1; // dino jump
            }
            else
            {
                g_ui_main_state = UI_STATE_MENU; // back to menu
            }
        }
        else if (g_ui_main_state == UI_STATE_FUNC)
        {
            if (g_current_func == FUNC_PAGE_GAME2)
            {
            }
            else
            {
                g_ui_main_state = UI_STATE_MENU; // back to menu
            }
        }
        g_ui_refresh = 1; // OK key usually uses a normal refresh
    }

    // --- BACK key (B1_Pin) ---
    else if (GPIO_Pin == B1_Pin)
    {
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
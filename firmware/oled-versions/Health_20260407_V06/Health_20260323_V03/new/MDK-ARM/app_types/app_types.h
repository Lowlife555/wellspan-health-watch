#ifndef __APP_TYPES_H__
#define __APP_TYPES_H__

#include <stdint.h>

// 1. UI 状态枚举
typedef enum {
    UI_STATE_HOME = 0,    
    UI_STATE_SETTING,     
    UI_STATE_MENU,        
    UI_STATE_FUNC         
} ui_main_state_t;

// 2. 功能页枚举 (增加迷宫页面)
typedef enum {
    FUNC_PAGE_DHT = 0,    // 温湿度
    FUNC_PAGE_HEART,      // 心率血氧
    FUNC_PAGE_MOTION,     // 陀螺仪原始数据
    FUNC_PAGE_LEVEL,      // 水平仪
    FUNC_PAGE_DINO,       // 恐龙跳跃游戏 (原 FUNC_PAGE_GAME)
    FUNC_PAGE_MAZE,       // 新增：迷宫小球游戏
    FUNC_PAGE_SETTING,    // 设置页 (在菜单末尾)
    FUNC_PAGE_COUNT       // 自动变为 7
} ui_func_page_t;

// 3. 设置项枚举
typedef enum {
    SET_ITEM_YEAR = 0,    
    SET_ITEM_MONTH,       
    SET_ITEM_DAY,         
    SET_ITEM_HOUR,        
    SET_ITEM_MIN,         
    SET_ITEM_SEC,         
    SET_ITEM_COUNT        
} setting_edit_item_t;

// 4. 游戏状态结构体
typedef struct {
    float y;       
    float vy;      
    int   score;   
    int   state;   
    int   obs_x;   
    int   obs_h;   
} GameState_t;

// 5. 菜单项结构体
typedef struct {
    const uint8_t* icon;    
    const char* name;    
} menu_item_t;

#endif /* __APP_TYPES_H__ */


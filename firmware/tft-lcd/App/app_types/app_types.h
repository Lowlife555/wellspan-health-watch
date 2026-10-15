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

// 2. 功能页枚举
typedef enum {
    FUNC_PAGE_DHT = 0,    
    FUNC_PAGE_HEART,      
    FUNC_PAGE_MOTION,     
    FUNC_PAGE_LEVEL,      
    FUNC_PAGE_GAME1, 
    FUNC_PAGE_GAME2,     
    FUNC_PAGE_SETTING,    
    FUNC_PAGE_COUNT       
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

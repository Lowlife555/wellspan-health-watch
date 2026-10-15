#ifndef __TEST_CODE_H
#define __TEST_CODE_H
#include "main.h"

void rtp_test(void);             // 电阻触摸屏测试函数
void ctp_test(void);             // 电容触摸屏测试函数
void DrawTestPage(uint8_t *str); // 主固定栏
void Chinese_Font_test(void);    // 中文测试
void main_test(uint8_t *id);     // 测试主页
void FillRec_Test(void);         // 图形测试
void English_Font_test(void);    // 英文测试
void Pic_top(void);              // 图片测试
void Rotate_Test(void);          // 旋转测试
void Color_Test(void);           // 纯色测试
void Switch_test(void);          // 显示开关测试
void Read_Test(void);            // 读ID 颜色 测试

#endif

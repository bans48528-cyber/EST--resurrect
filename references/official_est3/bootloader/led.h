
#ifndef __LED_H
#define __LED_H	

#include "sys.h" 

typedef enum 
{
  LED1 = 0,
  LED2 = 1,
  LED3 = 2,
  LED4 = 3,
	LED5 = 4
} Led_TypeDef;

void LED_Init(uint8_t num);						// 初始化		 
void LED_Off(uint8_t num);						// LED灭
void LED_On(uint8_t num);							// LED亮
void LED_Toggle(uint8_t num);					// LED状态反转
void KEY_Init(void);									// 按键初始化

#endif


















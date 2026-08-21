#include "led.h"

//================================================================
//功能：LED初始化
//参数：Led:编号
//返回：无
//================================================================
void LED_Init(uint8_t num)
{
  GPIO_InitTypeDef  GPIO_InitStructure;
  
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	switch(num)
	{
		case LED1: 
			RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
			GPIO_Init(GPIOD, &GPIO_InitStructure);
		break;
		case LED2: 
			RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE); 
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
			GPIO_Init(GPIOD, &GPIO_InitStructure);
			break;
		case LED3: 
			RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
			GPIO_Init(GPIOF, &GPIO_InitStructure);
			break;
		case LED4: 
			RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE); 
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
			GPIO_Init(GPIOC, &GPIO_InitStructure);
			break;
		case LED5: 
			RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); 
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
			GPIO_Init(GPIOA, &GPIO_InitStructure);
			break;
	}
}
//================================================================

//================================================================
//功能：LED灭
//参数：num:编号
//返回：无
//================================================================
void LED_Off(uint8_t num)
{
	switch(num)
	{
		case LED1: GPIOD->BSRRL = GPIO_Pin_2;  break;
		case LED2: GPIOD->BSRRL = GPIO_Pin_3;  break;
		case LED3: GPIOF->BSRRH = GPIO_Pin_2;  break;
		case LED4: GPIOC->BSRRH = GPIO_Pin_13; break;
		case LED5: GPIOA->BSRRH = GPIO_Pin_8; break;
	}
}
//================================================================

//================================================================
//功能：LED亮
//参数：Led:编号
//返回：无
//================================================================
void LED_On(uint8_t num)
{
	switch(num)
	{
		case LED1: GPIOD->BSRRH = GPIO_Pin_2;  break;
		case LED2: GPIOD->BSRRH = GPIO_Pin_3;  break;
		case LED3: GPIOF->BSRRL = GPIO_Pin_2;  break;
		case LED4: GPIOC->BSRRL = GPIO_Pin_13; break;
		case LED5: GPIOA->BSRRL = GPIO_Pin_8;  break;
	}
}
//================================================================

//================================================================
//功能：LED状态改变
//参数：Led:编号
//返回：无
//================================================================
void LED_Toggle(uint8_t num)
{
	switch(num)
	{
		case LED1: GPIOD->ODR ^= GPIO_Pin_2; break;
		case LED2: GPIOD->ODR ^= GPIO_Pin_3;  break;
		case LED3: GPIOF->ODR ^= GPIO_Pin_2;  break;
		case LED4: GPIOC->ODR ^= GPIO_Pin_13; break;
		case LED5: GPIOA->ODR ^= GPIO_Pin_8; break;
	}
}
//================================================================

//================================================================
//功能：按键初始化
//参数：无
//返回：无
//================================================================
void KEY_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOF, ENABLE);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14|GPIO_Pin_15;								//KEY0 KEY1 KEY4对应引脚
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;													//普通输入模式
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;										//100M
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;													//上拉
  GPIO_Init(GPIOC, &GPIO_InitStructure);																//初始化GPIOC
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_4;									//KEY3 KEY5 对应引脚
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;													//普通输入模式
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;										//100M
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;													//上拉
  GPIO_Init(GPIOE, &GPIO_InitStructure);																//初始化GPIOE
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 |GPIO_Pin_1;									//KEY2 KEY4 对应引脚
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;													//普通输入模式
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;										//100M
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;													//上拉
  GPIO_Init(GPIOF, &GPIO_InitStructure);																//初始化GPIOF
}
//================================================================

#include "timer3.h"
#include "main.h"
#include "C_Protocol.h"

extern USB_OTG_CORE_HANDLE    USB_OTG_dev;

uint8_t 	SysTick_2ms 				= 0;										//时间片时基2ms
uint8_t 	SysTick_10ms 				= 0;										//时间片时基10ms
uint8_t 	SysTick_100ms 			= 0;										//时间片时基300ms
uint8_t 	SysTick_1000ms 			= 0;										//时间片时基1000ms

//通用定时器3中断初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
//这里使用的是定时器3!
void TIM3_Int_Init(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);  					// 使能TIM3时钟
	
  TIM_TimeBaseInitStructure.TIM_Period = arr; 									// 自动重装载值
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  								// 定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;	// 向上计数模式
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);						// 初始化TIM3
	
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE); 											// 允许定时器3更新中断
	TIM_Cmd(TIM3,ENABLE); 																				// 使能定时器3
	
	NVIC_InitStructure.NVIC_IRQChannel=TIM3_IRQn; 								// 定时器3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1; 			// 抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=2; 							// 子优先级1
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
}
//==============================================================

//==============================================================
//功能：定时器3中断服务函数
//参数：无
//返回: 无
//==============================================================
void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3,TIM_IT_Update)==SET) 									// 溢出中断
	{
		if(lcd_value >= light_volume/10 && lcd_value <= 10)
		{
			LED_Off(LED5);
		}
		else if(lcd_value < light_volume/10)
		{
			LED_On(LED5);
		}
		
		if(lcd_value == 10)
		{
			lcd_value = 0;
		}
		else
		{
			lcd_value++;
		}
	}
	TIM_ClearITPendingBit(TIM3,TIM_IT_Update);  
}
//==============================================================


void TIM4_Int_Init(u32 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE);  					// 使能TIM4时钟
	
  TIM_TimeBaseInitStructure.TIM_Period = arr; 									// 自动重装载值
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  								// 定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;	// 向上计数模式
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	TIM_TimeBaseInit(TIM4,&TIM_TimeBaseInitStructure);						// 初始化TIM4
	
	TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE); 											// 允许定时器4更新中断
	TIM_Cmd(TIM4,ENABLE); 																				// 使能定时器4
	
	NVIC_InitStructure.NVIC_IRQChannel=TIM4_IRQn; 								// 定时器4中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1; 			// 抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=1; 							// 子优先级2
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

//==============================================================
//功能：定时器1中断服务函数
//参数：无
//返回: 无
//==============================================================
void TIM4_IRQHandler(void)
{
	static uint16_t SysTick_50us		= 0;													//时间片时基50us
	static uint8_t 	SysTick_count_2ms	= 0;												//时间片时基2ms
	static uint16_t SysTick_count_100ms	= 0;											//时间片时基100ms
	static uint16_t SysTick_count_1000ms	= 0;										//时间片时基1000ms
	
	if(TIM_GetITStatus(TIM4,TIM_IT_Update)==SET) 									//溢出中断
	{
		CheckOrder();																								// usb数据帧分析
		
		SysTick_50us++;
		if(	SysTick_50us == 20)
		{
			SysTick_50us		= 0;		
			SysTick_count_2ms++;
			SysTick_count_100ms++;
			SysTick_count_1000ms++;
		}

		if( SysTick_count_2ms == 2 )																	//2ms
		{ 
			FrameOrFlashBuffLoseCheck();																// 对数据包、结束符丢失检查			
			SysTick_count_2ms = 0;
			SysTick_2ms = 1;
		}
						
		if( SysTick_count_100ms == 100 )															//100ms
		{ 
			LED_Toggle(LED3);																						// LED闪烁
			SysTick_count_100ms = 0;  
			SysTick_100ms = 1;
		}
		
		if( SysTick_count_1000ms == 1000 )														//1000ms
		{ 
			if(app_update_flag > 0 && app_update_flag < 5)
			{
				app_update_flag++;
			}		
			if(app_update_flag == 4)
			{
				App_Update_Check();
				USB_OTG_StopDevice(&USB_OTG_dev);													// USB断开连接
				iap_jump_app_s();																					// 跳转到app的复位向量地址	
				app_update_flag = 0xFF;
			}		
			SysTick_count_1000ms = 0; 
			SysTick_1000ms = 1;
		}
	}
	TIM_ClearITPendingBit(TIM4,TIM_IT_Update);  										//清除中断标志位
}



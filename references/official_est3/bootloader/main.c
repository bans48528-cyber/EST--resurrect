#include "main.h"
#include "stm32f4xx_it.h"
#include "C_Protocol.h"

#define volume_ADDR   	1024*(1024*28+3200)	+ 0			// 系统音量地址
#define light_ADDR  		1024*(1024*28+3200) + 3 		// 屏幕亮度地址

//#define PRINTF_M  0																// 调试使能

u8 lcd_value=0;
u8 light_volume = 0;
u8 sound_volume = 0;																// 音量0~100
__ALIGN_BEGIN USB_OTG_CORE_HANDLE  			USB_OTG_dev __ALIGN_END;

//==============================================================
//功能：MP3播放任务
//参数：无
//返回: 无
//==============================================================
void MP3_Task(void)
{
	static u8 i = 0;
	u8 buffer[SPI_MP3_BUFNUM];
	
	if(DREQ() != 0)																		// 声音定时执行
	{
		if(Opensound_flag == 1) 												// 主机关闭声音
		{
			if( Opensound_index < 5779 )
			{
				for(i=0; i<SPI_MP3_BUFNUM; i++)
				{
					buffer[i] = Opensound[i+Opensound_index];
				}
				VS1003_WriteData(buffer,SPI_MP3_BUFNUM);
				Opensound_index += SPI_MP3_BUFNUM;		
			}
				
			if( Opensound_index >= 5779 )									// 声音播放完毕
			{
				Opensound_index = 0;
				Opensound_flag = 0;
			}
		}
	}
}
//==============================================================

//==============================================================
//功能：获取flash大小
//参数：无
//返回: 无
//==============================================================
uint16_t cpuGetFlashSize(void)
{
   return (*(__IO u16*)(0x1FFF7A22));
}
//==============================================================

//================================================================
//功能：HID发送数据到PC
//参数：buff:数据指针
//			len:数据长度
//返回：无
//================================================================
void HID_SendBuff(u8 *buff,u16 len)
{
	USBD_HID_SendReport(&USB_OTG_dev,buff,len);
}
//================================================================

//==============================================================
//功能：主函数
//参数：无
//返回: 无
//==============================================================
int main(void)
{
/*
	RCC_ClocksTypeDef RCC_Clocks;
	uint8_t clock;
*/	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);		// 配置系统中断
	SystemInit();
	delay_init();
	
	usart6_init(115200);
	
	KEY_Init();																				// 按键初始化
  LED_Init(LED3);																		// LED RED
	LED_On(LED3);
  LED_Init(LED4);																		// LED BLUE
  LED_On(LED4);                          	
	LED_Init(LED1);
  LED_On(LED1);
	
	POWER_Init();																			// 软件开关机复位脚
	Power_On();																				// 锁住电源
	delay_ms(1000);
	
	if(GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_4) == 0)	// POWER键按下
	{ 
		LED_Off(LED3);
	}
	else
	{
		Power_Off();	
		while(1);
	}
	
	LED_Init(LED5);																		// LCD_LED
  LED_Off(LED5);  
	TIM3_Int_Init(10-1,8400-1);												// 定时器时钟84M，分频系数8400，所以84M/8400=10Khz的计数频率，计数10次为1ms  
		
	SPI3_Init();																			// SPI3初始化
	VS1003_Init();
	VS1003_Reset();
	VS1003_SoftReset();
		
	W25QXX_Init();																		// 初始化W25Q128
	W25QXX_Read(&sound_volume, volume_ADDR,1);				// 获取FLASH保存的系统信息
	if(sound_volume > 100 )	
	{
		sound_volume = 100;
		W25QXX_Write(&sound_volume,volume_ADDR,1);
	}
	VS1003_SetVol(sound_volume);
	
	LED_Init(LED2);																		// LED初始化
  LED_On(LED2);
	
	LCD_UC1638C_Init();																// LCD初始化
  LCD_Display_pic(0, 0, 180, 128, LOGO);						// 加载LOGO图片
	LCD_refresh();
		
	Opensound_flag = 1;																// 准备播放声音
	Opensound_index = 0;
	
	App_Update_Check();																// 检测是否需要升级
	
	while(Opensound_flag == 1)
	{
		MP3_Task();																			// MP3播放处理				
	}
	delay_ms(1000);																		// 等待声音播放完成
	
	if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_14) == 1)	// ESC键未按下
	{
		if(STMFLASH_ReadHalfWord(APP_CONFIG_ADDR) == APP_CONFIG_SET_VALUE)	
		{			
			iap_jump_app_s();															// 跳转到APP
		}	
	}
#ifdef	PRINTF_M
	printf("\r\n跳转到APP失败");
	printf("\r\n加载USB驱动");
#endif	
	LCD_Display_pic(0, 12, 180, 128, DOWNLOAD_FIRMWARE);
	LCD_refresh();
	LED_Off(LED4); delay_ms(1000);										// 关闭LED BLUE
	USBD_Init(&USB_OTG_dev,USB_OTG_HS_CORE_ID,&USR_desc,&USBD_HID_cb,&USR_cb);
	TIM4_Int_Init(5-1,840-1);													// 任务调度,0.5ms
	
	while(1)
	{
		CheckOrder();																		// usb数据帧分析
	}
}
//==============================================================

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{
  while (1)
  {
  }
}
#endif


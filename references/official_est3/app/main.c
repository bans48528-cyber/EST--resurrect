#include "main.h"
#include "ui_control.h"
#include "C_Protocol.h"
#include "lego_pwm.h"
#include "led.h"
#include "malloc.h" 
#include "input.h"
#include "myexti.h"
 
//系统信息
const char System_Name[8] = {'D','r','.','L','u','c','k'};

//Flash保存信息
u8 sound_volume = 0;																			// 音量 0~100
u8 sleep_time		= 0;																			// 休眠时间 0~5
u8 language			= 0;																			// 界面语言 0：汉字  1：英语

u8 BriLight_last = 0;																			// 前一次LED状态
u8 format_flag=0;
char bri_name[64] = {'E','S','T'};
u8 bri_name_len = 0;
u8 bri_battery = 0;
u8 program_sleepflag = 0;																	// 系统休眠标志

__ALIGN_BEGIN USB_OTG_CORE_HANDLE       USB_OTG_Core_dev __ALIGN_END ;
__ALIGN_BEGIN USBH_HOST                 USB_Host __ALIGN_END ;
__ALIGN_BEGIN USB_OTG_CORE_HANDLE  			USB_OTG_dev __ALIGN_END;

extern HID_Machine_TypeDef HID_Machine;	

//声明
void USBH_HID_Reconnect(void);
void USBH_Connect_Check(void);
void USBD_ConnectionCheck(USB_OTG_CORE_HANDLE  *pdev);

void delayms(int n);
void deal_daisy_Chain(u8 Port, u8 DeviceId, u8 mode,	u8 bufflength, u8 buff[13]);

u8 Sleephandle(u8 sleep_time);
u8 GetBriNameLen(char bri_name[]);

//================================================================
//功能：系统配置函数
//参数：无
//返回：无
//================================================================
void App_Config(void)
{  
	RCC_ClocksTypeDef RCC_Clocks;
	uint8_t clock;
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);					// 配置系统中断
	SystemInit();	
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0X10000);				// 中断向量表偏移
	
	POWER_Init();																						// 软件开关机复位脚
	Power_On();																							// 锁住电源
	BlueTooth_GPIO();																				// 蓝牙GPIO初始化
	
	my_mem_init();																					// 内存管理初始化	
	scan_memery_init();
	
	delay_init();	
	usart6_init(115200);																		// 蓝牙和调试串口	
	get_system_id();																				// 获取系统ID信息
	
  LED_Init(LED4);	LED_On(LED4);														// LED初始化 
  LED_Init(LED3);	LED_On(LED3);
 	
	RCC_GetClocksFreq(&RCC_Clocks);													// 获取RCC频率
	clock=RCC_Clocks.SYSCLK_Frequency/1000000;
	if(clock!=168 && clock!=180)
	{
		App_debug("\r\n系统频率错误");
	}
	LED_Init(LED1);	LED_Off(LED1);	
	LED_Init(LED2);	LED_Off(LED2);	
	
	LCD_UC1638C_Init();																			// LCD初始化
	LCD_clear();																						// 清屏
	LCD_Display_pic(0, 0, 180, 128, STARTUP);								// 显示产品Logo
	LCD_refresh();
	
	SPI3_Init();																						// SPI3初始化
	VS1003_Init(); 																					// VS1003初始化
	VS1003_Reset();																					// VS1003硬件复位
	VS1003_SoftReset();																			// VS1003软件复位
		
	W25QXX_Init();																					// 初始化W25Q256
	if(font_init()!=0)																			// 判断flash有无字库
	{
		LED_On(LED1);																					// 红灯亮
		App_debug("\r\n字库不存在或者已损坏");		
	}
	res_flash = f_mount(&myfs,"1:",1);											// 挂载文件系统，文件系统挂载时会对SPI设备初始化
	if(res_flash == FR_NO_FILESYSTEM)												// 如果没有文件系统就格式化创建创建文件系统
	{
		App_debug("\r\n格式化文件系统...");
		res_flash=f_mkfs("1:",0,0);		 												// 格式化文件系统				
		App_debug("\r\n格式化完成");		
		if(res_flash == FR_OK)
		{
			res_flash = f_mount(NULL,"1:",1);			     					// 格式化后，先取消挂载
			res_flash = f_mount(&myfs,"1:",1);      						// 重新挂载
		}
	}
	
	check_app_key();																				// 检测激活码
	
	UI_Control_Init();																			// UI界面初始化
			
	TIM3_Int_Init(10-1,8400-1);															// LCD背光PWM(1ms)
	TIM5_Int_Init(16777216-1,652-1);												// 电机相关函数使用(arr+1=2^24=16777216  psc+1=84M*256/33M=652)

  USBH_Init(&USB_OTG_Core_dev,USB_OTG_FS_CORE_ID,&USB_Host,&HID_cb,&USR_Callbacks);
	USBD_Init(&USB_OTG_dev,USB_OTG_HS_CORE_ID,&USR_desc,&USBD_HID_cb,&USR_cb);
	 
	SysTick_Config(SystemCoreClock / 20000);	

	usart1_init(2400);																			// USART初始化
	usart2_init(2400);
	usart3_init(2400);
	uart4_init(2400);	
	Input_init();																						// 输入端口相应管脚初始化
	IIC_Init();																							// IIC初始化	

	Adc1_Init();																						// ADC初始化
	Adc3_Init();

	Battery_Init();																					// 电池电量检测初始化

	W25QXX_Read(&sound_volume, volume_ADDR,1);							// 获取FLASH保存的系统信息
	if(sound_volume > 100)
	{
		sound_volume = 100;
		W25QXX_Write(&sound_volume, volume_ADDR,1);
	}
	VS1003_SetVol(sound_volume); 														// 设置音量
	
	KEY_Init();																							// 按键IO初始化	
	RNG_Init();																							// 随机数产生初始化
	EXTIX_Init();																						// 电机外部中断检测编码盘数据管脚初始化
	Output_Init();																					// 电机管脚初始化

	W25QXX_Read(&light_volume, light_ADDR,1);								// 获取FLASH保存的系统信息
	W25QXX_Read(&sleep_time, sleep_ADDR,1);
	W25QXX_Read(&language, language_ADDR,1);
	W25QXX_Read((u8 *)bri_name,bri_name_ADDR,64);
	bri_name_len = GetBriNameLen(bri_name);

	if(light_volume > 100 )
	{light_volume = 100;W25QXX_Write(&light_volume, light_ADDR,1);}
	if(sleep_time > 5 )
	{sleep_time = 5;W25QXX_Write(&sleep_time, sleep_ADDR,1);}
	if(language > 1)
	{language = 1;W25QXX_Write(&language, language_ADDR,1);}

	W25QXX_Read((u8*)&bluetooth_power,bt_start_ADDR,sizeof(bluetooth_power));
	W25QXX_Read((u8*)&bluetooth_role,bt_visual_ADDR,sizeof(bluetooth_role));
	W25QXX_Read((u8*)&device_name,device_name_ADDR,sizeof(device_name));   // 本机蓝牙信息
			
	Bluetooth_test();																				// 测试蓝牙模块	
	
	IWDG_Init(4,50000); 																		// 与分频数为64,重载值为500,溢出时间为1s	
	system_turn_on = 1;																			// 标志为开机状态
}
//================================================================

//================================================================
//功能：主函数区域
//参数：无
//返回：无
//================================================================
int main(void)
{
	u8 i,tmp;
	
	App_Config();																						// 初始化外设

  while(1)
  { 
		USBH_Process(&USB_OTG_Core_dev,	&USB_Host);						// USB Host进程
		
		if( SysTick_1ms == 1 )																// 1ms任务
		{		
			FrameOrFlashBuffLoseCheck();												// 对数据包、结束符丢失检查			
			get_speed();																				// 电机速度处理
			Brake_Motor();																			// 电机刹车处理
			SysTick_1ms = 0; 
		}
		
		if( SysTick_2ms == 1 )																// 2ms任务
		{
			tmp = KEY_Scan(0);                                  // 按键检测
			if(tmp != 0)
			{ 
				BriLight_flag = BriLight_last;										// 恢复LED状态
				SystemCountTime = 0;
				ButtonAction = tmp; 
			}
			if(key_flag!=1 || programflag!=1)	Bributton_value = tmp;
			SysTick_2ms = 0; 
		}
		
		if( SysTick_10ms == 1 )																// 10ms任务
		{									
			ChannelMotorCheck();																// 电机端口检测、缓冲区赋值
			Device3TimerInterrupt1();														// DCM传感器类型检测
			USBD_ConnectionCheck(&USB_OTG_dev);									// USB Savle连接检测
			PID_Speed_Control();
			SysTick_10ms = 0; 
		}
		
		if( SysTick_50ms == 1 )																// 50ms任务
		{																											// USB 数据发送
			USBH_SendDataProcess(&USB_OTG_Core_dev, &USB_Host, reportBuff, &reportLen);		
			SysTick_50ms = 0; 
		}
		
		if( SysTick_75ms == 1 )																// 75ms任务
		{
			UI_ButtonrRespond();																// 屏幕UI处理
			SysTick_75ms = 0; 
		}
		
		if( SysTick_100ms == 1 )															// 100ms任务
		{		
			MotorRunTime_Check();
			USBH_Connect_Check();																// USB Host断开自动重连
			SysTick_100ms = 0; 
		}
		
		if( SysTick_300ms == 1 )															// 300ms任务
		{
			UpdateCascadeNum_Process();													// 级联编号更新进程		
			LightControl();																			// LED灯控制
			LED_Toggle(LED2);																		// LED2闪烁
			SysTick_300ms = 0; 
		}
		
		if( SysTick_1s == 1 )																	// 1000ms任务
		{
			Sleephandle(sleep_time);
			Battery_Detect();																		// 侦测电池类型			
			SysTick_1s = 0;   
		}
		
		CheckOrder();																					// usb数据帧分析
								
		if(programflag == 1)                            			// 用户程序运行
		{
			if(program_sleepflag)	
			{
				BriLight_flag = BriLight_last;										// 恢复LED状态				
			}
			SystemCountTime = 0;
			for(ThreadNow = 0; ThreadNow<ThreadSize; ThreadNow++)
			{
				if(Thread[ThreadNow].state != 0)
				{
					pc = Thread[ThreadNow].Threadpc;
					Byte_code_excute[Flash_Buff[pc]]();
					Thread[ThreadNow].Threadpc = pc;
				}
			}
		}
		else if(programflag == 0 && UIRUN_flag == 1)
		{
			UIRUN_flag = 0;
		}

		SoundPlay();																					// 播放声音
		
		if(Cascade_Function.allflag == 1)											// 直接指令、或者级联执行
		{
			if(program_sleepflag) 
			{
				BriLight_flag = BriLight_last;										// 恢复LED状态
				SystemCountTime = 0;
			}
			for(i=0; i<8; i++)
			{
				if(Cascade_Function.Cascade_Order[i].flag == 1)
				{
					deal_daisy_Chain(Cascade_Function.Cascade_Order[i].port,
															Cascade_Function.Cascade_Order[i].id,
																Cascade_Function.Cascade_Order[i].mode,
																	Cascade_Function.Cascade_Order[i].bufflength,
																		Cascade_Function.Cascade_Order[i].buff);			
#ifdef 	CASCADE_DEBUG
					printf("		执行控制指令\n");
					printf("i %x  \r\n",i+1);
					printf("ID %x  \r\n",Cascade_Function.Cascade_Order[i].id);
					printf("MODE %x  \r\n",Cascade_Function.Cascade_Order[i].mode);
					printf("FLAG %x  \r\n",Cascade_Function.Cascade_Order[i].flag);
					printf("bufflength %x  \r\n",Cascade_Function.Cascade_Order[i].bufflength);
#endif
				}
			}
			if(Cascade_Function.Cascade_Order[0].flag == 0	&& Cascade_Function.Cascade_Order[1].flag == 0	&&
							Cascade_Function.Cascade_Order[2].flag == 0	&& Cascade_Function.Cascade_Order[3].flag == 0	&&
									Cascade_Function.Cascade_Order[4].flag == 0	&& Cascade_Function.Cascade_Order[5].flag == 0	&&
											Cascade_Function.Cascade_Order[6].flag == 0	&& Cascade_Function.Cascade_Order[7].flag == 0)
			{
				Cascade_Function.allflag = 0;
			}
		}
  }
}
//================================================================

//================================================================
//功能：系统休眠处理
//参数：sleep_time:休眠时间
//返回：无
//================================================================
u8 Sleephandle(u8 sleep_time)
{
	static u8 low_battery_counter = 0;
	static u8 last_battery = 0;
	static u8 no_battery_counter = 0;
	u8 sleep_value;
	
	sleep_value = SleepTable[sleep_time];
	if(updateflash_start == 0 && sleep_value != 0)
	{
		if(SystemCountTime >= sleep_value*60*1000)
		{
			SOUND_Buff_flag = 0; 																// 播放关机音后进入休眠
			tab_flag.sound_flag = 0;
			Closesound_flag = 1; 			
		}
		else if(SystemCountTime >= (sleep_value*60 - 20)*1000)
		{
			if(tab_flag.Tab_Level != 10 && system_turn_on)
			{
				LCD_Display_pic(15, 35, 150, 61, SYSTEM_SHUTDOWN);// 提示系统即将关机
				if(BriLight_flag != 5)
				{
					BriLight_last = BriLight_flag;									// 记录LED状态
					BriLight_flag = 5;															// 休眠前10秒红灯闪烁		
				}
				if(tab_flag.Tab_Level <8) 
				{
					tab_flag_last.Tab_Level = tab_flag.Tab_Level;
					tab_flag_last.MainTab_choice = tab_flag.MainTab_choice;
				}
				tab_flag.update_flag = 1;
				tab_flag.Tab_Level = 10;
			}		
		}
	}
	
	if(bt_heartbeat_cnt>0) bt_heartbeat_cnt--;							// 蓝牙心跳计数
	
	battery_check();																				// 电量检测
	if(last_battery != bri_battery)
	{
		if(programflag == 0) NotificationBar();
		last_battery = bri_battery;
	}
	
	if(bri_battery == 0)																		// 电池没有电了							
	{
		no_battery_counter++;
		if( no_battery_counter == 2)
		{
			BriLight_last = BriLight_flag;											// 记录LED状态
			
		}
		if(no_battery_counter > 2) 
		{
			BriLight_flag = 4;																	// 红灯常亮
		}
		
		if(no_battery_counter > 15)
		{
			no_battery_counter = 16;
			SOUND_Buff_flag = 0; 																// 播放关机音后进入休眠
			tab_flag.sound_flag = 0;
			Closesound_flag = 1; 	
		}
	}
	else
	{
		no_battery_counter = 0;
		if(BriLight_flag == 4) BriLight_flag =  BriLight_last;	
	}
	
	if(bri_battery == 1)																		// 电量不足
	{
		if(low_battery_display && system_turn_on && tab_flag.Tab_Level != 9 && programflag == 0)
		{
			LCD_Display_pic(15, 35, 150, 61, LOW_POWER );				// 提示电量不足		
			if(tab_flag.Tab_Level <8) 
			{
				tab_flag_last.Tab_Level = tab_flag.Tab_Level;
				tab_flag_last.MainTab_choice = tab_flag.MainTab_choice;
			}
			tab_flag.update_flag = 1;
			tab_flag.Tab_Level = 9;	
		}
		if(low_battery_counter++ > 30)												// 30秒
		{
			low_battery_counter = 0;
			low_battery_display = 1;
		}
	}
	else
	{
		low_battery_display = 1;
	}
	
	return sleep_value;
}
//================================================================

//================================================================
//功能：软件延时函数
//参数：n:延时毫秒数
//返回：无
//================================================================
void delayms(int n)
{
	u16 i=0;
	
	while(n--)
	{
		i=10000; 
		while(i--);	
		IWDG_ReloadCounter();																	// 延时需要喂狗		
	}
}
//================================================================


//================================================================
//功能：USB HOST重新连接
//参数：无
//返回：无
//================================================================
void USBH_HID_Reconnect(void)
{
	USBH_DeInit(&USB_OTG_Core_dev,&USB_Host);								// 复位USB HOST
	if(USB_Host.usr_cb->DeviceDisconnected)									// 存在,才禁止
	{
		USB_Host.usr_cb->DeviceDisconnected(); 								// 关闭USB连接
		USBH_DeInit(&USB_OTG_Core_dev, &USB_Host);
		USB_Host.usr_cb->DeInit();
		USB_Host.class_cb->DeInit(&USB_OTG_Core_dev,&USB_Host.device_prop);
	}
	USB_OTG_DisableGlobalInt(&USB_OTG_Core_dev);						// 关闭所有中断
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS,ENABLE);		// USB OTG FS 复位
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS,DISABLE);	// 复位结束  

	memset(&USB_OTG_Core_dev,0,sizeof(USB_OTG_CORE_HANDLE));
	memset(&USB_Host,0,sizeof(USB_Host));
	USBH_Init(&USB_OTG_Core_dev,USB_OTG_FS_CORE_ID,&USB_Host,&HID_cb,&USR_Callbacks); 
}
//================================================================

//================================================================
//功能：USB HOST连接检测
//参数：无
//返回：无
//================================================================
void USBH_Connect_Check(void)
{
	static u8 count = 0;
	
	if(bDeviceState==1)																			// 连接建立了	bDeviceState在USBH_Process中置位
	{ 	
	}
	else																										// 连接未建立的时候,检测
	{
		if(USBH_Check_EnumeDead(&USB_Host))										// 检测USB HOST 枚举是否死机了?死机了,则重新初始化 
		{ 	    
			USBH_HID_Reconnect();																// 重连
		}			
	}
	if(USBH_Check_HIDCommDead(&USB_OTG_Core_dev,&HID_Machine))//检测USB HID通信,是否还正常? 
	{
		USBH_HID_Reconnect();																	// 重连
	}
	
	count++;
	if(count>=10)
	{
		if((HostComInsert && HostComConnected==0))						// 如果主机端usb线插入 但未成功连接
		{	
			USBH_DeInit(&USB_OTG_Core_dev, &USB_Host);					// 重初始化
		}
		count = 0;
	}
	
	if(usbrestart)
	{
		HostComConnected = 0;
		USBH_HID_Reconnect();
		usbrestart = 0;
	}
}
//================================================================

//================================================================
//功能：USB Slave连接检测
//参数：pdev:设备句柄
//返回：无
//================================================================
void USBD_ConnectionCheck(USB_OTG_CORE_HANDLE  *pdev)
{
	if(pdev->dev.device_status == USB_OTG_CONFIGURED)				// 如果 host与slave连接成功
	{
		SlaveComConnected = 1;
	}
	else
	{
		SlaveComConnected = 0;
		if(programflag == 0)
		{
			BriLight_flag = 0;
		}
	}
}
//================================================================

//================================================================
//功能：HID发送数据到PC
//参数：buff:数据指针
//			len:数据长度
//返回：无
//================================================================
void HID_SendBuff(u8 *buff,u16 len)
{
	u16 i =0;
	
	if(BT_PC_Flage == 1 && flag_bt_state == 0)								// 如果数据是从蓝牙来的
	{
		for(i=0;i<len;i++)
		{
			while(!USART_GetFlagStatus(USART6,USART_FLAG_TXE));			
			USART_SendData(USART6,buff[i]);												// 通过蓝牙发送数据 
		}
		BT_PC_Flage = 0;
	}
	else																											// 数据来自USB
	{
		USBD_HID_SendReport(&USB_OTG_dev,buff,len);
	}
}
//================================================================

//================================================================
//功能：HID发送数据到HOST
//参数：buff:数据指针
//			len:数据长度
//返回：无
//================================================================
void HID_SendBuffToHost(u8 *buff,u8 len)
{
	u8 buf[64];
	int i;
	
	if(SlaveComConnected)
	{
		buf[0] = len;																					// 首位代表数据长度
		for(i=0;i<len;i++)
		{
			buf[1+i] = *(buff+i);
		}
		USBD_HID_SendReport (&USB_OTG_dev,buf,len+1);
	}
}
//================================================================

//================================================================
//功能：获取设备名称长度
//参数：bri_name:设备名称字符串
//返回：长度
//================================================================
u8 GetBriNameLen(char bri_name[])
{
	u8 i=0;
	
	while(bri_name[i] != 0x00 && bri_name[i] != 0xFF) i++;
	if(i>=60) 
	{
		i = 60;
		bri_name[59] = 0;
	}
	return i;
}
//================================================================

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
  while (1)
  {}
}

#endif
//================================================================

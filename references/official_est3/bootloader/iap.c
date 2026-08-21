#include "iap.h"		
#include "uc1638c.h"						
											
#define Update_Buff_Size 2048														// 一次读取的块大小

//#define PRINTF_M 1

uint16_t app_update_flag = 0;

//==============================================================
//功能：检测是否需要更新程序
//参数：无
//返回: 无
//==============================================================
void App_Update_Check(void)
{
	static uint32_t Update_Count_Adderss  = UPDATE_ADDRESS;
	static uint32_t App_Count_Adderss  = APPLICATION_ADDRESS;
	static u16 Update_Buff[Update_Buff_Size] = {0x0000};
	u16 Buff_Num = 0;
	u32 Update_data_length;
	u16 i;
	
	if(STMFLASH_ReadHalfWord(Update_Data_Length_L16)!=0xFFFF || STMFLASH_ReadHalfWord(Update_Data_Length_H16)!=0xFFFF)
	{ //文件长度是否合法
		Update_data_length = STMFLASH_ReadHalfWord(Update_Data_Length_L16)+STMFLASH_ReadHalfWord(Update_Data_Length_H16)*0x10000;
		if(Update_data_length < (512-48)*1024 && Update_data_length > 200*1024)
		{
			LCD_Display_pic(0, 0, 180, 128, UPDATE);
			LCD_refresh();
				
			FLASH_Unlock();																												// 先擦除APP散区
			FLASH_EraseSector(FLASH_If_GetSectorNumber(ADDR_FLASH_SECTOR_3),VoltageRange_3);
			FLASH_EraseSector(FLASH_If_GetSectorNumber(ADDR_FLASH_SECTOR_4),VoltageRange_3);
			FLASH_EraseSector(FLASH_If_GetSectorNumber(ADDR_FLASH_SECTOR_5),VoltageRange_3);
			FLASH_EraseSector(FLASH_If_GetSectorNumber(ADDR_FLASH_SECTOR_6),VoltageRange_3);
			FLASH_EraseSector(FLASH_If_GetSectorNumber(ADDR_FLASH_SECTOR_7),VoltageRange_3);
			
			Buff_Num = Update_data_length/Update_Buff_Size;
			if(Update_data_length%Update_Buff_Size) Buff_Num += 1;
			
			for(i=0;i<=Buff_Num;i++)
			{
				STMFLASH_Read(Update_Count_Adderss,Update_Buff,Update_Buff_Size);		// 读取一块数据
				Update_Count_Adderss += Update_Buff_Size;	
	#ifdef	PRINTF_M
				if(i==0) 
				{
					printf("\r\n%08X,%04X,%04X,%04X,%04X,%04X,%04X,%04X,%04X",Update_Count_Adderss,
					Update_Buff[0],Update_Buff[1],Update_Buff[2],Update_Buff[3],
					Update_Buff[4],Update_Buff[5],Update_Buff[6],Update_Buff[7]);
				}
	#endif
				STMFLASH_Write_NoCheck(App_Count_Adderss,Update_Buff,Update_Buff_Size);	// 将数据写入APP区域
				App_Count_Adderss += Update_Buff_Size;		
				fupd_prog(114,70,16,Buff_Num,i);									// 更新进度条
	#ifdef	PRINTF_M
				printf("\r\n更新进度:%d\n",i*100/Buff_Num);
	#endif
			}			
			FLASH_Lock();																				// 上锁
		}
		else
		{
#ifdef	PRINTF_M
		printf("\r\n数据长度不对，文件长度%d",Update_data_length);	
#endif
		}
	}
	
	if(STMFLASH_ReadHalfWord(APP_CONFIG_ADDR)!=APP_CONFIG_SET_VALUE )				
	{																												// 更新完毕，清除更新标志位
#ifdef	PRINTF_M
		printf("\r\n更新完毕，清除更新标志位");	
#endif
		FLASH_Unlock();																	// 解锁FLASH,为写入做准备
		FLASH_DataCacheCmd(DISABLE);		
		FLASH_EraseSector(FLASH_Sector_3,VoltageRange_3);
		Test_Write(Update_Data_Length_L16,0xFFFF); 
		Test_Write(Update_Data_Length_H16,0xFFFF); 
		Test_Write(APP_CONFIG_ADDR,APP_CONFIG_SET_VALUE); 		
		FLASH_DataCacheCmd(ENABLE);
		FLASH_Lock();
	}
}

typedef  void (*iapfun)(void);														// 定义一个函数类型的参数.
iapfun jump2app;

//跳转到应用程序段
//appxaddr:用户代码起始地址.
void iap_load_app(u32 appxaddr)
{
#ifdef	PRINTF_M
	printf("\r\nappxaddr:%08X",appxaddr);
	printf("\r\n*appxaddr:%08X",((*(vu32*)appxaddr)));
	printf("\r\n*appxaddr&0x2FFD0000:%08X",((*(vu32*)appxaddr)&0x2FFE0000));
#endif
	
//	if(((*(vu32*)appxaddr)&0x2FFE0000)==0x20000000)				// 检查栈顶地址是否合法.
	if(((*(vu32*)appxaddr)&0x2FFE0000)==0x20020000 || ((*(vu32*)appxaddr)&0x2FFE0000)==0x20000000)	
	{
		jump2app=(iapfun)*(vu32*)(appxaddr+4);								// 用户代码区第二个字为程序开始地址(复位地址)	
		MSR_MSP(*(vu32*)appxaddr);														// 初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)
		TIM_DeInit(TIM3);
		TIM_DeInit(TIM4);
		USART_DeInit(USART6);
		SPI_I2S_DeInit(SPI3);			
		jump2app();																						// 跳转到APP.
	}
}

//跳转到app区域运行
void iap_jump_app_s(void)
{
	iap_load_app(APPLICATION_ADDRESS);											// 跳转到app的复位向量地址
}

void iap_Func(u32 Update_Data_Length)
{
	FLASH_Unlock();	
	FLASH_EraseSector(FLASH_Sector_3, VoltageRange_3);			// 修改Update_Data_Length长度
	Test_Write(Update_Data_Length_L16,Update_Data_Length%0x10000);
	Test_Write(Update_Data_Length_H16,Update_Data_Length/0x10000);
	Test_Write(APP_CONFIG_ADDR,APP_CONFIG_CLEAR_VALUE) ;		// 将APP_CONFIG_ADDR置空
	FLASH_Lock();	
	app_update_flag = 1;																		// 表示更新成功	
}


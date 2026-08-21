#include "iap.h"
#include "stdio.h"
#include "power.h"
#include "sys.h"

#define FLASH_IAP_ADDR		(uint32_t)0x08010000  	

typedef  void (*iapfun)(void);										//定义一个函数类型的参数.
iapfun jump2iap;

void iap_jump(u32 iapxaddr)
{
	if(((*(vu32*)iapxaddr)&0x2FFE0000)==0x20000000)	//检查栈顶地址是否合法.0x20000000是sram的起始地址,也是程序的栈顶地址
	{ 		
		jump2iap=(iapfun)*(vu32*)(iapxaddr+4);				//用户代码区第二个字为程序开始地址(复位地址)		
		MSR_MSP(*(vu32*)iapxaddr);										//初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)
		jump2iap();																		//跳转到APP.
	}
}


void iap_Func(u32 Update_Data_Length)
{
	FLASH_Unlock();	
																									// 修改Update_Data_Length长度
	FLASH_EraseSector(FLASH_Sector_3, VoltageRange_3);
	Test_Write(Update_Data_Length_L16,Update_Data_Length%0x10000);
	Test_Write(Update_Data_Length_H16,Update_Data_Length/0x10000);
																									// 将APP_CONFIG_ADDR置空
	Test_Write(APP_CONFIG_ADDR,APP_CONFIG_CLEAR_VALUE) ;
	FLASH_Lock();	
	delay_ms(1000);																	// 延时，等待USB数据发送完成
	Power_Off();																		// 关闭电源
//	NVIC_SystemReset();
}


 












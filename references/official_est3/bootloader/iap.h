#ifndef __IAP_H_
#define __IAP_H_

#include "flash.h"
#include "sys.h"
#include "delay.h"
#include "flash_if.h"
#include "uart5.h"

#define APP_CONFIG_ADDR 					0X0800C000+16			//配置地址，页1
#define APP_CONFIG_SET_VALUE			0X5555						//设置值
#define APP_CONFIG_CLEAR_VALUE		0X0000						//清零值
#define APP_CONFIG_NONE_VALUE			0XFFFF						//初始配置值

#define Update_Data_Length_L16 		0x0800C000				//APP文件长度低字节
#define Update_Data_Length_H16 		0x0800C000+8			//APP文件长度高字节

extern uint16_t app_update_flag;

void iap_jump_app_s(void);
void App_Update_Check(void);
void iap_Func(u32 Update_Data_Length);

#endif




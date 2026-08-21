#ifndef __FLASH_H__
#define __FLASH_H__

#include "delay.h"

#define STM32_FLASH_SIZE 	1024*1024
#define STM32_FLASH_BASE 	0x08000000 														// STM32 FLASH的起始地址
#define STM_SECTOR_SIZE		1024*2

u16 STMFLASH_ReadHalfWord(u32 faddr);
u32 STMFLASH_ReadWord(u32 faddr);		  													// 读出字  
void STMFLASH_Read(u32 ReadAddr,u16 *pBuffer,u16 NumToRead);
void Test_Write(u32 WriteAddr,u16 WriteData);
void STMFLASH_Write_NoCheck(u32 WriteAddr,u16 *pBuffer,u16 NumToWrite) ;
void Erase_AppBack(void);

#endif


















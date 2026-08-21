#include "flash.h"
#include "flash_if.h"
#include "stdio.h"


u16 STMFLASH_ReadHalfWord(u32 faddr)
{
    return *(vu16*)faddr; 
}

u32 STMFLASH_ReadWord(u32 faddr)
{
	return *(vu32*)faddr; 
}  

void STMFLASH_Write_NoCheck(u32 WriteAddr,u16 *pBuffer,u16 NumToWrite)   
{
	u16 i;
	
	for(i=0;i<NumToWrite;i++)
	{
		FLASH_ProgramHalfWord(WriteAddr,pBuffer[i]);
		WriteAddr+=2;																	//地址增加2.
	}
}

void STMFLASH_Read(u32 ReadAddr,u16 *pBuffer,u16 NumToRead)   	
{
	u16 i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMFLASH_ReadHalfWord(ReadAddr);		//读取2个字节.
		ReadAddr+=2;																	//偏移2个字节.	
	}
}

void Test_Write(u32 WriteAddr,u16 WriteData)   	
{
	STMFLASH_Write_NoCheck(WriteAddr,&WriteData,1);
}

void Erase_AppBack(void)
{
	uint8_t status = 0;
	
	FLASH_Unlock();																	// 解锁FLASH,为写入做准备
	FLASH_DataCacheCmd(DISABLE);	
	
	status = FLASH_EraseSector(FLASH_Sector_8,VoltageRange_3);
	if(status != FLASH_COMPLETE) printf("\r\n擦除散区8=%d",status);
	status = FLASH_EraseSector(FLASH_Sector_9, VoltageRange_3);
	if(status != FLASH_COMPLETE) printf("\r\n擦除散区9=%d",status);
	status = FLASH_EraseSector(FLASH_Sector_10, VoltageRange_3);
	if(status != FLASH_COMPLETE) printf("\r\n擦除散区10=%d",status);
	status = FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);
	if(status != FLASH_COMPLETE) printf("\r\n擦除散区11=%d",status);
	
	FLASH_DataCacheCmd(ENABLE);
	FLASH_Lock();
}





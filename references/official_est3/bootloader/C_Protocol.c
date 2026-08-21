#include <string.h>
#include "iap.h"
#include "led.h"
#include "stdio.h"
#include "C_Protocol.h"
#include "uc1638c.h"

const char version[7]=
{
'0','3','.','0','0','B'											// 软件版本号
};	

extern void HID_SendBuff(u8 *buff,u16 len);

uint8_t USB_Rx_Buff[1024];									// USB接收数据帧缓存
unsigned int USB_Rx_Length = 0;
unsigned int USB_TimePic = 1000;						// 接收时间计时
uint8_t Receive_start = 0;									// 帧内数据接收标志位
uint8_t errorflag = 0;											// 帧尾丢失
uint8_t Receive_end = 0;

uint32_t Flash_TimePic = 400000;						// 包接收时间计时
uint8_t flashbuff_start = 0;								// 包接收标志位
uint8_t flashtimeout = 0;										// 包接收超时

uint32_t UpdateFlash_TimePic = 400000;			// 包接收时间计时
uint8_t updateflash_start = 0;							// 包接收标志位
uint8_t updateflashtimeout = 0;							// 包接收超时
uint32_t Updateflash_length = 0;

void Send_Heartbeat_Packet(void);
uint8_t ProgramToUpdateFlash(uint8_t *Buff, u32 *Total_Length);
void Send_Firmware_Download(u16 total, u16 now, u8 flag);
void Send_Error_Order(char n);

//================================================================
//功能：USB数据帧分析
//参数：无
//返回：无
//================================================================
void CheckOrder(void)
{
	int i,updateflag;
	static char sum_checkcode = 0x00; 												// 校验码累加
			
	if(!Receive_start&&USB_Rx_Length&&Receive_end)						// 数据接收完全后，使用数据
	{ //printf("\r\nUSB接收");
		//for(i=0;i<USB_Rx_Length;i++)printf("%02X ",USB_Rx_Buff[i]);
		sum_checkcode = 0;
		for(i=0;i<USB_Rx_Length-2;i++)sum_checkcode += USB_Rx_Buff[i];
		if(sum_checkcode != USB_Rx_Buff[USB_Rx_Length-2])				// 校验码不匹配，数据接收不完整
		{
			Send_Error_Order(0x02);																// 校验码出错
		}
		else
		{											
			switch(USB_Rx_Buff[2])																// 解析功能码
			{
				case 0x01:	// 心跳包
					if(USB_Rx_Buff[3]==0x00&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==7)	
					{																									// 判断数据域格式是否对应
						Send_Heartbeat_Packet();
					}
					else
					{
						Send_Error_Order(0x05); 												// 数据域格式不对应
					}
					break;
																																		
				case 0x05:	// 固件升级
					updateflag = ProgramToUpdateFlash(USB_Rx_Buff, &Updateflash_length);
					switch(updateflag)
					{
						case 1:
							iap_Func(Updateflash_length-1);								// 修改固态标志及更新数据长度
							Send_Firmware_Download(USB_Rx_Buff[5]+USB_Rx_Buff[6]*256,USB_Rx_Buff[7]+USB_Rx_Buff[8]*256,0x01);						
							break;
						case 2:
							Send_Firmware_Download(USB_Rx_Buff[5]+USB_Rx_Buff[6]*256,USB_Rx_Buff[7]+USB_Rx_Buff[8]*256,0x01); 										
							break;
						default:
							Send_Firmware_Download(USB_Rx_Buff[5]+USB_Rx_Buff[6]*256,USB_Rx_Buff[7]+USB_Rx_Buff[8]*256,0x00);										
							break;
					}
					break;																				
																																																				
				default: Send_Error_Order(0x04);											// 无效功能码
					break;
				}
			}
		
			USB_Rx_Length = 0;																			// 允许接收下一次数据
			Receive_end = 0;
		}
		
		if(errorflag)
		{
			Send_Error_Order(0x03);																	// 帧格式不全
			errorflag = 0;
		}

		if(updateflashtimeout)
		{
			Send_Firmware_Download(USB_Rx_Buff[5],USB_Rx_Buff[6],0x02);// 包接收超时
			updateflashtimeout = 0;
		}
}
//================================================================

//================================================================
//功能：发送心跳包函数
//参数：无
//返回：无
//================================================================
void Send_Heartbeat_Packet(void)   
{
	int _cnt,i;
	u8 data_to_send[1024]={"0"},jiaoyanma;
	
	_cnt = 0;jiaoyanma = 0x00;

	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x01;
	data_to_send[_cnt++]=0x06;	
	data_to_send[_cnt++]=0x00;
		
	for(i=0;i<6;i++) data_to_send[_cnt++] = version[i];
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;	
	data_to_send[_cnt++]=0x16;
	
	HID_SendBuff(data_to_send,HID_IN_PACKET);
}
//================================================================

//================================================================
//功能：固件下载返回函数
//参数：total:总帧数
//			now:当前帧编号
//			flag:执行结果
//返回：无
//================================================================
void Send_Firmware_Download(u16 total, u16 now, u8 flag)
{
	int _cnt,i;
	u8 data_to_send[1024]={"0"},jiaoyanma;
	
	_cnt = 0;jiaoyanma = 0x00;

	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x05;
	data_to_send[_cnt++]=0x05;	
	data_to_send[_cnt++]=0x00;
		
	data_to_send[_cnt++]=total;
	data_to_send[_cnt++]=total>>8;
	data_to_send[_cnt++]=now;
	data_to_send[_cnt++]=now>>8;
	data_to_send[_cnt++]=flag;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);		
}
//================================================================

//================================================================
//功能：错误指令返回函数
//参数：n:代表指令错误类型
//返回：无
//================================================================
void Send_Error_Order(char n)   
{
	int _cnt = 0,i;
	u8 data_to_send[1024]={"0"},jiaoyanma = 0;

	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0xee;
	data_to_send[_cnt++]=0x01;	
	data_to_send[_cnt++]=0x00;
	data_to_send[_cnt++]=n;
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
	HID_SendBuff(data_to_send,HID_IN_PACKET);	
}
//================================================================

//================================================================
//功能：固件更新，将usb接收到的帧数据存到内置flash的Update区
//参数：Frame_Buff:数据指针
//			Total_Length:数据总长度
//返回：0=错误,1=完成，2=正确
//================================================================
uint8_t ProgramToUpdateFlash(uint8_t *Buff, u32 *Total_Length)
{
	static unsigned int i = 0;
	static uint32_t Update_Count_Adderss;
	static u16 frame_now=0;																			// 避免重复写入同样帧
	static u8 CRC_check  = 0;

	u16 Update_Length = 0;
	u16 Update_Buff[1024] = {0}; 																// 用于缓存数据的数组
	u16 temp = 0;
	u16 frame_index = 0;
	u16 frame_total = 0;
	u16 data_len = 0;

	frame_index = Buff[7]+Buff[8]*256;													// 计算当前帧编号
	frame_total = Buff[5]+Buff[6]*256;													// 计算总帧数
		
	if(frame_index==0 && updateflash_start != 1)																			
	{								
		if(Buff[9]=='A' && Buff[10]=='P' && Buff[11]=='P'&& Buff[12]=='=')
		{	
			*Total_Length = 0;
			UpdateFlash_TimePic = 500000;														// 包接收时间为500s
					
//			printf("\r\n擦除FLASH");
			LED_On(LED3);
			FLASH_Unlock();																					// 解锁FLASH,为写入做准备			
			FLASH_EraseSector(FLASH_Sector_8, VoltageRange_3);
			FLASH_EraseSector(FLASH_Sector_9, VoltageRange_3);
			FLASH_EraseSector(FLASH_Sector_10, VoltageRange_3);
			FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);
						
			Update_Count_Adderss = UPDATE_ADDRESS-4;
			frame_now = 0xFFFF;
			updateflash_start = 1;
		}
		else
		{
//			printf("\r\nAPP=%02X,%02X,%02X,%02X",Buff[9],Buff[10],Buff[11],Buff[12]);
			return 0;
		}
	}
//	printf("\r\n帧：%d/%d",frame_index,frame_total);
	if(updateflash_start)																				
	{ 
		if(frame_now != frame_index)															// 避免重复写入同样帧
		{
			data_len = Buff[3] + 256*Buff[4];
			
			CRC_check = 0;
			for(i = 0; i <  data_len + 5; i++)											// 计算CRC
			{
				CRC_check += Buff[i];
			}
			
			if(CRC_check != Buff[data_len+5])												// 判断CRC
			{
//				printf("\r\nCRC错误:%02X,%02X",CRC_check,Buff[data_len+5]);	
				return 0;
			}
			
			for(i = 0; i < data_len-4; i+=2)												// 数据八位融合为16位
			{
				temp = (((u16)Buff[9+i+1])<<8) + ((u16)Buff[9+i]);
				Update_Buff[Update_Length] = temp;
				Update_Length++;				
			}
			
			STMFLASH_Write_NoCheck(Update_Count_Adderss,Update_Buff,Update_Length);
			Update_Count_Adderss += 2*Update_Length;								// 写入FLASH并更新写入位置
	
			*Total_Length += 2*Update_Length;												// 更新数据总共长度		
			frame_now = frame_index;
		}
		
		fupd_prog(130,84,16,frame_total,frame_index);							// 更新进度条
		
		if(frame_total <= frame_index+1)													// 尾帧
		{ 
//			printf("\r\n尾帧");		
			FLASH_Lock();																						// 上锁
			updateflash_start = 0;																	// 本次program接收完成，允许接收新的program
			return 1;	
		}
		else
		{
			return 2;																								// 当前数据包处理成功		
		}
	}
//	printf("\r\n未开始固件下载");		
	
	return 0;
}
//================================================================

//================================================================
//功能：数据帧结束符丢失检查	
//参数：无
//返回：无
//================================================================
void FrameBuffLoseCheck(void)
{	
	if(USB_TimePic) USB_TimePic--;		
	if(!USB_TimePic&&Receive_start)															// 当到达时间上限,usb还没接收完全数据帧,则系统报错
	{
		Receive_start = 0;																				// 允许重新接收数据
		USB_Rx_Length = 0;
		errorflag = 1;
	}
}
//================================================================

//================================================================
//功能：对数据包丢失检查
//参数：无
//返回：无
//================================================================
void FlashBuffLoseCheck(void)
{
	if(Flash_TimePic) Flash_TimePic--;		
	if(!Flash_TimePic&&flashbuff_start)													// 当到达时间上限,包接收未完成
	{
		flashbuff_start = 0;																			// 允许重新接收数据
		flashtimeout = 1;																					// 提示包接收超时
	}
	
	if(UpdateFlash_TimePic) UpdateFlash_TimePic--;
	if(!UpdateFlash_TimePic&&updateflash_start)									// 当到达时间上限,包接收未完成
	{
		updateflash_start = 0;																		// 允许重新接收数据
		updateflashtimeout = 1;																		// 提示包接收超时
	}
}
//================================================================

//================================================================
//功能：帧/包检测
//参数：无
//返回：无
//================================================================
void FrameOrFlashBuffLoseCheck(void)
{
	FrameBuffLoseCheck();
	FlashBuffLoseCheck();
}
//================================================================




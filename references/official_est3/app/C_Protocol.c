#include <string.h>
#include "stdio.h"
#include "main.h"
#include "excutefunction.h"
#include "C_Protocol.h"
#include "spi3.h"
#include "malloc.h"
#include "lego_pwm.h"

extern void delayms(int n);

uint8_t USB_Rx_Buff[1024+128];							// USB接收数据帧缓存
unsigned int USB_Rx_Length = 0;
unsigned int USB_TimePic = 1000;						// 接收时间计时
uint8_t Receive_start = 0;									// 帧内数据接收标志位
uint8_t errorflag = 0;											// 帧尾丢失
uint8_t Receive_end = 0;

uint32_t Flash_TimePic = 800000;						// 包接收时间计时
uint8_t flashbuff_start = 0;								// 包接收标志位
uint8_t flashtimeout = 0;										// 包接收超时

uint32_t UpdateFlash_TimePic = 400000;			// 包接收时间计时
uint8_t updateflash_start = 0;							// 包接收标志位
uint8_t updateflashtimeout = 0;							// 包接收超时
uint32_t Updateflash_length = 0;

uint8_t Channel_State[32][13] = {0};				// 功能2状态查询所需的端口数组，记录各端口传感器与电机的状态
																						// 8组传感器数据，每组数据包括PORT、ID、Mode、XX、XX、XX、XX、XX、XX、XX、XX、XX、XX
uint8_t UserWareData[10];										// 用于波形上传的用户波形数据
																						// 第0位为触发标志位，1-8为用户数据位
uint8_t UserMotorControlData[13];						// 用用于电机上位机控制的用户数据数组
																						// 第0位为触发标志位，1-12为用户数据位
u8 usbmasterflag = 0;

int usbmastertimems = 0;

uint8_t i,programlen[2];

void Send_Heartbeat_Packet(void);
void Send_Sensor_Data(u8 Connection_N);
void Send_System_Source(void);   
void Send_Program_Download(u8 mode, u8 flag);

void Send_Firmware_Download(u16 total, u16 now, u8 flag);
void Send_Wave(u8 lay,u8 port,u8 id, u8 mode);
void GetRamData(u8 Address0,u8 Address1,u8 Address2,u8 Address3,u8 Length);
void Send_Error_Order(char n);

uint8_t ProgramToFile(uint8_t Frame_Buff[], char *file);
uint8_t ProgramToUpdateFlash(uint8_t *Buff, u32 *Total_Length);
void Send_MotorControl(void);  
void Send_FATFS_State(uint8_t mode, uint8_t state);

void Send_FATFS_PathNone(void);
void Send_FATFS_PathAll(void);
void Send_FATFS_File(uint8_t totalframe, u8 nowframe,  uint8_t  Flash_Buff[],unsigned int Flash_BuffSize);
void Send_Project_State(uint8_t state);
void Send_Project_Adr(uint8_t flag, uint32_t pc, uint8_t instruct);
void Send_Namebattery(uint8_t bri_battery, char bri_name[]);
uint8_t PianoToFlash(uint8_t *Buff);
void Send_PianoToFlash(u8 Num, u32 Offset, u8 flag);
uint8_t FontToFlash(uint8_t *Buff);
void Send_FontToFlash(u8 Num, u32 Offset, u8 flag);
void Send_uuid(void);
uint8_t KeyToFlash(uint8_t *Buff);
void Send_KeyToFlash(u8 flag);

u8 project_file_num=0,project_file_num_now=0;

//================================================================
//功能：USB数据帧分析
//参数：无
//返回：无
//================================================================
void CheckOrder(void)
{
	char *file_path;
	int i,res,updateflag;
	static char sum_checkcode = 0x00; 												// 校验码累加
	DIR dir;
			
	if(usbmastertimems>0)
		usbmastertimems--;
	else
		usbmasterflag = 0;

	if(!Receive_start&&USB_Rx_Length&&Receive_end)						// 数据接收完全后，使用数据
	{ 
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
						usbmasterflag = 1;
						usbmastertimems = 150;
						Send_Heartbeat_Packet();
					}
					else
					{
						Send_Error_Order(0x05); 												// 数据域格式不对应
					}
					break;
					
				case 0x02:	// 传感器状态查询
					if(USB_Rx_Buff[3]==0x01&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==8)	
					{																									// 判断数据域格式是否对应
						Send_Sensor_Data(USB_Rx_Buff[5]) ; 
					}
					else
					{
						Send_Error_Order(0x05); 												// 数据域格式不对应
					}
					break;
					
				case 0x03:	// 系统资源查询
					if(USB_Rx_Buff[3]==0x00&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==7)	
					{																									// 判断数据域格式是否对应
						Send_System_Source();
					}
					else
					{
						Send_Error_Order(0x05); 												// 数据域格式不对应
					}
					break;
					
				case 0x04:	// 程序下载
					file_path = mymalloc(256);												// 申请内存
					if(USB_Rx_Buff[5] == 1)														// 下载程序模式(创建目录和文件)
					{
						if(USB_Rx_Buff[7] == 0)
						{
							BriLight_flag = 4;
						}
						project_file_num = USB_Rx_Buff[6];
						project_file_num_now = USB_Rx_Buff[7];
						
						memset(file_path,0,256);
						file_path[0] = '1'; file_path[1] = ':';
						for(i=0; i< USB_Rx_Buff[8]-1; i++)
						{
							file_path[2+i] = USB_Rx_Buff[9+i];
						}												
						res=f_opendir(&dir,(const TCHAR*)file_path);		// 尝试打开目录 
						if(res!=FR_OK)
						{													
							res=f_mkdir((const TCHAR*)file_path);					// 打开目录失败，创建目录 
						}
						else
						{													
							res=f_closedir(&dir);													// 如果目录已经存在，关闭它 
						}
						
						if(res == FR_OK)
						{
							memset(file_path,0,256);
							file_path[0] = '1'; file_path[1] = ':';
							for(i=0; i< (USB_Rx_Buff[3]+0xFF*USB_Rx_Buff[4]-4); i++)
							{
								file_path[2+i] = USB_Rx_Buff[9+i];
							}
							res_flash = f_open(&file1, (const TCHAR*)file_path, FA_CREATE_ALWAYS | FA_WRITE );
							if(res_flash == FR_OK)
							{
								f_close(&file1);
								Send_Program_Download(0x01,0x01);
							}
							else
							{
								BriLight_flag = 0;																	
								Send_Program_Download(0x01,0x02);						// 返回打开失败
							}							
						}
						else
						{
							BriLight_flag = 0;
							Send_Program_Download(0x01,0x02);
						}																									
					}
					else if(USB_Rx_Buff[5] == 2)											// 下载程序模式2
					{
						//flashbuff_flag = ProgramToFlash(USB_Rx_Buff,Flash_Buff,&Flash_Buff_Length,file_path);
						flashbuff_flag = ProgramToFile(USB_Rx_Buff, file_path);
						if(flashbuff_flag == 1 )
						{																															
							if(project_file_num_now>=project_file_num-1 && USB_Rx_Buff[7]>=USB_Rx_Buff[6]-1)
							{
								downloadsound_flag = 1;
								BriLight_flag = 0;																
								for(i=0;i<5;i++)Tab[i].Location = 0;				// 各大类功能的子选项位置置0																																		
								tab_flag.Tab_Level = 0;											// 显示主界面
								tab_flag.MainTab_choice =  0;								// 大类功能0															
								strcpy((char*)fpath,"1:");
								window_top = 0;
								file_load_clear();
								scan_files((char *)fpath,0);
								inversion_file_load();
								Tab[0].SubfunctionNum = file_name_num;
								tab_flag.update_flag = 1;
								tab_flag.sound_flag = 1;	
							}								
							Send_Program_Download(0x02,0x01);							// 返回下载成功信号                             													
						}
						else if(flashbuff_flag == 2)
						{
							Send_Program_Download(0x02,0x01);
						}
						else
						{
							BriLight_flag = 0;
							Send_Program_Download(0x02,0x02);
						}
					}
					myfree(file_path);																// 释放内存
					break;
													
				case 0x05:	// 固件升级
					updateflag = ProgramToUpdateFlash(USB_Rx_Buff, &Updateflash_length);
					switch(updateflag)
					{
						case 1:
							Send_Firmware_Download(USB_Rx_Buff[5]+USB_Rx_Buff[6]*256,USB_Rx_Buff[7]+USB_Rx_Buff[8]*256,0x01);
							iap_Func(Updateflash_length-1);								// 修改固态标志及更新数据长度，并跳转至IAP
							break;
						case 2:
							Send_Firmware_Download(USB_Rx_Buff[5]+USB_Rx_Buff[6]*256,USB_Rx_Buff[7]+USB_Rx_Buff[8]*256,0x01); 										
							break;
						default:
							Send_Firmware_Download(USB_Rx_Buff[5]+USB_Rx_Buff[6]*256,USB_Rx_Buff[7]+USB_Rx_Buff[8]*256,0x00);										
							break;
						}
						break;
						
					case 0x06:	// 波形上传
						if(USB_Rx_Buff[3]==0x09&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==16)	
						{																								// 判断数据域格式是否对应
							UserWareData[0] = 1;													// 触发标志位							
							for(i=0;i<9;i++) UserWareData[1+i] = USB_Rx_Buff[5+i];																																		
							Send_Wave(UserWareData[6],UserWareData[7],UserWareData[8],UserWareData[9]) ;
						}
						else
						{
							Send_Error_Order(0x05); 
						}
						break; 
						
					case 0x07:	// RAM内存地址内容查询
						if(USB_Rx_Buff[3]==0x05&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==12)	
						{																								// 判断数据域格式是否对应
							GetRamData(USB_Rx_Buff[5],USB_Rx_Buff[6],USB_Rx_Buff[7],USB_Rx_Buff[8],USB_Rx_Buff[9]);
						}
						else
						{
							Send_Error_Order(0x05); 	
						}
						break; 
						
					case 0x08:	// 电机上位机控制
						if(USB_Rx_Buff[3]==0x05&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==12)	
						{																								// 判断数据域格式是否对应
							UserMotorControlData[0] = 1;									// 触发标志位												
							for(i=0;i<6;i++)UserMotorControlData[1+i] = USB_Rx_Buff[5+i];
							if(UserMotorControlData[5] == 2)
							{
								Motor_PowerSet(UserMotorControlData[2],0);
							}
							else
							{
								Large_Motor_On(UserMotorControlData[2], 100.0,UserMotorControlData[5]);
							}										
							Send_MotorControl(); 
						}
						else
						{
							Send_Error_Order(0x05); 	
						}
						break; 
													
					case 0x09:	// 文件系统控制
						file_path = mymalloc(256);											// 申请内存
						memset(file_path,0,256); file_path[0] = '1'; file_path[1] = ':';
						switch(USB_Rx_Buff[5])
						{
							case 0x01:																		// 复制文件
							{																			
								for(i=0; i<USB_Rx_Buff[6]; i++) file_path[2+i] = USB_Rx_Buff[7+i];												
								res_flash = f_open(&file1,(const TCHAR*)file_path, FA_OPEN_EXISTING | FA_READ);
								if(res_flash == FR_OK)
								{
									memset(file_path,0,256); file_path[0] = '1'; file_path[1] = ':';
									for(i=0; i<USB_Rx_Buff[USB_Rx_Buff[6]+7]; i++) file_path[2+i] = USB_Rx_Buff[USB_Rx_Buff[6]+8+i];												
									res_flash = f_open(&file2, (const TCHAR*)file_path, FA_CREATE_ALWAYS | FA_WRITE );
									if ( res_flash == FR_OK )
									{
										Flash_Buff_frame = file1.fsize/Flash_Buffsize;
										for(i=0; i< Flash_Buff_frame; i++)
										{
											res_flash = f_read(&file1+Flash_Buff_frame*Flash_Buffsize, Flash_Buff+i*Flash_Buffsize,  Flash_Buffsize, &fnum); 
											res_flash=f_write(&file2+Flash_Buff_frame*Flash_Buffsize,Flash_Buff+i*Flash_Buffsize, Flash_Buffsize,&fnum);
										}
										res_flash = f_read(&file1+Flash_Buff_frame*Flash_Buffsize, Flash_Buff+Flash_Buff_frame*Flash_Buffsize,  file1.fsize-Flash_Buff_frame*Flash_Buffsize , &fnum); 
										res_flash=f_write(&file2+Flash_Buff_frame*Flash_Buffsize,Flash_Buff+Flash_Buff_frame*Flash_Buffsize, file1.fsize-Flash_Buff_frame*Flash_Buffsize ,&fnum);												
										f_close(&file2);
										f_close(&file1);
										Flash_Buff_frame=0;
										Send_FATFS_State(0x01,1);
									}
									else
									{
										Send_FATFS_State(0x01,0);
									}
								}
								else
								{
									Send_FATFS_State(0x01,0);
								}
							}
							break;
							
							case 0x02:																			// 删除文件
							{
								for(i=0; i< (USB_Rx_Buff[3]+0xFF*USB_Rx_Buff[4]-1); i++) file_path[2+i] = USB_Rx_Buff[6+i];
								res_flash = f_unlink((const TCHAR*)file_path);
								if(res_flash==FR_OK)
								{
									Send_FATFS_State(0x02,1);
								}
								else
								{
									Send_FATFS_State(0x02,0);
								}														
							}
							break;
							
							case 0x03:																		// 查看全部项目文件
							{
								filelist_index = 0;
								strcpy((char*)fpath,"1:");
								scan_all_files((char *)fpath);  //LCD_ShowNum(16,58,filelist_index,3,12,0);
								if(filelist_index > 0)
								{
									Send_FATFS_PathAll();
								}
								else																				// 不存在任何文件
								{
									Send_FATFS_PathNone();
								}
								/*
								for(i=0; i<filelist_index; i++)
								{
									if(allfilesize[i]<1024)
										Send_FATFS_Path(filelist_index,i,filelist[i],1);
									else
										Send_FATFS_Path(filelist_index,i,filelist[i],(u8)(allfilesize[i]/1024));
								}
								*/
								for(i=0; i<file_max; i++)
								{
									if(filelist[i] != NULL)
									{
										myfree(filelist[i]);										// 释放内存
										filelist[i] = NULL;
									}
								}								
							}
							break;
							
							case 0x04:																		// 文件上传
							{
								for(i=0; i< (USB_Rx_Buff[3]+0xFF*USB_Rx_Buff[4]-1); i++) file_path[2+i] = USB_Rx_Buff[6+i];															
								res_flash = f_open(&file1, (const TCHAR*)file_path, FA_OPEN_EXISTING | FA_READ); 	 
								if(res_flash == FR_OK)
								{
									res_flash = f_read(&file1, Flash_Buff, file1.fsize, &fnum); 
									if(res_flash==FR_OK)
									{
										Flash_Buff_frame = (file1.fsize-1)/(1024-10);
										for(i=0; i< Flash_Buff_frame; i++)
										{
											Send_FATFS_File(Flash_Buff_frame+1,i,Flash_Buff+i*(1024-10),(1024-10));
											delayms(50);
										}
										Send_FATFS_File(Flash_Buff_frame+1,Flash_Buff_frame,Flash_Buff+Flash_Buff_frame*(1024-10),file1.fsize-Flash_Buff_frame*(1024-10));
									}
									Flash_Buff_frame=0;
									f_close(&file1);	
								}												
							}
							break;
						}
						myfree(file_path);																// 释放内存
						break; 
													
					case 0x0A:	// 下位机程序启动/关闭
						if(USB_Rx_Buff[5] == 0x01)												// 启动
						{
							memset(file_path,0,256);											
							file_path[0] = '1'; file_path[1] = ':';
							for(i=0; i< (USB_Rx_Buff[3]+0xFF*USB_Rx_Buff[4]-1); i++) file_path[2+i] = USB_Rx_Buff[6+i];														
							tab_flag.Tab_Level = 0;													// 选择主界面
							tab_flag.update_flag = 1;
							EnterProgram((u8 *)file_path);
							Send_Project_State(0x01);													
						}
						else if(USB_Rx_Buff[5] == 0x02)										// 关闭
						{
							EndProgram();																													
							Motor_DirectionSet(5,0);												// 暂停电机
							Motor_DirectionSet(6,0);
							Motor_DirectionSet(7,0);
							Motor_DirectionSet(8,0);
							Send_Project_State(0x01);
						}
						else
						{
							Send_Project_State(0x00);
						}
						break; 
						
					case 0x0B:	// 下位机返回运行的地址
						if(USB_Rx_Buff[3]==0x00&&USB_Rx_Buff[4]==0x00&&USB_Rx_Length==7)	
						{																									// 判断数据域格式是否对应
							if(programflag == 1 )
							{
								Send_Project_Adr(1, pc, Flash_Buff[pc]);
							}
							else
							{
								Send_Project_Adr(0, 0x00000000, 0x00);
							}
						}
						else
						{
							Send_Error_Order(0x05); 												// 数据域格式不对应
						}
						break; 
						
					case 0x0C:	//修改主机名，获取下位机电量
						if(USB_Rx_Buff[5]==0x01)													// 判断数据域格式是否对应
						{
							Send_Namebattery(bri_battery, bri_name);
						}
						else if(USB_Rx_Buff[5]==0x02)
						{
							for(i=0;i<64;i++) bri_name[i]=0;											
							bri_name_len = USB_Rx_Buff[3]+0xFF*USB_Rx_Buff[4]-1;
							if(bri_name_len>60) bri_name_len =60;
							for(i=0;i<bri_name_len;i++) bri_name[i]=USB_Rx_Buff[6+i];	
							bri_name[i] = 0x00;															// 末尾加0x00
							W25QXX_Write((u8 *)bri_name, bri_name_ADDR,64);
							Send_Namebattery(bri_battery, bri_name);
							for(i=0;i<strlen((char *)bri_name);i++) device_name[i]=bri_name[i];														
//							for(j=0;i<strlen((char *)device_addr);j++) device_name[i+j]=device_addr[j];												
							W25QXX_Write( (u8*)&device_name,device_name_ADDR,sizeof(device_name) );
							Bluetooth_rename(bri_name);											// 蓝牙重命名		  
							LCD_flag = 0; 																	// UI复位
							if(tab_flag.MainTab_choice == 1)
							{
								tab_flag.Tab_Level = 1;												// 选择子界面
								tab_flag.update_flag = 1;
							}
							else
							{
								tab_flag.Tab_Level = 0;												// 选择主界面
								tab_flag.update_flag = 1;
							}
						}
						else
						{
							Send_Error_Order(0x05); 												// 数据域格式不对应
						}
						break;
					case 0x11:	//写入钢琴文件到外部FLASH
						if(PianoToFlash(USB_Rx_Buff)==0)
						{
							Send_PianoToFlash(USB_Rx_Buff[5], USB_Rx_Buff[6]+USB_Rx_Buff[7]*256+USB_Rx_Buff[8]*256*256, 0x00);
						}
						else
						{
							Send_PianoToFlash(USB_Rx_Buff[5], USB_Rx_Buff[6]+USB_Rx_Buff[7]*256+USB_Rx_Buff[8]*256*256, 0x01);
						}
						break;
					case 0x12:	//写入字库文件到外部FLASH
						if(FontToFlash(USB_Rx_Buff)==0)
						{
							Send_FontToFlash(USB_Rx_Buff[5], USB_Rx_Buff[6]+USB_Rx_Buff[7]*256+USB_Rx_Buff[8]*256*256, 0x00);
						}
						else
						{
							Send_FontToFlash(USB_Rx_Buff[5], USB_Rx_Buff[6]+USB_Rx_Buff[7]*256+USB_Rx_Buff[8]*256*256, 0x01);
						}
						break;
					case 0x13:	//获取UUID
						Send_uuid();
						break;
					case 0x14:	//激活安全码
						if(KeyToFlash(USB_Rx_Buff)==0)
						{
							Send_KeyToFlash( 0x00);
						}
						else
						{
							Send_KeyToFlash(0x01);
						}
						break;
					default: Send_Error_Order(0x04);										// 无效功能码
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
		if(flashtimeout)
		{
			Send_Program_Download(0x02,0x02);												// 包接收超时
			flashtimeout = 0;
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
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
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
	myfree(data_to_send);																	// 释放内存
}
//================================================================

//================================================================
//功能：发送传感器数据函数
//参数：Channel_State
//			Connection_N 为级联层数，1-4
//返回：无
//================================================================
void Send_Sensor_Data(u8 Connection_N)   								//需修改！！！！
{
		int _cnt = 0;
		u8 i,j;
		u8 *data_to_send,jiaoyanma = 0x00;
		
		data_to_send = mymalloc(HID_IN_PACKET*3);						// 申请内存
		memset(data_to_send,0,HID_IN_PACKET*3);
		data_to_send[_cnt++]=0x68;
		data_to_send[_cnt++]=0x21;
		data_to_send[_cnt++]=0x02;
		data_to_send[_cnt++]=0x21;
		data_to_send[_cnt++]=0x00;
		
		data_to_send[_cnt++]=Connection_N;
		
		for(i=0;i<8*(Connection_N+1);i++)
		{
			data_to_send[_cnt++] = 0;
			for(j=0;j<3;j++)
			{
				data_to_send[_cnt++] = Channel_State[i][j];
			}
		}
				
		for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
		data_to_send[_cnt++]=jiaoyanma;
		
		data_to_send[_cnt++]=0x16;
		
		HID_SendBuff(data_to_send,((_cnt-1)/HID_IN_PACKET+1)*HID_IN_PACKET);						
		myfree(data_to_send);																	// 释放内存
}
//================================================================

//================================================================
//功能：发送下位机系统资源
//参数：无
//返回：无
//================================================================
void Send_System_Source(void)   
{
	int _cnt,i;
	u32 total,free;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x03;
	data_to_send[_cnt++]=0x04;	
	data_to_send[_cnt++]=0x00;
		
	free = mf_showfree((u8 *)"1:",&total);								// 查询文件系统剩余空间 6821030400 806E 806E 6C16
	data_to_send[_cnt++] = (u8)(total-4*1024);
	data_to_send[_cnt++] = (u8)((total-4*1024)>>8);	
	data_to_send[_cnt++] = (u8)(free-4*1024);
	data_to_send[_cnt++] = (u8)((free-4*1024)>>8);
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	myfree(data_to_send);																	// 释放内存
}
//================================================================

//================================================================
//功能：程序下载返回函数
//参数：mode:
//			flag:
//返回：无
//================================================================
void Send_Program_Download(u8 mode, u8 flag)   
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x04;
	data_to_send[_cnt++]=0x02;	
	data_to_send[_cnt++]=0x00;		
	data_to_send[_cnt++]=mode;
	data_to_send[_cnt++]=flag;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;

	HID_SendBuff(data_to_send,HID_IN_PACKET);	
	myfree(data_to_send);																	// 释放内存
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
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
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
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：钢琴文件下载返回函数
//参数：Num:编号
//			Offset:偏移地址
//			flag:执行结果
//返回：无
//================================================================
void Send_PianoToFlash(u8 Num, u32 Offset, u8 flag)
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x11;
	data_to_send[_cnt++]=0x05;	
	data_to_send[_cnt++]=0x00;
		
	data_to_send[_cnt++]=Num;
	data_to_send[_cnt++]=Offset;
	data_to_send[_cnt++]=Offset>>8;
	data_to_send[_cnt++]=Offset>>16;
	data_to_send[_cnt++]=flag;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：字库文件下载返回函数
//参数：Num:编号
//			Offset:偏移地址
//			flag:执行结果
//返回：无
//================================================================
void Send_FontToFlash(u8 Num, u32 Offset, u8 flag)
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x12;
	data_to_send[_cnt++]=0x05;	
	data_to_send[_cnt++]=0x00;
		
	data_to_send[_cnt++]=Num;
	data_to_send[_cnt++]=Offset;
	data_to_send[_cnt++]=Offset>>8;
	data_to_send[_cnt++]=Offset>>16;
	data_to_send[_cnt++]=flag;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：发送uuid到PC
//参数：无
//返回：无
//================================================================
void Send_uuid(void)   
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	u8 buff[25];
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存 
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x13;
	data_to_send[_cnt++]=0x18;	
	data_to_send[_cnt++]=0x00;
		
	sprintf((char *)buff,"%08X%08X%08X",*(vu32*)(0x1FFF7A10),*(vu32*)(0x1FFF7A14),*(vu32*)(0x1FFF7A18));
	for(i=0;i<24;i++) data_to_send[_cnt++] = buff[i];
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;	
	data_to_send[_cnt++]=0x16;
	
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	myfree(data_to_send);																	// 释放内存
}
//================================================================

//================================================================
//功能：写激活码返回函数
//参数：flag:执行结果
//返回：无
//================================================================
void Send_KeyToFlash(u8 flag)
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;									
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x14;
	data_to_send[_cnt++]=0x01;	
	data_to_send[_cnt++]=0x00;
	data_to_send[_cnt++]=flag;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：波形上传函数
//参数：Channel_State:
//			lay: 
//			port: 
//			mode:
//返回：无
//================================================================
void Send_Wave(u8 lay,u8 port,u8 id, u8 mode)   				//n代表数据组数		//需修改！！！！
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x06;
	data_to_send[_cnt++]=0x0E;	
	data_to_send[_cnt++]=0x00;
		
	if(Channel_State[port-1][0]==port && Channel_State[port-1][1]==id && Channel_State[port-1][2]==mode)
	{																											// id、mode匹配
		data_to_send[_cnt++] = lay;
		for(i=0;i<13;i++) data_to_send[_cnt++] = Channel_State[port-1][i];
	}
	else
	{
		for(i=0;i<14;i++) data_to_send[_cnt++] = 0xff;
	}
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：文件系统反馈
//参数：mode:
//			state
//返回：无
//================================================================
void Send_FATFS_State(uint8_t mode, uint8_t state)
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x09;
	data_to_send[_cnt++]=0x02;
	data_to_send[_cnt++]=0x00;
	
	data_to_send[_cnt++]=mode;
	data_to_send[_cnt++]=state;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	
	data_to_send[_cnt++]=0x16;
	
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：文件路径反馈(无任何文件)
//参数：无
//返回：无
//================================================================
void Send_FATFS_PathNone(void)
{
	u8 *data_to_send;
	u8 jiaoyanma = 0,i,_cnt = 0;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	data_to_send[_cnt++]=0x68; 
	data_to_send[_cnt++]=0x21; 
	data_to_send[_cnt++]=0x09; 
	
	data_to_send[_cnt++]=0x02; 
	data_to_send[_cnt++]=0x00;
	data_to_send[_cnt++]=0x03;
	data_to_send[_cnt++]=0x00;														// 文件总数为0
	
	for(i=0;i<_cnt;i++)jiaoyanma += data_to_send[i];			
	data_to_send[_cnt++]=jiaoyanma;
	data_to_send[_cnt++]=0x16;
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存		
}
//================================================================
//功能：文件路径反馈(所有文件信息)
//参数：无
//返回：无
//================================================================
void Send_FATFS_PathAll(void)
{
	uint8_t *p,*name_str;
	unsigned int i,j;
	unsigned int data_len = 0,index = 6;
	u8 *data_to_send,jiaoyanma,name_len;
	float len;
		
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	for(i=0; i<filelist_index; i++)
	{
		if(allfilesize[i]<1024)
		{
			len = 1;																					// 不足1K按照当成1K
		}
		else
		{
			len = (u8)(allfilesize[i]/1024);									// 文件大小以K为单位
		}
		
		p=(uint8_t *)(&len);
		name_str = filelist[i];
		name_len = (strlen((char *)name_str)-2);						// 去掉“1：”
		
		data_len += (name_len+7);
		
		data_to_send[index++] = filelist_index;
		data_to_send[index++] = i+1;
		data_to_send[index++] = *(p);
		data_to_send[index++] = *(p+1);
		data_to_send[index++] = *(p+2);
		data_to_send[index++] = *(p+3);
			
		for(j=0; j<name_len; j++)
		{
			data_to_send[index++]=name_str[2+j];							// 去掉“1：”
		}
		
		data_to_send[index++] = 0x00;												// 字符串后面加0
		
		if(index>(HID_IN_PACKET-50))												// 满一包数据了
		{
			data_to_send[0]=0x68; data_to_send[1]=0x21; data_to_send[2]=0x09; 
			data_len += 1;																		// 把data_to_send[5]包含进去
			data_to_send[3]=data_len%256;
			data_to_send[4]=data_len/256;
			data_to_send[5]=0x03;
			
			jiaoyanma = 0;																		// 计算校验码
			for(j=0; j<index; j++) jiaoyanma += data_to_send[j];
			data_to_send[index++]=jiaoyanma;
			data_to_send[index++]=0x16;
			HID_SendBuff(data_to_send,HID_IN_PACKET);					// 发送一包数据
			memset(data_to_send,0,HID_IN_PACKET);
			delayms(50);																			// 延时，以便上位机处理
			data_len = 0;index = 6;
		}
	}
	
	if(index > 6)																					// 发送余下的数据
	{
		data_to_send[0]=0x68; data_to_send[1]=0x21; data_to_send[2]=0x09; 
		data_len += 1;																			// 把data_to_send[5]包含进去
		data_to_send[3]=data_len%256;
		data_to_send[4]=data_len/256;
		data_to_send[5]=0x03;
		
		jiaoyanma = 0;																			// 计算校验码
		for(j=0; j<index; j++) jiaoyanma += data_to_send[j];
		data_to_send[index++]=jiaoyanma;
		data_to_send[index++]=0x16;
		HID_SendBuff(data_to_send,HID_IN_PACKET);						// 发送一包数据
		memset(data_to_send,0,HID_IN_PACKET);
	}
	
	myfree(data_to_send);																	// 释放内存	
}	
//================================================================
/*
void Send_FATFS_Path(uint8_t totalframe, u8 nowframe,  uint8_t  buff[],float len)
{
	uint8_t *p;
	int _cnt;
	unsigned int i;
	u8 *data_to_send,jiaoyanma,LengthL,LengthH,name_len;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	p=(uint8_t *)(&len);
	
	name_len = (strlen((char *)buff)-2);								// 去掉“1：”
	LengthL = (name_len+7)%256;
	LengthH = (name_len+7)/256;
	
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x09;
	data_to_send[_cnt++]=LengthL;
	data_to_send[_cnt++]=LengthH;
	
	data_to_send[_cnt++]=0x03;
	
	data_to_send[_cnt++]=totalframe;
	data_to_send[_cnt++]=nowframe;
	
	data_to_send[_cnt++] = *(p);
	data_to_send[_cnt++] = *(p+1);
	data_to_send[_cnt++] = *(p+2);
	data_to_send[_cnt++] = *(p+3);
	
	for(i=0; i<name_len; i++)
	{
		data_to_send[_cnt++]=buff[2+i];										// 去掉“1：”
	}
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	
	data_to_send[_cnt++]=0x16;
	
	HID_SendBuff(data_to_send,((_cnt-1)/HID_IN_PACKET+1)*HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================
*/
//================================================================
//功能：文件内容反馈
//参数：totalframe:
//			nowframe:
//			buff:
//			len:
//返回：无
//================================================================
void Send_FATFS_File(uint8_t totalframe, u8 nowframe,  uint8_t  buff[],unsigned int len)
{
	int _cnt;
	unsigned int i;
	u8 *data_to_send,jiaoyanma,LengthL,LengthH;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	LengthL = (len+3)%256;
	LengthH = (len+3)/256;
	
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x09;
	data_to_send[_cnt++]=LengthL;
	data_to_send[_cnt++]=LengthH;
	
	data_to_send[_cnt++]=0x04;
	data_to_send[_cnt++]=totalframe;
	data_to_send[_cnt++]=nowframe;
		
	for(i=0; i<len; i++)
	{
		data_to_send[_cnt++]=buff[i];
	}
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	data_to_send[_cnt++]=0x16;
	
	for(i=0; i<(_cnt-1)/HID_IN_PACKET+1; i++)
	{
		HID_SendBuff(data_to_send+HID_IN_PACKET*i,HID_IN_PACKET);
	}
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：下位机启动、关闭
//参数：state:
//返回：无
//================================================================
void Send_Project_State(uint8_t state)
{
	int _cnt,i;
	u8 *data_to_send,jiaoyanma;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	_cnt = 0;jiaoyanma = 0x00;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x0A;
	data_to_send[_cnt++]=0x01;
	data_to_send[_cnt++]=0x00;
	
	data_to_send[_cnt++]=state;
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	
	data_to_send[_cnt++]=0x16;
	
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：返回程序运行地址
//参数：flag:
//			pc:
//			instruct:
//返回：无
//================================================================
void Send_Project_Adr(uint8_t flag, uint32_t pc, uint8_t instruct)
{
	int _cnt = 0,i;
	u8 *data_to_send,jiaoyanma = 0;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x0B;
	data_to_send[_cnt++]=0x06;
	data_to_send[_cnt++]=0x00;
	if(flag == 1)
	{
		data_to_send[_cnt++]=0x01;
		data_to_send[_cnt++]= (uint8_t) ( pc & 0xFF000000) >> 8*3;
		data_to_send[_cnt++]= (uint8_t) ( pc & 0x00FF0000) >> 8*2;
		data_to_send[_cnt++]= (uint8_t) ( pc & 0x0000FF00) >> 8;
		data_to_send[_cnt++]= (uint8_t) ( pc & 0x000000FF);
		data_to_send[_cnt++]=instruct;
	}else
	{
		data_to_send[_cnt++]=0x00;
		data_to_send[_cnt++]=0x00;
		data_to_send[_cnt++]=0x00;
		data_to_send[_cnt++]=0x00;
		data_to_send[_cnt++]=0x00;
		data_to_send[_cnt++]=0x00;
	}
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	
	data_to_send[_cnt++]=0x16;
	
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：发送主机名称和电量信息
//参数：bri_battery:电量值
//			bri_name:主机名称
//返回：无
//================================================================
void Send_Namebattery(uint8_t bri_battery, char bri_name[])
{
	int _cnt=0,i;
	u8 *data_to_send,jiaoyanma=0,LengthL=0,LengthH=0;

	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	LengthL = (bri_name_len+1)%256;
	LengthH = (bri_name_len+1)/256;
	
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x0C;
	data_to_send[_cnt++]=LengthL;
	data_to_send[_cnt++]=LengthH;
	
	data_to_send[_cnt++]=bri_battery;
	for(i=0; i<bri_name_len; i++)
	{
		data_to_send[_cnt++]=bri_name[i];
	}
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	data_to_send[_cnt++]=0x16;
	
	for(i=0; i<(_cnt-1)/HID_IN_PACKET+1; i++)
	{
		HID_SendBuff(data_to_send+HID_IN_PACKET*i,HID_IN_PACKET);
	}
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：RAM地址内容查询
//参数：Address0~Address4:地址信息
//			Length:长度
//返回：无
//================================================================
void GetRamData(u8 Address0,u8 Address1,u8 Address2,u8 Address3,u8 Length)
{
	int _cnt = 0;
	u16 i,DataLength;
	u8 *data_to_send,jiaoyanma = 0x00;
	
	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	DataLength = 5+Length;
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x07;
	data_to_send[_cnt++]=DataLength%256;
	data_to_send[_cnt++]=DataLength/256;
	
	data_to_send[_cnt++]=Address0;
	data_to_send[_cnt++]=Address1;
	data_to_send[_cnt++]=Address2;
	data_to_send[_cnt++]=Address3;
	data_to_send[_cnt++]=Length;
	for(i=0;i<Length;i++)																	//0x00---0xff
	{
		data_to_send[_cnt++]=GetByte(GetRamBase(Address0,Address1,Address2,Address3)+i);
	}
	
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
	data_to_send[_cnt++]=0x16;
	
	for(i=0; i<(_cnt-1)/HID_IN_PACKET+1; i++)
	{
		HID_SendBuff(data_to_send+HID_IN_PACKET*i,HID_IN_PACKET);
	}
	
	myfree(data_to_send);																	// 释放内存	
}
//================================================================

//================================================================
//功能：电机控制反馈
//参数：无
//返回：无
//================================================================	
void Send_MotorControl(void)   
{
	int _cnt = 0,i;
	u8 *data_to_send,jiaoyanma = 0;

	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
	data_to_send[_cnt++]=0x68;
	data_to_send[_cnt++]=0x21;
	data_to_send[_cnt++]=0x08;
	data_to_send[_cnt++]=0x00;	
	data_to_send[_cnt++]=0x00;
		
	for(i=0;i<_cnt;i++)	jiaoyanma += data_to_send[i];
	data_to_send[_cnt++]=jiaoyanma;
		
	data_to_send[_cnt++]=0x16;
		
	HID_SendBuff(data_to_send,HID_IN_PACKET);
	
	myfree(data_to_send);																	// 释放内存	
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
	u8 *data_to_send,jiaoyanma = 0;

	data_to_send = mymalloc(HID_IN_PACKET);								// 申请内存
	memset(data_to_send,0,HID_IN_PACKET);
	
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
	myfree(data_to_send);																	// 释放内存		
}
//================================================================

//================================================================
//功能：程序下载模式(写文件)
//参数：Frame_Buff:
//			Frame_Buff_Length:
//			flash_buff:
//			flash_length:
//返回：无
//================================================================
uint8_t ProgramToFile(uint8_t Frame_Buff[], char *file)
{
	u32 count=0;
	static u32 offset = 0; 
	
	if(!Frame_Buff[7])																			// 首帧
	{
		offset = 0;
		Flash_TimePic = 800000;																// 包接收时间为800ms
		flashbuff_start = 1; 
		f_open(&file1, (const TCHAR*)file,FA_CREATE_ALWAYS | FA_WRITE);	
	}
	
	if(flashbuff_start)
	{
		count = Frame_Buff[3]+256*Frame_Buff[4]-3;

		f_lseek(&file1,offset);															// 定位到文件末尾
		res_flash=f_write(&file1,&Frame_Buff[8],count,&fnum);
		
		if(Frame_Buff[7]>=Frame_Buff[6]-1)										// 尾帧
		{
			f_close(&file1);
			flashbuff_start = 0;																// 本次program接收完成，允许接收新的program
			return 1;	
		}
		if(res_flash == FR_OK )
		{
			offset += count;
			return 2;
		}
		else
		{
			return 0;
		}
	}
	
	return 0;
}
/*
uint8_t ProgramToFlash(uint8_t Frame_Buff[],uint8_t flash_buff[], unsigned int *flash_length, char *file)
{
	int i;
	u32 count=0;

	if(!Frame_Buff[7])																			// 首帧
	{
		count=0;
		*flash_length = 0;
		Flash_TimePic = 800000;																// 包接收时间为800ms
		flashbuff_start = 1; 

		f_open(&file1, (const TCHAR*)file,FA_CREATE_ALWAYS | FA_WRITE);	
	}
	
	if(flashbuff_start)
	{
		count = Frame_Buff[3]+256*Frame_Buff[4]-3;

		if(Frame_Buff[7]>=Frame_Buff[6]-1)										// 尾帧
		{
			for(i=0; i<count; i++)
			{
				Flash_Buff[(*flash_length)+i] = Frame_Buff[8+i];
			}
			(*flash_length) += count;
			res_flash=f_write(&file1,Flash_Buff,(*flash_length),&fnum);
			f_close(&file1);
						
			for(i=0; i<(*flash_length); i++)
			{
				Flash_Buff[i] = 0;																// 用完buffer清零
			}
			
			flashbuff_start = 0;																// 本次program接收完成，允许接收新的program
			return 1;																						// program接收完成
		}
		else
		{
			for(i=0; i<count; i++)
			{
				Flash_Buff[(*flash_length)+i] = Frame_Buff[8+i];
			}
			(*flash_length) += count;
			return 2;
		}
	}
	return 0;
}*/
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
	
	IWDG_Feed();																								// 喂狗
	frame_index = Buff[7]+Buff[8]*256;													// 计算当前帧编号
	frame_total = Buff[5]+Buff[6]*256;													// 计算总帧数
		
	if(frame_index==0 && updateflash_start != 1)																			
	{								
		if(Buff[9]=='A' && Buff[10]=='P' && Buff[11]=='P'&& Buff[12]=='=')
		{	
			*Total_Length = 0;
			UpdateFlash_TimePic = 400000;														// 包接收时间为400s
	
			if(programflag) EndProgram();														// 先结束正在运行的程序
			LCD_clear();
			LCD_Display_pic(0, 12, 180, 128, DOWNLOAD_FIRMWARE);
			LCD_refresh();
			FLASH_Unlock();																					// 解锁FLASH,为写入做准备
			IWDG_Feed();																						// 擦除散区
			FLASH_EraseSector(FLASH_Sector_8, VoltageRange_3);
			IWDG_Feed(); 	
			FLASH_EraseSector(FLASH_Sector_9, VoltageRange_3);
			IWDG_Feed();	
			FLASH_EraseSector(FLASH_Sector_10, VoltageRange_3);
			IWDG_Feed();	
			FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);
			IWDG_Feed();	
			
			tab_flag.update_flag = 1;
			tab_flag.Tab_Level = 4;
			
			Update_Count_Adderss = UPDATE_ADDRESS;
			frame_now = 0xFFFF;
			updateflash_start = 1;
		}
		else
		{
			Show_Str(20,83,148,32,"固件内容错误  ",16,0);			// 显示接收错误信息
//			printf("\r\nAPP=%02X,%02X,%02X,%02X",Buff[9],Buff[10],Buff[11],Buff[12]);
			return 0;
		}
	}
	//printf("\r\n帧：%d/%d",frame_index,frame_total);
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
				Show_Str(20,83,148,32,"数据校验错误  ",16,0);					// 显示接收错误信息
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
			fupd_prog(130,84,16,frame_total,frame_total);	
			LCD_refresh();			
			return 1;	
		}
		else
		{ 
			return 2;																								// 当前数据包处理成功		
		}
	}
	
	Show_Str(20,83,148,32,"固件下载失败  ",16,0);	
//	printf("\r\n未开始固件下载");		
	return 0;
}
//================================================================

//================================================================
//功能：写钢琴文件
//参数：Buff:数据指针
//返回：0=错误,1=正确
//================================================================
uint8_t PianoToFlash(uint8_t *Buff)
{
	u8 Num = 0;
	u8 *write;
	u8 *read;
	u16 i = 0;
	u16 len = 0;
	u32 Offset = 0;
	u32 Addr = 0;
	
	len = Buff[3] + 256*Buff[4] - 4;
	Num  = Buff[5];
	Offset = Buff[6] + 256*Buff[7] + 256*256*Buff[8];
	
	if(len > 1024 || Offset>7*1024) 
	{
//		printf("\r\n长度不对:%d,%d",len,Offset);
		return 1;
	}
	read = mymalloc(len);																				// 分配内存														
	Addr = 1024*(1024*28+(3328+7*Num)) + Offset;
	write = Buff+9;
	W25QXX_Write(write,Addr,len);																// 写FLASH
	W25QXX_Read(read,Addr,len);														
	for(i=0;i<len;i++)
	{
		if(read[i] != write[i])																		// 回读并校验
		{
			myfree(read);																						// 释放内存
			return 0;
		}
	}
	myfree(read);																								// 释放内存
	
	if((Offset+len)>=PIANO_LEN[Num])														// 下载完成
	{
		piano_num = Num+1;																				// 播放相应的钢琴音
		piano_Buff_flag = 1;
		tab_flag.sound_flag = 0;
		downloadsound_flag = 0;
	}
	if(Num >= 36)																								// 下载到最后一个时停止播放
	{
		piano_num = 0;
		piano_Buff_flag = 0;
	}
	return 1;
}
//================================================================

//================================================================
//功能：写字库文件
//参数：Buff:数据指针
//返回：0=错误,1=正确
//================================================================
uint8_t FontToFlash(uint8_t *Buff)
{
	static u8 flag = 0;
	u8 Num = 0;
	u8 *write;
	u8 *read;
	u16 i = 0;
	u16 len = 0;
	u32 Offset = 0;
	u32 Addr = 0;
	u32 flashaddr=0;			
	
	len = Buff[3] + 256*Buff[4] - 4;
	Num  = Buff[5];
	Offset = Buff[6] + 256*Buff[7] + 256*256*Buff[8];
	
	if(len > 1024 || Offset>1684*1024) 
	{
		return 1;
	}
	
	ftinfo.ugbkaddr=FONTINFOADDR+sizeof(ftinfo);								// 信息头之后，紧跟UNIGBK转换码表
	ftinfo.ugbksize=UNIGBK;																			// UNIGBK大小
	
	ftinfo.f12addr=ftinfo.ugbkaddr+ftinfo.ugbksize;							// UNIGBK之后，紧跟GBK12字库
	ftinfo.gbk12size=GBK12_FONSIZE;															// GBK12字库大小
	
	ftinfo.f16addr=ftinfo.f12addr+ftinfo.gbk12size;							// GBK12之后，紧跟GBK16字库
	ftinfo.gbk16size=GBK16_FONSIZE;															// GBK16字库大小
	
	ftinfo.f24addr=ftinfo.f16addr+ftinfo.gbk16size;							// GBK16之后，紧跟GBK24字库
	ftinfo.gkb24size=GBK24_FONSIZE;															// GBK24字库大小
	
	switch(Num)
	{
		case 0:																										// 更新UNIGBK.BIN			
			flashaddr=ftinfo.ugbkaddr;
			flag |= 0x01;
			break;
		case 1:			
			flashaddr=ftinfo.f12addr;																// GBK12的起始地址
			flag |= 0x02;
			break;
		case 2:			
			flashaddr=ftinfo.f16addr;																// GBK16的起始地址
			flag |= 0x04;
			break;
		case 3:			
			flashaddr=ftinfo.f24addr;																// GBK24的起始地址
			flag |= 0x08;
			break;
		default:
			break;
	} 
		
	read = mymalloc(len);																				// 分配内存														
	Addr = flashaddr + Offset;
	write = Buff+9;
	W25QXX_Write(write,Addr,len);																// 写FLASH
	W25QXX_Read(read,Addr,len);														
	for(i=0;i<len;i++)
	{
		if(read[i] != write[i])																		// 回读并校验
		{
			myfree(read);																						// 释放内存
			flag = 0;
			return 0;
		}
	}
	myfree(read);																								// 释放内存
	
	if(flag == 0x0f)
	{
		if((Offset + len) >= 1723680)															// 表示成功升级完成
		{
			ftinfo.fontok=0XAA;
			W25QXX_Write((u8*)&ftinfo,FONTINFOADDR,sizeof(ftinfo));	// 保存字库信息
			font_test();
			LCD_refresh();
			tab_flag.sound_flag = 0;
			downloadsound_flag = 1;
		}
	}
	return 1;
}
//================================================================

//================================================================
//功能：写激活码到Flash
//参数：Buff:数据指针
//返回：0=错误,1=正确
//================================================================
uint8_t KeyToFlash(uint8_t *Buff)
{			
	uint8_t i;	
	uint8_t	read[16];
	uint8_t *pt = Buff+5;
	
	W25QXX_Write(pt,key_ADDR,16);																// 2ECA2ED7 628ED259766C04 57 38AC994D
	W25QXX_Read(read,key_ADDR,16);	
	for(i=0;i<16;i++)
	{
		if(read[i] != Buff[i+5])																	// 回读并校验
		{
			return 0;
		}
	}
	check_app_key();																						// 重新检测激活码
	return 1;
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
	if(!USB_TimePic&&data_from_blue_start)
	{
		data_from_blue_start = 0;																	// 允许重新接收数据
		data_from_blue_Length = 0;
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

//================================================================
//功能：运行程序
//参数：filename:程序路径
//返回：无
//================================================================
void EnterProgram(u8 *filename)
{
	int i=0;

	strcpy(now_programm_path,(char *)filename);
	res_flash = f_open(&file1,(const TCHAR*)filename, FA_OPEN_EXISTING | FA_READ);

	if(res_flash == FR_OK)
	{
		memset(Flash_Buff,0,sizeof(Flash_Buff));
		res_flash = f_read(&file1, Flash_Buff,  file1.fsize, &fnum); 

		if(res_flash==FR_OK)
		{		
			programflag = 1;																				// 进入程序
			UIRUN_flag  = 1;
			BriLight_flag = 1;			
			tab_flag.Tab_Level = 3;																	// UI更新
			tab_flag.update_flag = 1;
		
			for(i=0;i<4;i++)																				// 电机端口脉冲复位
			{
				SumPluse[i]=0;
				SumPluseForDisplay[i] = 0;
				motor_brake_angle[i] = 0;
				motor_goal_angle[i] = 0;
			}
		
			memset(RAM_Buff,0,sizeof(RAM_Buff));										// RAM初始化
						
			for(i=0;i<ThreadSize;i++)																// 程序线程初始化
			{
				Thread[i].state			= 0;
				Thread[i].Threadpc	= 0x00000000;
				Thread[i].timetmp		= 0;
			}
			Thread[0].state = 1;
																										
			ProgramTime = 0;																				// 程序时间初始化		
			for(i=0; i<8; i++) FunTimer[i].time = 0;
					
			key_flag=0;																							// 按键特殊处理清零
			for(i=0; i<4; i++) touch_flag[i] = 0x00;
		}

		f_close(&file1);	
	}
}
//================================================================

//================================================================
//功能：结束程序
//参数：无
//返回：无
//================================================================
void EndProgram(void)
{
	int i,j;
	
	programflag = 0;																						// 退出程序
	BriLight_flag = 0;
	pc = 0x00000000;

	Send_Project_Adr(0, 0x00000000, 0x00);											// 向上位机发送程序运行地址
  
	LCD_flag = 0;																								// UI复位
	if(tab_flag.MainTab_choice == 1)
	{
		tab_flag.Tab_Level = 1;																		// 选择子界面
		tab_flag.update_flag = 1;
	}
	else
	{
		tab_flag.Tab_Level = 0;																		// 选择主界面
		tab_flag.update_flag = 1;
	}
		
	for(i=0;i<4;i++)																						// 电机复位
	{
		motor_state[i]  = 0;
		Motor_Invert[i] = 0;
	}
	
	if(SOUND_Buff_flag == 1)
	{
		f_close(&file3);
	}
	
	piano_Buff_flag = 0;																				// 结束声音播放
	SOUND_Buff_flag = 0;
	
	program_sleepflag = 0;																			// 程序运行按时休眠
	BriLight_flag = 0;
	Bributton_value = 0;
	
	for(i=0;i<4;i++)		
	{
		for(j=0;j<8;j++) cascade_funt_send[i][j] = 0;
	}
	Cascade_Function.allflag = 0;
	
	SOUND_UI=0;
	SOUND_Buff_index = 0;
	SOUND_Buff_flag = 0;
	piano_Buff_flag = 0;
	piano_index = 0;
	
	PICTURE_Buff[0]=0;
	
	for(i=0;i<4;i++)																						// 电机端口脉冲复位
	{
		SumPluse[i]=0;
		SumPluseForDisplay[i] = 0;
		motor_brake_angle[i] = 0;
		motor_goal_angle[i] = 0;
	}

	memset(RAM_Buff,0,sizeof(RAM_Buff));												// RAM初始化
	memset(Flash_Buff,0,sizeof(Flash_Buff));
	
	for(i=0;i<ThreadSize;i++)																		// 程序线程初始化
	{
		Thread[i].state			= 0;
		Thread[i].Threadpc	= 0x00000000;
		Thread[i].timetmp		= 0;
	}
	Thread[0].state = 1;
																		
	ProgramTime = 0;																						// 程序时间初始化				
	for(i=0; i<8; i++) FunTimer[i].time = 0;
	VS1003_SetVol(sound_volume);
	
	key_flag=0;																									// 按键特殊处理清零
	for(i=0; i<4; i++) touch_flag[i] = 0x00;
}
//================================================================


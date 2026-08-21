#ifndef __C_PROTOCOL_H
#define __C_PROTOCOL_H

#include "sys.h"
#include "usart6.h"
#include "usbd_hid_core.h"
#include "flash.h"
#include "iap.h"

extern uint8_t USB_Rx_Buff[1024];
extern uint8_t Receive_start;
extern uint8_t Receive_end;

extern unsigned int USB_Rx_Length;
extern unsigned int USB_TimePic;
extern uint8_t errorflag;
extern uint8_t Channel_State[32][13];
extern uint8_t UserWareData[];	
extern u8 usbmasterflag;
extern int usbmastertimems;

extern uint8_t updateflash_start;				//包接收标志位 

extern void CheckOrder(void);
extern void FrameOrFlashBuffLoseCheck(void);


#endif

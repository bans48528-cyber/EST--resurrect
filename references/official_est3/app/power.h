#ifndef __POWER_H
#define __POWER_H	 

#include "sys.h" 

#define Power_On()			GPIO_SetBits(GPIOE,GPIO_Pin_2)
#define Power_Off()			GPIO_ResetBits(GPIOE,GPIO_Pin_2)

extern void POWER_Init(void);

#endif


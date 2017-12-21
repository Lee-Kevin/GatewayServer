#ifndef _DEVICE_STATUS_H
#define _DEVICE_STATUS_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "msgqueue.h"


extern uint8_t CheckDevicestatusErrFlag;
void CheckDevicestatusInit(char *dbname);
uint8_t CheckDevStatusfromDatabase(char *PrdId, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* _DEVICE_STATUS_H */
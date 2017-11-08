#ifndef _DEVICE_STATUS_H
#define _DEVICE_STATUS_H

#ifdef __cplusplus
extern "C"
{
#endif

struct myMsg  
{  
    long msgType;              // 不同的msgType代表不同消息类型
    char msgName[20]; 
	char msgDevID[20];
	uint8_t  value;
};

typedef struct myMsg myMsg_t;

extern int QueueIndex;
extern uint8_t CheckDevicestatusErrFlag;
void CheckDevicestatusInit(char *dbname);
uint8_t CheckDevStatusfromDatabase(char *PrdId, uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* _DEVICE_STATUS_H */
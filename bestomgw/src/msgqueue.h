#ifndef _MSG_QUEUE_H
#define _MSG_QUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "database.h"

#define QUEUE_READ_ALWAYS_WAIT          0  
#define QUEUE_READ_NO_WAIT              1  

#define QUEUE_TYPE_SOCKET_SEND      	1
#define QUEUE_TYPE_DATABASE_SEND    	2
#define QUEUE_TYPE_DATABASE_GETINFO     3
#define QUEUE_TYPE_DATABASE_GETSTATUS   4


struct DataBaseMsg  
{  
    long msgType;              // 不同的msgType代表不同消息类型
	uint8_t  Option_type;
	sDevlist_info_t devlist;
};

typedef struct DataBaseMsg myDataBaseMsg_t;

struct myMsg  
{  
    long msgType;              // 不同的msgType代表不同消息类型
    char msgName[20]; 
	char msgDevID[20];
	uint8_t  value;
};

typedef struct myMsg myMsg_t;

extern int DataBaseQueueIndex;
extern int QueueIndex;

void MsgQueueInit(); 
int ReadQueueMessage(int msqid, void *msgp, size_t msgsz, int msgtype, int flag);


#ifdef __cplusplus
}
#endif

#endif /* _DEVICE_STATUS_H */
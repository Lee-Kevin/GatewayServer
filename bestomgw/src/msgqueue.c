/**************************************************************************************************
 Filename:       msgqueue.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-12-11 15:10:10 
 Revision:       

 Description:    This is a file that define the funs about msgqueue

 
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "socket_client.h"
#include "ap_protocol.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "sqlite3.h"

#include "devicestatus.h"
#include "log.h"

// #define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

#define MQDATABASENAME "/"
#define MQNAME "."

#define QUEUE_RECIVE_TIMEOUT   5   // *10 uint ms

int DataBaseQueueIndex;
int QueueIndex;

struct msqid_ds msgbuf;      // 获取消息属性的结构体  每个消息队列都有一个msqod_ds结构与其关联

/* 本地函数 */

/*********************************************************************
 * @fn     CheckDevicestatusInit
 *
 * @brief  初始化消息队列，并创建数据库检查相关的线程
 *
 * @param  
 *
 * @return
 */
void MsgQueueInit() {
	key_t key;
    if ( -1 == (key = ftok(MQDATABASENAME, 1)))   // 创建key  
    {  
         perror("Create key error!\n");  
         exit (-1);  
    }
	
    if ( -1 == (DataBaseQueueIndex = msgget(key, IPC_CREAT | 0777)) )  //     创建消息队列 或打开一个已经存在的队列
    {  
        perror("message queue already exitsted!\n");  
        exit (-1);  
    }
	debug_printf("create . open database queue qid is %d!\n", DataBaseQueueIndex); 
	
	
    if ( -1 == (key = ftok(MQNAME, 1)))   // 创建key  
    {  
         perror("Create key error!\n");  
         exit (-1);  
    }
	
    if ( -1 == (QueueIndex = msgget(key, IPC_CREAT | 0777)) )  //     创建消息队列 或打开一个已经存在的队列
    {  
        perror("message queue already exitsted!\n");  
        exit (-1);  
    }
	debug_printf("create . open queue qid is %d!\n", QueueIndex); 
	
}

/*********************************************************************
 * @fn     ReadQueueMessage
 *
 * @brief  初始化消息队列，并创建数据库检查相关的线程
 *
 * @param  int msqid, void *msgp, size_t msgsz, msgtype, 	int flag
 *            queue index    msg    size        queue type    flag
 *         flag = 0 阻塞接收
 *         flag = 1 非阻塞接收
 * @return
 */

int ReadQueueMessage(int msqid, void *msgp, size_t msgsz, int msgtype, int flag) {
	int size;
	uint32_t count = 0;

	if(flag) {  // 非阻塞接收
		do {
			size = msgrcv(msqid, msgp, msgsz, msgtype, IPC_NOWAIT);
			debug_printf("\n[DBG] ReadQueueMessage The size is %d ",size);
			if (size != -1 || count>= QUEUE_RECIVE_TIMEOUT) {
				debug_printf("\n[DBG] Goint to exit the while");
				
				break;
			}
			usleep(10000);  // sleep 10ms
			count++;
		} while(1);
			
	} else { 
		size = msgrcv(msqid, msgp, msgsz, msgtype, MSG_NOERROR);
		if(-1 == size) {
			debug_printf("\n[ERR] DataWritetoDev msgrcv Something Err");
		} else {  // get the message
		}	
	}
	return size;

}

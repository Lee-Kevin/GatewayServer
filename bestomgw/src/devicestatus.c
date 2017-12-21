/**************************************************************************************************
 Filename:       devicestatus.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-11-01 13:23:33 -0700 
 Revision:       

 Description:    This is a file that can check the database and to see what device is offline

 
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
#include "database.h"
#include "devicestatus.h"
#include "log.h"
#include "msgqueue.h"

// #define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif


#define S1_PRDID  "07"
#define S2_PRDID  "05"
#define S3_PRDID  "14"
#define S4_PRDID  "01"
#define S5_PRDID  "08"
#define S6_PRDID  "80"
#define S7_PRDID  "80"
#define S8_PRDID  "18"
#define S9_PRDID  "80"
#define S10_PRDID  "c0"
#define S11_PRDID  "c1"
#define S12_PRDID  "c2"
#define S17_PRDID  "17"

#define S2_PRDID  "05"



#define MAXBUF 20;
#define CHECKTIMEINTERVAL  60         // unit s  此处数值为 60s
// #define CHECKTIMEINTERVAL  10         // unit s  此处数值为 60s

#define READ_QUEUE_TIMES   20          // 重复尝试读取20次数据，COO下面最多20个设备，以后可能会更改

extern int DataBaseQueueIndex;
extern int QueueIndex;

uint8_t CheckDevicestatusErrFlag = 1;  // 0 代表没有错误， 1 代表刚刚连接成功  2 断开

// int QueueIndex;

typedef struct __tag_devinfo{
	char prdID[3];
	uint8_t time_minute;
	uint8_t flag;
}DEV_info;

// static DEV_info g_devinfo[]={
		// {S1_PRDID, 2, 0},    
		// {S2_PRDID, 2, 0},
		// {S3_PRDID, 2, 0},
		// {S4_PRDID, 2, 0},
		// {S5_PRDID, 2, 0},
		// {S6_PRDID, 2, 0},
	    // {S7_PRDID, 2, 0},
		// {S8_PRDID, 2, 0},
		// {S9_PRDID, 2, 0},
		// {S10_PRDID, 2, 0},
		// {S11_PRDID, 2, 0},
		// {S12_PRDID, 2, 0},
	// };


static DEV_info g_devinfo[]={
		{S1_PRDID, 12, 0},    
		{S2_PRDID, 12, 0},
		{S3_PRDID, 12, 0},
		{S4_PRDID, 12, 0},
		{S5_PRDID, 12, 0},
		{S6_PRDID, 12, 0},
		// {S7_PRDID, 10, 0},
		{S8_PRDID, 2, 0},
		{S9_PRDID, 2, 0},
		{S10_PRDID, 13, 0},
		{S11_PRDID, 13, 0},
		{S12_PRDID, 13, 0},
		// {S17_PRDID, 10, 0},
		
	};

#define DEVINFO_NUM sizeof(g_devinfo)/sizeof(g_devinfo[0])


// struct msqid_ds
  // {
    // struct msqid_ds {
    // struct ipc_perm msg_perm;
    // struct msg *msg_first;      /* first message on queue,unused  */
    // struct msg *msg_last;       /* last message in queue,unused */
    // __kernel_time_t msg_stime;  /* last msgsnd time */
    // __kernel_time_t msg_rtime;  /* last msgrcv time */
    // __kernel_time_t msg_ctime;  /* last change time */
    // unsigned long  msg_lcbytes; /* Reuse junk fields for 32 bit */
    // unsigned long  msg_lqbytes; /* ditto */
    // unsigned short msg_cbytes;  /* current number of bytes on queue */
    // unsigned short msg_qnum;    /* number of messages in queue */
    // unsigned short msg_qbytes;  /* max number of bytes on queue */
    // __kernel_ipc_pid_t msg_lspid;   /* pid of last msgsnd */
    // __kernel_ipc_pid_t msg_lrpid;   /* last receive pid */
// };

struct msqid_ds msgbuf;      // 获取消息属性的结构体  每个消息队列都有一个msqod_ds结构与其关联

/* 本地函数 */
static void *CheckDevicestatus(void *arg); 
static char databaseName[30];

/*********************************************************************
 * @fn     CheckDevicestatusInit
 *
 * @brief  初始化消息队列，并创建数据库检查相关的线程
 *
 * @param  串口数据帧
 *
 * @return
 */
void CheckDevicestatusInit(char *dbname) {
	pthread_t tdCheckStatus;
	
	sprintf(databaseName,"%s",dbname);
	
	if (pthread_create(&tdCheckStatus, NULL, CheckDevicestatus, &QueueIndex))
	{
		// thread creation failed
		printf("Failed to create CheckDevicestatus thread \n");
		// return -1;
	}
	
}

void *CheckDevicestatus(void *arg) {
    uint8_t done = 0;
	static uint8_t count;
	 
    //const int qid=*((int *)arg);  
    int qid, tmpqid, err;  
	
	struct timeval temp;
	// struct timespec timeout;
	srand((int)time(0));  //设置随机数种子
	
    tmpqid = *((int *)arg);  
    qid = tmpqid;	
	debug_printf("This is CheckDevicestatus thread \n");  
	// printf("CheckDevicestatus The pid is %d \n",tmpqid);  
	
	do {
		
		temp.tv_sec = CHECKTIMEINTERVAL; // 60s
		uint32_t tempdata = (int)(1000.0*random()/(RAND_MAX+1.0));
		temp.tv_usec = 1000*tempdata;
		// debug_printf("\n[DBG] I am in the CheckDevicestatus fun\n");	
		select(0,NULL,NULL,NULL,&temp);
		count++;
		if(CheckDevicestatusErrFlag == 1) { // 刚刚连接成功
			CheckDevicestatusErrFlag = 0;
		} else if (CheckDevicestatusErrFlag == 0) { // 无错误，开启正常的设备状态检查
		
			for (uint8_t i=0; i< DEVINFO_NUM; i++) {
				if(count%g_devinfo[i].time_minute == 0) {
					CheckDevStatusfromDatabase(g_devinfo[i].prdID,i);
					// usleep(150000);
				}
			};
		} else if (CheckDevicestatusErrFlag == 2) {  // Socket 断开
			debug_printf("\n[DBG]Check Device status stop. \n");	
		}
		gettimeofday(&temp,NULL);  //
		
	} while (!done);

}


/*********************************************************************
 * @fn     CheckDevStatusfromDatabase
 *
 * @brief  从数据库里读出 设备的上次更新状态的时间，如果离线，则发送消息至消息队列
	"CREATE TABLE Devlist( \
		1			   ID  INTEGER PRIMARY KEY AUTOINCREMENT    NOT NULL,\
		2			   DeviceName         TEXT      NOT NULL, \
		3			   ProductId          TEXT      NOT NULL, \
		4			   ShortAddr          TEXT      NOT NULL, \
		5			   DeviceId           TEXT      NOT NULL, \
		6			   DeviceStatus       INT       NOT NULL, \
		7			   Power              INT,                \
		8			   Note          TEXT,  \
		9		       TIME TEXT);";
		
 *
 * @param  产品ID, 以及产品ID有关的index。d
 *
 * @return true 代表读取数据失败, 0代表读取数据成功
 */
uint8_t CheckDevStatusfromDatabase(char *PrdId, uint8_t index) {
	int 			rc;
	int 			err;
	myDataBaseMsg_t msg_database;
	myMsg_t 		msg_socket; 
	uint8_t count;
	
	memset(&msg_database,0,sizeof(myDataBaseMsg_t));
	msg_database.msgType     = QUEUE_TYPE_DATABASE_SEND;
	msg_database.Option_type = DATABASE_OPTION_SELECT;
	sprintf(msg_database.devlist.prdID,"%s",PrdId);
	// 做超时处理
	if ( -1 == (rc = (msgsnd(DataBaseQueueIndex, &msg_database, sizeof(myDataBaseMsg_t), 0))) ) {
		debug_printf("\n[ERR]Can't send the msg \n");
	} else {
		memset(&msg_database,0,sizeof(myDataBaseMsg_t));
		do {		
			err = ReadQueueMessage(DataBaseQueueIndex,&msg_database,sizeof(myDataBaseMsg_t),QUEUE_TYPE_DATABASE_GETSTATUS,QUEUE_READ_NO_WAIT);
			if (err != -1) {
				memset(&msg_socket,0,sizeof(myMsg_t));
				debug_printf("\n[DBG] Get the information from database devID: %s",msg_database.devlist.devID);
				sprintf(msg_socket.msgName,"%s",msg_database.devlist.devName);
				sprintf(msg_socket.msgDevID,"%s",msg_database.devlist.devID);
				struct tm * tmp_time = (struct tm *)malloc(sizeof(struct tm));
				
				strptime(msg_database.devlist.TimeText,"%Y-%m-%d %H:%M:%S",tmp_time);
				time_t last_time = mktime(tmp_time);
				// debug_printf("\n[DBG] The last timestamp is %ld\n",last_time);
				time_t now_time;
				time(&now_time);
				// debug_printf("\n[DBG] The now timestamp is %ld\n", now_time);				
				
				long timeInterval = now_time - last_time;
				debug_printf("\n[DBG] The timeInterval is %ld\n", timeInterval);
				
				if(timeInterval > 60 * g_devinfo[index].time_minute ) {
					debug_printf("\n[DBG] The device is offine\n");
					msg_socket.msgType = 1;
					msg_socket.value   = 0;
					if( -1 == (msgsnd(QueueIndex, &msg_socket, sizeof(myMsg_t), 0)) ){
						debug_printf("\n[ERR] CheckDevStatusfromDatabase There is something wrong with msgsnd\n");
					}
					memset(&msg_database,0,sizeof(myDataBaseMsg_t));
					msg_database.msgType         = QUEUE_TYPE_DATABASE_SEND;
					msg_database.Option_type     = DATABASE_OPTION_UPDATE;
					msg_database.devlist.status  = 0;
					sprintf(msg_database.devlist.devID,"%s",msg_socket.msgDevID);
					sprintf(msg_database.devlist.note,"%s","offline");
					if ( -1 == (msgsnd(DataBaseQueueIndex, &msg_database, sizeof(myDataBaseMsg_t), 0)) ) {
						debug_printf("\n[ERR]Can't send the msg \n");
					}
					logWarn("[Status] Offline -- prdID:%s -- devID:%s .",PrdId,msg_database.devlist.devID);
				}
			}
			if(++count >= READ_QUEUE_TIMES) {
				break;
			}
			// usleep(50000) // 50ms
			
			
			
		} while(1);
	}	
	return rc;
}
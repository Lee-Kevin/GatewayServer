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


#define __BIG_DEBUG__

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
#define S10_PRDID  "80"
#define S11_PRDID  "20"
#define S12_PRDID  "17"

#define S2_PRDID  "05"


#define MQNAME "."
#define MAXBUF 20;
#define CHECKTIMEINTERVAL  30          // unit s 

uint8_t CheckDevicestatusErrFlag = 1;  // 0 代表没有错误， 1 代表刚刚连接成功  2 断开

int QueueIndex;


typedef struct __tag_devinfo{
	char prdID[3];
	uint8_t time_minute;
	uint8_t flag;
}DEV_info;


static DEV_info g_devinfo[]={
		{S1_PRDID, 2, 0},    
		{S2_PRDID, 2, 0},
//		{S3_PRDID, 2, 0},
		{S4_PRDID, 2, 0},
		{S5_PRDID, 2, 0},
		{S6_PRDID, 2, 0},
		{S7_PRDID, 2, 0},
		{S8_PRDID, 2, 0},
		{S9_PRDID, 2, 0},
		{S10_PRDID, 2, 0},
		{S11_PRDID, 2, 0},
		{S12_PRDID, 2, 0},
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
	key_t key;
	sprintf(databaseName,"%s",dbname);
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
	printf("This is CheckDevicestatus \n");  
	printf("CheckDevicestatus The pid is %d \n",tmpqid);  
	
	do {
		
		temp.tv_sec = CHECKTIMEINTERVAL; // 10s
		uint32_t tempdata = (int)(1000.0*random()/(RAND_MAX+1.0));
		temp.tv_usec = 1000*tempdata;
		debug_printf("\n[DBG] I am in the CheckDevicestatus fun\n");	
		select(0,NULL,NULL,NULL,&temp);
		count++;
		if(CheckDevicestatusErrFlag == 1) { // 刚刚连接成功
			CheckDevicestatusErrFlag = 0;
		} else if (CheckDevicestatusErrFlag == 0) { // 无错误，开启正常的设备状态检查
		
			for (uint8_t i=0; i< DEVINFO_NUM; i++) {
				if(count%g_devinfo[i].time_minute == 0) {
					CheckDevStatusfromDatabase(g_devinfo[i].prdID,i);
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
	int rc;
	sDevlist_info_t myDevlist;
	sDevlist_info_t * devInfo = &myDevlist;
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	char sql1[255];
	myMsg_t msg; 
	uint8_t updateDataBaseFlag = 0;

	char *sql = "SELECT * FROM Devlist WHERE ProductId = ?";
	rc = sqlite3_open(databaseName, &db);  
	if(rc){  
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));   
	}else{  
		sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
		sqlite3_bind_text(stmt, 1, PrdId, -1, NULL);
		Start_Transaction(db);  // 开启事务模式
		
		while(sqlite3_step(stmt) != SQLITE_DONE) {
			int num_cols = sqlite3_column_count(stmt);
			debug_printf("\n[DBG]********************************\n");
			
			for (uint8_t i = 0; i < num_cols; i++)
			{
				if (i==1) {
					sprintf(msg.msgName,"%s",sqlite3_column_text(stmt, i));
				} else if (i==2) {
					sprintf(devInfo->prdID,"%s",sqlite3_column_text(stmt, i));
					// *devInfo.prdID = sqlite3_column_text(stmt, i);
				} else if (i==4) {     // 获得设备ID
					sprintf(devInfo->devID,"%s",sqlite3_column_text(stmt, i));
					// *devInfo.shortAddr = sqlite3_column_text(stmt, i);
					// sprintf(msg.msgName,"%s",sqlite3_column_text(stmt, i));
					sprintf(msg.msgDevID,"%s",sqlite3_column_text(stmt, i));
				} else if (i==5) {
					devInfo->status = sqlite3_column_int(stmt, i); // 模块的在线状态信息 0 Offline 1 Online
					if(!devInfo->status) {
						break; //已经离线的设备无需检查
					}
				} else if (i==8) { // 获得时间  
					struct tm * tmp_time = (struct tm *)malloc(sizeof(struct tm));
					sprintf(devInfo->TimeText,"%s",sqlite3_column_text(stmt, i));
					strptime(devInfo->TimeText,"%Y-%m-%d %H:%M:%S",tmp_time);
					time_t last_time = mktime(tmp_time);
					
					debug_printf("\n[DBG] The last timestamp is %ld\n",last_time);
					time_t now_time;
					time(&now_time);
					debug_printf("\n[DBG] The now timestamp is %ld\n", now_time);
					long timeInterval = now_time - last_time;
					debug_printf("\n[DBG] The timeInterval is %ld\n", timeInterval);
					if(timeInterval > 60 * g_devinfo[index].time_minute ) {  // If the node is offline
						char* err_msg = NULL;
						printf("\nThe device is offline\n");
						msg.msgType = 1;
						msg.value  = 0;
								/* 调用msgsnd 将数据放到消息队列中 */
						if ( -1 == (msgsnd(QueueIndex, &msg, sizeof(myMsg_t), 0)) )  
						{
						}
						devInfo->status = 0;
						
						sprintf(devInfo->note,"%s","OffLine");
						sprintf(sql1,"UPDATE Devlist SET DeviceStatus = %d, \
							TIME = datetime('now','localtime'), \
							Note = '%s' \
							WHERE DeviceId = '%s'",devInfo->status,devInfo->note,devInfo->devID);
						rc = sqlite3_exec(db, sql1, NULL, 0, &err_msg);
						if(rc != SQLITE_OK) {
							printf("SQL error:%s\n",err_msg);
							sqlite3_free(err_msg);
						} 
					}
					free(tmp_time);
					
				}
				
				switch (sqlite3_column_type(stmt, i))
				{
				case (SQLITE3_TEXT):
					printf("%d %s, ", i,sqlite3_column_text(stmt, i));
					break;
				case (SQLITE_INTEGER):
					printf("%d,%d, ",i, sqlite3_column_int(stmt, i));
					break;
				case (SQLITE_FLOAT):
					printf("%d, %g, ", i,sqlite3_column_double(stmt, i));
					break;
				default:
					break;
				}
			}
			
			debug_printf("\n[DBG] The num_cols is %d",num_cols);
		}
		End_Transaction(db);			// 关闭事务模式
		sqlite3_finalize(stmt);
		sqlite3_close(db); 
		debug_printf("[DBG] End of the CheckDevStatusfromDatabase\n");
	}
	return rc;
}


 // /*********************************************************************
 // * @fn     UpdateDevStatustoDatabase
 // *			
 // * @brief  更新子设备的在线状态
 // *
 // * @param  数据库名称, devInfo
 // *
 // * @return
 // */
// uint8_t __UpdateDevStatustoDatabase(sDevlist_info_t * devInfo) {
	
	// sDevlist_info_t _devInfo = *devInfo;
	// int rc;
    // sqlite3* db = NULL;
	// char* err_msg = NULL;
	// char sql[255];
    // rc = sqlite3_open(databaseName, &db);  
    // if(rc){  
        // fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
        // return rc;  
    // }else{  
        // fprintf(stderr, "UpdateDevStatustoDatabase Opened database successfully\n");  
    // }
	
	// sprintf(sql,"UPDATE Devlist SET DeviceStatus = %d, \
	                    // TIME = datetime('now','localtime'), \
						// Note = '%s' \
						// WHERE DeviceId = '%s'",_devInfo.status, _devInfo.note, _devInfo.devID);
	
	// rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
	
    // if(rc != SQLITE_OK) {
        // printf("SQL error:%s\n",err_msg);
        // sqlite3_free(err_msg);
    // }
	// debug_printf("[DBG] THE SQL IS %s\n",sql);
	// debug_printf("[DBG] Update data to database successfully\n");
    // sqlite3_close(db);
	// return rc;
// }

        // // qid = tmpqid;     
        // /* msgrcv竟然会在出错返回时改掉 qid 的值，加const修饰都会被改为 1  
         // * 太扯淡了，所以必须使用qidtmp保存第一次传进来的 qid值， 
         // * 以便后面消息的接收 
         // */  
		 
		// /*
		// 从消息队列中取消息
		// ssize_t msgrcv(int msqid, void *ptr, size_t nbytes, long type, int flag); 
		
		// ptr: 指向一个长整型数(返回的消息类型存放在其中), 跟随其后的是存放实际消息数据的缓冲区. 
		// nbytes: 数据缓冲区的长度. 若返回的消息大于nbytes, 且在flag中设置了MSG_NOERROR, 则该消息被截短. 
		// type: 
		// type == 0: 返回队列中的第一个消息. 
		// type > 0: 返回队列中消息类型为type的第一个消息. 
		// type < 0: 返回队列中消息类型值小于或等于type绝对值的消息, 如果这种消息有若干个, 则取类型值最小的消息. 
		
		// */
        // if (-1 == (err = msgrcv(qid, &msg, sizeof(struct myMsg), 1, IPC_NOWAIT)) )  
        // {  

            // continue;  
            // //pthread_exit((void *)-1);  

        // }  
        // printf("thread1:received msg \"%s\" %s  MsgType = %d\n", msg.msgText, msg.msg2, msg.msgType);  
          

        // msg.msgType = 2; 
        // sprintf(msg.msgText,"%s","Message from thr_fun1"); 
        // if ( -1 == (err = msgsnd(qid, &msg, sizeof(struct myMsg), 0)) ) 
        // { 
            // perror("thr1: send msg error!\n"); 
        // } 

        // //sleep(2);  
          
    // }  
      
    // //fprintf(stdout, "thread1 receive: %s\n", msg.msgText);  
    // //  
  
  
      
  
  
    // //pthread_exit((void *)2);  
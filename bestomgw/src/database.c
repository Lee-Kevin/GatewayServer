/**************************************************************************************************
 Filename:       database.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-08-30 13:23:33 -0700 
 Revision:       

 Description:    This is a file that are interfaces for the database

 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/msg.h>
#include "sqlite3.h"
#include "socket_client.h"
#include "ap_protocol.h"
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

/* Global variables */
extern int     QueueIndex;
extern int     DataBaseQueueIndex;
extern uint8_t CheckDevicestatusErrFlag;

static uint8_t UpdateShortAddrtoDatabase(sDevlist_info_t * devInfo);
static uint8_t CheckIfDataRepeat(sDevlist_info_t * devInfo);
static uint8_t check_Database(char * databaseName);
static int     create_DatabaseTable(sqlite3 *db);
static int     callback(void *NotUsed, int argc, char **argv, char **azColName);
static int     check_DatabaseTable(sqlite3 * db);
static int     mysqlite3_exec(sqlite3 * db, char *sql);
static uint8_t DeleteDatafromDatabase(sDevlist_info_t * devInfo);
static int     GetDeviceInfofromDatabase(sDevlist_info_t * devInfo);
static uint8_t DatabaseFlag = 0;
static char    databaseName[30];

static void    *DataBaseOptionFun(void *arg);

/*********************************************************************
 * @fn     APDatabaseInit
 *
 * @brief  初始化数据库
 *
 * @param  数据库名称
 *
 * @return
 */

void APDatabaseInit(char * database) {
	pthread_t tdDataBase;
	
	sprintf(databaseName,"%s",database);
	check_Database(databaseName);
	
	if (pthread_create(&tdDataBase, NULL, DataBaseOptionFun, &DataBaseQueueIndex)) {
		printf("Failed to create DataBaseOptionFun thread \n");
	}
	
}

/*********************************************************************
 * @fn     Start_Transaction
 *
 * @brief  开启事务模式函数
 *
 * @param  数据库 
 *
 * @return
 */

void Start_Transaction(sqlite3* db) {
	sqlite3_exec(db, "begin transaction;", NULL, NULL, NULL);
}

/*********************************************************************
 * @fn     End_Transaction
 *
 * @brief  关闭事务模式
 *
 * @param  数据库 
 *
 * @return
 */

void End_Transaction(sqlite3* db) {
	sqlite3_exec(db, "commit transaction;", NULL, NULL, NULL);
}

/*********************************************************************
 * @fn     DataBaseOptionFun
 *
 * @brief  插入数据
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
 * @param  数据库名称, devInfo
 *
 * @return true 代表已经有数据，插入失败, 0代表以前没有数据，插入成功
 */
void *DataBaseOptionFun(void *arg) {
	uint8_t done = 0;
	debug_printf("\n\n[DBG] ***********************DataBaseOptionFun.************************** \n\n"); 
	
	do {
		int  size;
		myDataBaseMsg_t msg;
		memset(&msg,0,sizeof(myDataBaseMsg_t));
		size = msgrcv(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), QUEUE_TYPE_DATABASE_SEND, MSG_NOERROR); 
		debug_printf("\n[DBG] ************************DataBaseOptionFun. Get the message****************************%d\n",size);
		if(size >=  sizeof(myDataBaseMsg_t)) {
			// debug_printf("\n[DBG]DataBaseOptionFun I have recived the msg size is %d\n",size);
			switch(msg.Option_type) {
			case DATABASE_OPTION_INSERT:
				InsertDatatoDatabase(&(msg.devlist));	//  无需修改 
				break;
			case DATABASE_OPTION_UPDATE:				//  无需修改
				UpdateDevStatustoDatabase(&(msg.devlist));
				break;
			case DATABASE_OPTION_SELECT:				//  需要向不同线程发送获得的数据
				GetDeviceInfofromDatabase(&(msg.devlist));
				break;
			case DATABASE_OPTION_DELETE:
				DeleteDatafromDatabase(&(msg.devlist));
				break;
			default:
				debug_printf("\n[ERR]DataBaseOptionFun may be wrong" );
				break;
			}
		}
		debug_printf("\n[DBG] This is DataBaseOptionFun while");
	} while (!done);
}
 
/*********************************************************************
 * @fn     InsertDatatoDatabase
 *
 * @brief  插入数据
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
 * @param  数据库名称, devInfo
 *
 * @return true 代表已经有数据，插入失败, 0代表以前没有数据，插入成功
 */
uint8_t InsertDatatoDatabase(sDevlist_info_t * devInfo) {
	sDevlist_info_t _devInfo = *devInfo;
	int rc;
	uint8_t ifRepeat = 0;
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	/* 判断数据是否重复 */
	if(CheckIfDataRepeat(&_devInfo)) {
		debug_printf("\n[DBG] The Database already have the data\n");
		ifRepeat = 1;
		UpdateShortAddrtoDatabase(&_devInfo);  // 如果数据重复，更新短地址信息
	} else {

		char *sql = "INSERT INTO Devlist(ID,DeviceName,ProductId,ShortAddr,DeviceId,DeviceStatus,Power,Note,TIME) values(?,?,?,?,?,?,?,?,datetime('now','localtime'));";
		rc = sqlite3_open(databaseName, &db);  
		if(rc){  
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));   
		}else{  
			fprintf(stderr, "InsertDatatoDatabase Opened database successfully\n");  
		}
		Start_Transaction(db);
		sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
	 //   sqlite3_bind_int(stmt,1, NULL);                                   /* ID 每执行一次插入动作，会自动增加 */
		sqlite3_bind_text(stmt, 2, _devInfo.devName, -1, NULL);
		sqlite3_bind_text(stmt, 3, _devInfo.prdID, -1, NULL);
		sqlite3_bind_text(stmt, 4, _devInfo.shortAddr,-1,NULL);
		sqlite3_bind_text(stmt, 5, _devInfo.devID, -1, NULL);
		sqlite3_bind_int(stmt,  6, _devInfo.status);
		sqlite3_bind_int(stmt,  7, _devInfo.power);
		sqlite3_bind_text(stmt, 8, _devInfo.note, -1, NULL);

		rc = sqlite3_step(stmt);                                         /* 将上述字符串传递至虚拟引擎 */
		if (rc != SQLITE_DONE) {
			debug_printf("[DBG] Cann't insert the data to the database, Error code :%d\n",rc);
		}
		
		sqlite3_finalize(stmt);
		End_Transaction(db);
		sqlite3_close(db); 
		debug_printf("[DBG] Insert the data to the database\n");

	}
	return ifRepeat;
}

/*********************************************************************
 * @fn     GetDeviceInfofromDatabase
 *
 * @brief  获取数据
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
 * @param  数据库名称, devInfo
 *
 * @return true 代表读取数据失败, 0代表读取数据成功
 */
int GetDeviceInfofromDatabase(sDevlist_info_t * devInfo) {
	int rc;
	sDevlist_info_t _devInfo = *devInfo;
	myDataBaseMsg_t msg;
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	memset(&msg,0,sizeof(myDataBaseMsg_t));
	debug_printf("\n[DBG]GetDeviceInfofromDatabase Fun \n");
	if (strlen(_devInfo.devID) != 0) {                                      /* 根据唯一的设备ID获取设备信息 */
		char sql[255];
		debug_printf("\n[DBG]GetDeviceInfofromDatabase Fun get device ID \n");
		msg.msgType = 	QUEUE_TYPE_DATABASE_GETINFO;
		msg.Option_type = DATABASE_OPTION_NULL;
		sDevlist_info_t _devInfoTemp;
		sprintf(sql,"SELECT * FROM Devlist WHERE DeviceId = '%s'",_devInfo.devID);
		rc = sqlite3_open(databaseName, &db);  
		
		if(rc) {  
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db)); 
		    return rc;
		} else{  
			debug_printf("\n[DBG]GetDevInfofromDatabase Opened database successfully\n");  
		}
		debug_printf("\n[DBG]GetDeviceInfofromDatabase The sql is %s",sql);
		Start_Transaction(db);
		sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
		rc =  sqlite3_step(stmt);	
		debug_printf("\n[DBG] The rc is %d \n",rc);
		if(rc == SQLITE_DONE) {       /* 没有获取任何数据 */
			sprintf(msg.devlist.prdID,"%s","00");
			sprintf(msg.devlist.shortAddr,"%s","0000");
			if ( -1 == (msgsnd(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), 0)) ) {
			}
			debug_printf("\n[DBG]GetDeviceInfofromDatabase Get nothing and send msg to queue\n");
		}
		
		while(rc != SQLITE_DONE) {
			// debug_printf("\n[DBG] In the while\n");
			int num_cols = sqlite3_column_count(stmt);
			debug_printf("\n[DBG] In the while the num_cols is %d\n",num_cols);
			for (uint8_t i = 0; i < num_cols; i++)
			{
				if (i==2) {
					sprintf(msg.devlist.prdID,"%s",sqlite3_column_text(stmt, i));
				} else if (i==3) {
					sprintf(msg.devlist.shortAddr,"%s",sqlite3_column_text(stmt, i));
					debug_printf("\n[DBG] In the while the num_cols is %d\n",num_cols);
					// *devInfo.shortAddr = sqlite3_column_text(stmt, i);
				} else if (i==5) {
					msg.devlist.status = sqlite3_column_int(stmt, i);
				} else {
					// debug_printf("\n[DBG] hello\n");
				}
			}
			rc =  sqlite3_step(stmt);
			if(rc == SQLITE_DONE) {
				/* 调用msgsnd 将数据放到消息队列中 */
				if ( -1 == (msgsnd(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), 0)) ) {

				}
			}
			// debug_printf("\n[DBG] Get DevInfo The num_cols is %d the rc is %d",num_cols,rc);
			if(num_cols == 0) {
				printf("\n[ERR] GetDevInfofromDatabase There is something error \n");
				break;
			}
		}
		
		End_Transaction(db);
		sqlite3_finalize(stmt);
		sqlite3_close(db); 
		debug_printf("[DBG] END GetDevInfofromDatabase \n");
	} else if (strlen(_devInfo.prdID) != 0) {                                   /* 根据产品ID 获取相同设备的信息 */
		char sql[255];
		msg.msgType = 	QUEUE_TYPE_DATABASE_GETSTATUS;
		msg.Option_type = DATABASE_OPTION_NULL;
		// debug_printf("\n[DBG] GetDevInfofromDatabase status the prdid is %s",_devInfo.prdID);
		sprintf(sql,"SELECT * FROM Devlist WHERE ProductId = '%s'",_devInfo.prdID);
		rc = sqlite3_open(databaseName, &db);  
		if(rc) {  
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));   
			return rc;
		} else {  
			sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
			Start_Transaction(db);  // 开启事务模式
			
			rc = sqlite3_step(stmt);
			debug_printf("\n[DBG] SELECT * FROM Devlist WHERE ProductId = %s",_devInfo.prdID);
			// debug_printf("\n[DBG] CheckDevStatusfromDatabase the myrc is %d\n",myrc);
			while(rc != SQLITE_DONE) {
				int num_cols = sqlite3_column_count(stmt);
				// debug_printf("\n[DBG]********************************\n");
				
				for (uint8_t i = 0; i < num_cols; i++) {
					if (i==1) {
						sprintf(msg.devlist.devName,"%s",sqlite3_column_text(stmt, i));
					} else if (i==2) {
						sprintf(msg.devlist.prdID,"%s",sqlite3_column_text(stmt, i));
						// *devInfo.prdID = sqlite3_column_text(stmt, i);
					} else if (i==4) {     // 获得设备ID
						sprintf(msg.devlist.devID,"%s",sqlite3_column_text(stmt, i));
						// *devInfo.shortAddr = sqlite3_column_text(stmt, i);
						// sprintf(msg.msgName,"%s",sqlite3_column_text(stmt, i));
					} else if (i==5) {
						msg.devlist.status = sqlite3_column_int(stmt, i); // 模块的在线状态信息 0 Offline 1 Online
						if(!msg.devlist.status) {
							break; //已经离线的设备无需检查
						}
					} else if (i==8) { // 获得时间  
					
						sprintf(msg.devlist.TimeText,"%s",sqlite3_column_text(stmt, i));
						if ( -1 == (msgsnd(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), 0)) )  {
						
						}
					}
					
					// switch (sqlite3_column_type(stmt, i))
					// {
					// case (SQLITE3_TEXT):
						// printf("%d %s, ", i,sqlite3_column_text(stmt, i));
						// break;
					// case (SQLITE_INTEGER):
						// printf("%d,%d, ",i, sqlite3_column_int(stmt, i));
						// break;
					// case (SQLITE_FLOAT):
						// printf("%d, %g, ", i,sqlite3_column_double(stmt, i));
						// break;
					// default:
						// break;
					// }
				}
				rc = sqlite3_step(stmt);
				// debug_printf("\n[DBG] GetDeviceInfofromDatabase prdid The num_cols is %d the myrc is %d",num_cols,rc);
				if(num_cols == 0) {
					debug_printf("\n[ERR] -----------------------Something wrong-------------------------------------\n");
					debug_printf("[ERR] ERR End of the GetDeviceInfofromDatabase\n");
					break;
				}
			}
			End_Transaction(db);			// 关闭事务模式
			sqlite3_finalize(stmt);
			sqlite3_close(db); 
			// debug_printf("\n[DBG] End of the GetDeviceStatusfromDatabase\n");
		}
	} else {
		debug_printf("\n[ERR] Sorry I don't know what to do! ");
	}

	return rc;
}

/*********************************************************************
 * @fn     GetDevInfofromDatabase
 *
 * @brief  获取数据
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
 * @param  数据库名称, devInfo
 *
 * @return true 代表读取数据失败, 0代表读取数据成功
 */
uint8_t GetDevInfofromDatabase(char *DevId, sDevlist_info_t * devInfo) {
	int rc;
	
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	
	if(DevId == NULL) {
		logError("GetDevInfofromDatabase the devID is NULL");
		return rc;
	}
	
	char sql[255];
	sprintf(sql,"SELECT * FROM Devlist WHERE DeviceId = '%s'",DevId);
	rc = sqlite3_open(databaseName, &db);  
	if(rc){  
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));   
	}else{  
		debug_printf("\n[DBG]GetDevInfofromDatabase Opened database successfully\n");  
	}
	
	Start_Transaction(db);
	sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
	// sqlite3_bind_text(stmt, 1, DevId, -1, NULL);
	
	// debug_printf("\n[DBG] Before ERR --GetDevInfofromDatabase ERR? ");
	rc =  sqlite3_step(stmt);
	// debug_printf("\n[DBG] After ERR --GetDevInfofromDatabase ERR? ");
	// debug_printf("\n[DBG] The rc is %d \n",rc);
	while(rc != SQLITE_DONE) {
		debug_printf("\n[DBG] In the while\n");
		int num_cols = sqlite3_column_count(stmt);
		debug_printf("\n[DBG] In the while the num_cols is %d\n",num_cols);
		for (uint8_t i = 0; i < num_cols; i++)
		{
			if (i==2) {
				debug_printf("\n[DBG] In the while Before sprintf \n");
				sprintf(devInfo->prdID,"%s",sqlite3_column_text(stmt, i));
				debug_printf("\n[DBG] ERR The prdID is  %s\n",devInfo->prdID);
			} else if (i==3) {
				sprintf(devInfo->shortAddr,"%s",sqlite3_column_text(stmt, i));
				// *devInfo.shortAddr = sqlite3_column_text(stmt, i);
			} else if (i==5) {
				devInfo->status = sqlite3_column_int(stmt, i);
			} else {
				debug_printf("\n[DBG] hello\n");
			}
		}
		rc =  sqlite3_step(stmt);
		// debug_printf("\n[DBG] Get DevInfo The num_cols is %d the rc is %d",num_cols,rc);
		if(num_cols == 0) {
			printf("\n[ERR] GetDevInfofromDatabase There is something error \n");
			break;
		}
	}
	End_Transaction(db);
	sqlite3_finalize(stmt);
	sqlite3_close(db); 
	// debug_printf("[DBG] END GetDevInfofromDatabase \n");

	return rc;
}

/*********************************************************************
 * @fn     DeleteDatafromDatabase
 *
 * @brief  获取数据
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
 * @param  数据库名称, devInfo
 *
 * @return true 代表读取数据失败, 0代表读取数据成功
 */
uint8_t DeleteDatafromDatabase(sDevlist_info_t * devInfo) {
	int rc;
	sDevlist_info_t _devInfo = *devInfo;
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	
	char sql[255];
	sprintf(sql,"DELETE FROM Devlist WHERE DeviceId = '%s'",_devInfo.devID);
	rc = sqlite3_open(databaseName, &db);  
	if(rc){  
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));   
	} else{  
		debug_printf("\n[DBG]DeleteDatafromDatabase Opened database successfully\n");  
	}
	
	Start_Transaction(db);
	sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
	// sqlite3_bind_text(stmt, 1, DevId, -1, NULL);

	rc =  sqlite3_step(stmt);

	debug_printf("\n[DBG] The rc is %d \n",rc);
	while(rc != SQLITE_DONE) {
		debug_printf("\n[DBG] DeleteDatafromDatabase rc is %d \n",rc);
		break;
	}
	End_Transaction(db);
	sqlite3_finalize(stmt);
	sqlite3_close(db); 
	debug_printf("[DBG] END GetDevInfofromDatabase \n");
	return rc;
}

 /*********************************************************************
 * @fn     UpdateDevStatustoDatabase
 *			
 * @brief  更新子设备的在线状态, 如果子设备离线，则发送消息至消息队列，告诉Server子设备在线
 *
 * @param  数据库名称, devInfo
 *	"CREATE TABLE Devlist( \
		1			   ID  INTEGER PRIMARY KEY AUTOINCREMENT    NOT NULL,\
		2			   DeviceName         TEXT      NOT NULL, \
		3			   ProductId          TEXT      NOT NULL, \
		4			   ShortAddr          TEXT      NOT NULL, \
		5			   DeviceId           TEXT      NOT NULL, \
		6			   DeviceStatus       INT       NOT NULL, \
		7			   Power              INT,                \
		8			   Note          TEXT,  \
		9		       TIME TEXT);";
 * @return
 */
uint8_t UpdateDevStatustoDatabase(sDevlist_info_t * devInfo) {
	
	sDevlist_info_t _devInfo = *devInfo;
	int rc;
	debug_printf("\n[DBG] UpdateDevStatustoDatabase in fun\n");
	if (CheckDevicestatusErrFlag != 2) {
		sqlite3* db = NULL;
		sqlite3_stmt* stmt = NULL;
		uint8_t deviceStatus = 1;
		char* err_msg = NULL;
		char sql[255];
		// char prdid[3];
		// char *sql1 = "SELECT DeviceName,DeviceStatus FROM Devlist WHERE DeviceId = ?";
		char sql1[255];
		sprintf(sql1,"SELECT DeviceName,ProductId,DeviceStatus FROM Devlist WHERE DeviceId = '%s'",_devInfo.devID);
		rc = sqlite3_open(databaseName, &db);  
		if(rc){  
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
			return rc;  
		}else{  
			fprintf(stderr, "UpdateDevStatustoDatabase Opened database successfully\n");  
		}
		Start_Transaction(db);
		//
		sqlite3_prepare_v2(db, sql1, strlen(sql1), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
		// sqlite3_bind_text(stmt, 1, _devInfo.devID, -1, NULL);
		
		rc = sqlite3_step(stmt);
		fprintf(stderr, "\n fprintf UpdateDevStatustoDatabase rc is %d\n",rc);  
		debug_printf("\n[DBG] The UpdateDevStatustoDatabase rc is %d.\n",rc);
		while(rc != SQLITE_DONE) {
			int num_cols = sqlite3_column_count(stmt);
			debug_printf("\n[DBG] 1The num_cols is %d, the rc is %d",num_cols,rc);
			
			for (uint8_t i = 0; i < num_cols; i++)  {
				if (i==0) {
					sprintf(_devInfo.devName,"%s",sqlite3_column_text(stmt, i));
				} else if (i==1) {
					sprintf(_devInfo.prdID,"%s",sqlite3_column_text(stmt, i));
					// debug_printf("\n[DBG] UpdateDevStatustoDatabase the device prdID is %s",_devInfo.prdID);
				} else if (i==2) {
					deviceStatus = sqlite3_column_int(stmt, i);
					// debug_printf("\n[DBG] UpdateDevStatustoDatabase the device status is %d",deviceStatus);
				}  else {
				}
				
				// switch (sqlite3_column_type(stmt, i)) {
				// case (SQLITE3_TEXT):
					// printf("%d-%s, ", i,sqlite3_column_text(stmt, i));
					// break;
				// case (SQLITE_INTEGER):
					// printf("%d-%d, ",i, sqlite3_column_int(stmt, i));
					// break;
				// case (SQLITE_FLOAT):
					// printf("%d-%g, ", i,sqlite3_column_double(stmt, i));
					// break;
				// default:
					// break;
				// }
				
			}
			if(num_cols == 0) {
				debug_printf("\n[DBG] break\n");
				break;
			}
			rc = sqlite3_step(stmt);
		}
		
		if (deviceStatus == 0) {
			myMsg_t msg;
			msg.msgType = 1;
			sprintf(msg.msgName,"%s",_devInfo.devName);
			sprintf(msg.msgDevID,"%s",_devInfo.devID);
			msg.value   = 1;
			debug_printf("\n[DBG] The device is offline last time");
			
			if (-1 == (msgsnd(QueueIndex, &msg, sizeof(myMsg_t), 0))) {    // 将在线信息，发送至消息队列
				debug_printf("\n[ERR] There is something wrong with msgsnd");
			} else {
				debug_printf("\n[DBG] Send info to the queue about the device status\n");
			}
			logWarn("[Status] Online  -- prdID:%s -- devID:%s .",_devInfo.prdID,_devInfo.devID);
		}

		sprintf(sql,"UPDATE Devlist SET DeviceStatus = %d, \
							TIME = datetime('now','localtime'), \
							Note = '%s' \
							WHERE DeviceId = '%s'",_devInfo.status, _devInfo.note, _devInfo.devID);
		
		rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
		fprintf(stderr, "\n[DBG] UpdateDevStatustoDatabase sql is %s\n",sql);  
		if(rc != SQLITE_OK) {
			printf("SQL error:%s\n",err_msg);
			sqlite3_free(err_msg);
		}
		End_Transaction(db);
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		debug_printf("[DBG] THE SQL IS %s\n",sql);
		debug_printf("[DBG] Update data to database successfully\n");
	}
	return rc;
}

 /*********************************************************************
 * @fn     DeleteDatatoDatabase
 *			
 * @brief  插入数据 更新传感器的在线状态
 *
 * @param  数据库名称, devInfo
 *
 * @return
 */
uint8_t UpdateDatatoDatabase(sDevlist_info_t * devInfo) {
	
	sDevlist_info_t _devInfo = *devInfo;
	int rc;
    sqlite3* db = NULL;
	char* err_msg = NULL;
	char sql[255];
    rc = sqlite3_open(databaseName, &db);  
    if(rc){  
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
        return rc;  
    }else{  
        fprintf(stderr, "UpdateDatatoDatabase Opened database successfully\n");  
    }
	
	sprintf(sql,"UPDATE Devlist SET DeviceStatus = %d, \
						Power = '%d',                   \
	                    TIME = datetime('now','localtime'), \
						Note = '%s' \
						WHERE DeviceId = '%s'",_devInfo.status, _devInfo.power, _devInfo.note, _devInfo.devID);
	
	rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
	
    if(rc != SQLITE_OK) {
        printf("SQL error:%s\n",err_msg);
        sqlite3_free(err_msg);
    }
	debug_printf("[DBG] THE SQL IS %s\n",sql);
	debug_printf("[DBG] Update data to database successfully\n");
    sqlite3_close(db);
	return rc;
}

 /*********************************************************************
 * @fn     UpdateShortAddrtoDatabase
 *			
 * @brief  更新子设备的短地址，每次重新入网，短地址可能会发生变化，需要重新刷新
 *
 * @param  数据库名称, devInfo
 *
 * @return
 */
uint8_t UpdateShortAddrtoDatabase(sDevlist_info_t * devInfo) {
	
	sDevlist_info_t _devInfo = *devInfo;
	int rc;
    sqlite3* db = NULL;
	char* err_msg = NULL;
	char sql[255];
    rc = sqlite3_open(databaseName, &db);  
    if(rc){  
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
        return rc;  
    }else{  
        fprintf(stderr, "UpdateShortAddrtoDatabase Opened database successfully\n");  
    }
	Start_Transaction(db);
	
	sprintf(sql,"UPDATE Devlist SET ShortAddr = '%s', \
	                    TIME = datetime('now','localtime') \
						WHERE DeviceId = '%s'",_devInfo.shortAddr, _devInfo.devID);
	
	rc = sqlite3_exec(db, sql, NULL, 0, &err_msg);
	
    if(rc != SQLITE_OK) {
        printf("SQL error:%s\n",err_msg);
        sqlite3_free(err_msg);
    }
	debug_printf("[DBG] THE SQL IS %s\n",sql);
	debug_printf("[DBG] Update shortAddr to database successfully\n");
	End_Transaction(db);
    sqlite3_close(db);
	return rc;
}


/*********************************************************************
 * @fn     check_Database
 *
 * @brief  检查数据库是否为空，如果为空，则创建新的数据库
 *
 * @param  数据库名称
 *
 * @return
 */

uint8_t check_Database(char* databaseName) {
	int rc;
	sqlite3 *db = NULL;
	// printf("\nThis is check_Database function, the databaseName is %s\n",databaseName);
	
	rc = sqlite3_open(databaseName, &db);
    if( rc ){  
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
		return rc;
    }else{  
        fprintf(stderr, "check_Database Opened database successfully\n");  
    }
	rc = check_DatabaseTable(db);
	if (rc != SQLITE_OK) {
		return rc;
	}
	/*
	** 判断是否已经有数据库存在
	*/
	if (DatabaseFlag) {
		/* 已经有数据库存在 */
		debug_printf("[DBG] There are already exist database\n");
		//mysqlite3_exec(db,"SELECT * FROM Devlist");
	} else {
		/* 创建表 */
		
		if(create_DatabaseTable(db) ==SQLITE_OK) {
			debug_printf("[DBG] Create new table successfully\n");
		} else {
			debug_printf("[DBG] Create new table failed\n");
		}
	}
	sqlite3_close(db);
}

/*********************************************************************
 * @fn     create_DatabaseTable
 *
 * @brief  创建数据库表
 *
 * @param  数据库名称
 *
 * @return
 */

int create_DatabaseTable(sqlite3 * db) {
	int rc;
	char *zErrMsg = 0;
	char *sql =  "CREATE TABLE Devlist( \
					   ID  INTEGER PRIMARY KEY AUTOINCREMENT    NOT NULL,\
					   DeviceName         TEXT      NOT NULL, \
					   ProductId          TEXT      NOT NULL, \
					   ShortAddr          TEXT      NOT NULL, \
					   DeviceId           TEXT      NOT NULL, \
					   DeviceStatus       INT       NOT NULL, \
					   Power              INT,                \
					   Note          TEXT,  \
				       TIME TEXT);";

	rc = mysqlite3_exec(db,sql);
	return rc;
}


/*********************************************************************
 * @fn     CheckIfDataRepeat
 *
 * @brief  插入数据
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
 * @param  数据库名称, devInfo
 *
 * @return 1 代表数据重复， 0 代表无数据重复
 */
static uint8_t CheckIfDataRepeat(sDevlist_info_t * devInfo) {
	sDevlist_info_t _devInfo = *devInfo;
	int rc;
	uint8_t ifRepeat = 0;
    sqlite3* db = NULL;
    sqlite3_stmt* stmt = NULL;
	/* 判断数据是否重复 */
	char sql[255];
	sprintf(sql,"SELECT * FROM Devlist WHERE DeviceId = '%s'",_devInfo.devID);
	//sprintf(sql,"SELECT * FROM Devlist WHERE DeviceId = '%s'", )
    rc = sqlite3_open(databaseName, &db);  
    if(rc){  
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
        // return rc;  
    }else{  
        fprintf(stderr, "CheckIfDataRepeat Opened database successfully\n");  
    }
	
    sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
	// sqlite3_bind_text(stmt, 1, _devInfo.devID, -1, NULL);
	
	// debug_printf("[DBG] Before sqlite3_step \n");
	rc = sqlite3_step(stmt);
	while(rc != SQLITE_DONE) {
		int num_cols = sqlite3_column_count(stmt);
		if(num_cols == 9) {
			ifRepeat = 1;
		}
		rc = sqlite3_step(stmt);
		debug_printf("\n[DBG] Check If DataRepeatThe num_cols is %d,the rc is %d",num_cols,rc);
		if (num_cols == 0) {
			ifRepeat = 1;
			debug_printf("\n[DBG] There is something wrong with data check repeat");
			break;
		}
	}
	debug_printf("[DBG] CheckIfDataRepeat Done \n");
    sqlite3_finalize(stmt);

    sqlite3_close(db); 
	return ifRepeat;
	
}

/*********************************************************************
 * @fn     callback
 *
 * @brief  回调函数
 *
 * @param  数据库名称
 *
 * @return
 */

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
//   printf("This is callback func\n");
   for(i=0; i<argc; i++){
      // printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	  
	  if (strcmp("Devlist",argv[i]) == 0) {
		  DatabaseFlag = 1; // 已经有经过初始化之后的数据库存在
	  }
   }
   printf("\n");
   return 0;
}

/*********************************************************************
 * @fn     check_DatabaseTable
 *
 * @brief  检查数据可以表是否存在
 *
 * @param  数据库名称
 *
 * @return
 */

int check_DatabaseTable(sqlite3 * db) {
	int rc;
	char *zErrMsg = 0;
	char *sql = "SELECT * from sqlite_master WHERE type = 'table'";
    rc = mysqlite3_exec(db,sql);
	return rc;
}

/*********************************************************************
 * @fn     mysqlite3_exec
 *
 * @brief  通用执行sql语句函数
 *
 * @param  数据库名称，执行的SQL语句
 *
 * @return
 */

int mysqlite3_exec(sqlite3 * db, char *sql) {
	int rc;
	char *zErrMsg = 0;
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
		// debug_printf("[ERR] mysqlite3_exec something wrong\n");
    } else {
		//fprintf(stdout, "Table check successfully\n");
		// debug_printf("[DBG] mysqlite3_exec successfully\n");
    }
	return rc;
}
/*********************************************************************
 * @fn     thread_sqlite3_step
 *
 * @brief  通用执行sql语句函数
 *
 * @param  数据库名称，执行的SQL语句
 *
 * @return
 */


// int thread_sqlite3_step(sqlite3_stmt** stmt, sqlite3* db)
// {
    // int sleep_acount = 0;
    // int rc;
    // char* errorMsg = NULL;

    // do{
        // rc = sqlite3_step(*stmt);   
        // // M1_LOG_DEBUG("step() return %s, number:%03d\n", rc == SQLITE_DONE ? "SQLITE_DONE": rc == SQLITE_ROW ? "SQLITE_ROW" : "SQLITE_ERROR",rc);
        // if(rc == SQLITE_BUSY || rc == SQLITE_LOCKED || rc == SQLITE_MISUSE){
            // sqlite3_reset(*stmt);
            // sleep_acount++;
            // usleep(100000);
        // }
        
    // }while((sleep_acount < 10) && ((rc == SQLITE_BUSY) || (rc == SQLITE_LOCKED) || (rc == SQLITE_MISUSE)));

    // if(rc == SQLITE_BUSY || rc == SQLITE_MISUSE || rc == SQLITE_LOCKED){
        // if(sqlite3_exec(db, "ROLLBACK", NULL, NULL, &errorMsg) == SQLITE_OK){
            // M1_LOG_DEBUG("ROLLBACK OK\n");
            // sqlite3_free(errorMsg);
        // }else{
            // M1_LOG_ERROR("ROLLBACK FALIED\n");
        // }
    // }

    // free(errorMsg);
    // return rc;
// }

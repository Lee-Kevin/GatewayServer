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
#include <sqlite3.h>
#include "socket_client.h"
#include "ap_protocol.h"
#include "database.h"

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

static uint8_t CheckIfDataRepeat(sDevlist_info_t * devInfo);
static uint8_t check_Database(char * databaseName);
static int create_DatabaseTable(sqlite3 *db);
static int callback(void *NotUsed, int argc, char **argv, char **azColName);
static int check_DatabaseTable(sqlite3 * db);
static int mysqlite3_exec(sqlite3 * db, char *sql);
static uint8_t DatabaseFlag = 0;
static char databaseName[30];

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
	
	sprintf(databaseName,"%s",database);
	check_Database(databaseName);
}

/*********************************************************************
 * @fn     InsertDatatoDatabase
 *
 * @brief  插入数据
 	 "CREATE TABLE Devlist( \
		1	   ID INT PRIMARY KEY           NOT NULL,\
		2	   DeviceId           TEXT      NOT NULL,\
		3	   DeviceName         TEXT      NOT NULL,\
		4	   DeviceStatus       INT       NOT NULL,\
		5	   Power              INT,                \
		6	   Note               TEXT ,\
		7	   TIME            TEXT);"
 *
 * @param  数据库名称, devInfo
 *
 * @return
 */
uint8_t InsertDatatoDatabase(sDevlist_info_t * devInfo) {
	sDevlist_info_t _devInfo = *devInfo;
	int rc;
	uint8_t ifRepeat = 0;
	sqlite3* db = NULL;
	sqlite3_stmt* stmt = NULL;
	/* 判断数据是否重复 */
	if(CheckIfDataRepeat(&_devInfo)) {
		debug_printf("\n[DBG] The Database already have the data\n",rc);
		ifRepeat = 1;
	} else {

		char *sql = "INSERT INTO Devlist(ID, DeviceId,DeviceName,DeviceStatus,Power,Note,TIME) values(?,?,?,?,?,?,datetime('now','localtime'));";
		rc = sqlite3_open(databaseName, &db);  
		if(rc){  
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));   
		}else{  
			fprintf(stderr, "InsertDatatoDatabase Opened database successfully\n");  
		}
		sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
	 //   sqlite3_bind_int(stmt,1, NULL);                                   /* ID 每执行一次插入动作，会自动增加 */
		sqlite3_bind_text(stmt, 2, _devInfo.devID, -1, NULL);
		sqlite3_bind_text(stmt, 3, _devInfo.devName, -1, NULL);
		sqlite3_bind_int(stmt, 4, _devInfo.status);
		sqlite3_bind_int(stmt, 5, _devInfo.power);
		sqlite3_bind_text(stmt, 6, _devInfo.note, -1, NULL);

		rc = sqlite3_step(stmt);                                         /* 将上述字符串传递至虚拟引擎 */
		if (rc != SQLITE_DONE) {
			debug_printf("[DBG] Cann't insert the data to the database, Error code :%d\n",rc);
		}
		debug_printf("[DBG] Insert the data to the database\n");
		sqlite3_finalize(stmt);
		sqlite3_close(db); 
		return ifRepeat;
	}
	
}

 /*********************************************************************
 * @fn     UpdateDevStatustoDatabase
 *			
 * @brief  更新子设备的在线状态
 *
 * @param  数据库名称, devInfo
 *
 * @return
 */
uint8_t UpdateDevStatustoDatabase(sDevlist_info_t * devInfo) {
	
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
        fprintf(stderr, "Opened database successfully\n");  
    }
	
	sprintf(sql,"UPDATE Devlist SET DeviceStatus = %d, \
	                    TIME = datetime('now','localtime'), \
						Note = '%s' \
						WHERE DeviceId = '%s'",_devInfo.status, _devInfo.note, _devInfo.devID);
	
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
 * @fn     UpdateDatatoDatabase
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
        fprintf(stderr, "Opened database successfully\n");  
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
        fprintf(stderr, "Opened database successfully\n");  
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
					   DeviceId           TEXT      NOT NULL, \
					   DeviceName         TEXT      NOT NULL, \
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
		1	   ID INT PRIMARY KEY           NOT NULL,\
		2	   DeviceId           TEXT      NOT NULL,\
		3	   DeviceName         TEXT      NOT NULL,\
		4	   DeviceStatus       INT       NOT NULL,\
		5	   Power              INT,                \
		6	   Note               TEXT ,\
		7	   TIME            TEXT);"
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
    char *sql = "SELECT * FROM Devlist WHERE DeviceId = ?";
	//sprintf(sql,"SELECT * FROM Devlist WHERE DeviceId = '%s'", )
    rc = sqlite3_open(databaseName, &db);  
    if(rc){  
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));  
        return rc;  
    }else{  
        fprintf(stderr, "Opened database successfully\n");  
    }
    sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);              /* 将sql命令 转存至stmt 结构体之中 */
	sqlite3_bind_text(stmt, 1, _devInfo.devID, -1, NULL);
	//debug_printf("[DBG] Before sqlite3_step \n");
	while(sqlite3_step(stmt) != SQLITE_DONE) {
		int num_cols = sqlite3_column_count(stmt);
		if(num_cols == 7) {
			ifRepeat = 1;
		}
		debug_printf("\n[DBG] The num_cols is %d",num_cols);
		
	}
	//debug_printf("[DBG] CheckIfDataRepeat Done \n");
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
/**************************************************************************************************
 Filename:       interface_protocol.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-08-30 13:23:33 -0700 
 Revision:       

 Description:    This is a file that are interfaces for the protocol

 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include "socket_client.h"
#include "ap_protocol.h"
#include "interface_protocol.h"

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

static uint8_t check_Database(char * databaseName);
static int create_DatabaseTable(sqlite3 *db);
static int callback(void *NotUsed, int argc, char **argv, char **azColName);
static int check_DatabaseTable(sqlite3 * db);
static int mysqlite3_exec(sqlite3 * db, char *sql);
static uint8_t DatabaseFlag = 0;


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
	char databaseName[30];
	sprintf(databaseName,"%s",database);
	check_Database(databaseName);
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
		mysqlite3_exec(db,"SELECT * FROM Devlist");
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


int create_DatabaseTable(sqlite3 * db) {
	int rc;
	char *zErrMsg = 0;
	char *sql =  "CREATE TABLE Devlist( \
			   ID INT PRIMARY KEY           NOT NULL,\
			   DeviceId           TEXT      NOT NULL,\
			   DeviceName         TEXT      NOT NULL,\
			   DeviceStatus       INT       NOT NULL,\
			   Note               TEXT ,\
			   TIME            TEXT);";
	rc = mysqlite3_exec(db,sql);
	return rc;
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
   printf("This is callback func\n");
   for(i=0; i<argc; i++){
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	  
	  if (strcmp("Devlist",argv[i]) == 0) {
		  DatabaseFlag = 1; // 已经有经过初始化之后的数据库存在
	  }
   }
   printf("\n");
   return 0;
}

int check_DatabaseTable(sqlite3 * db) {
	int rc;
	char *zErrMsg = 0;
	char *sql = "SELECT * from sqlite_master WHERE type = 'table'";
    rc = mysqlite3_exec(db,sql);
	return rc;
}

int mysqlite3_exec(sqlite3 * db, char *sql) {
	int rc;
	char *zErrMsg = 0;
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
		// debug_printf("[ERR] mysqlite3_exec something wrong\n");
    } else {
		fprintf(stdout, "Table check successfully\n");
		// debug_printf("[DBG] mysqlite3_exec successfully\n");
    }
	return rc;
}
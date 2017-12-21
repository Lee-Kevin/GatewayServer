/**************************************************************************************************
 Filename:       apclient.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-08-23 13:23:33 -0700 
 Revision:       $Revision: 246 $

 Description:    This file contains an apclient for the M1 Gateway sever


 **************************************************************************************************/
#define _GNU_SOURCE
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>

#include <mqueue.h>

#include "log.h"
#include "socket_client.h"
#include "interface_srpcserver.h"
#include "hal_defs.h"

#include "zbSocCmd.h"
#include "interface_devicelist.h"
#include "interface_grouplist.h"
#include "interface_scenelist.h"

#include "ap_protocol.h"
#include "cJSON.h"
#include "database.h"
#include "interface_protocol.h"
#include "devicestatus.h"
#include "msgqueue.h"

#define CONSOLEDEVICE "/dev/console"

#define MAX_DB_FILENAMR_LEN 255
//#define POLLRDHUP 0x2000

//全局变量	
//int fdserwrite, fdread; //串口 写,读
 long MAXLEN = 10*1024;//10KB
 char sexepath[PATH_MAX];
 char slogpath[PATH_MAX]; //log 文件路径
 char sdbpath[PATH_MAX]; //db 文件路径
 char sinipath[PATH_MAX]; //db 文件路径 

sqlite3 *gdb=NULL;

extern char mac[23];
extern int localport;

void usage(char* exeName)
{
	printf("Usage: ./%s <port>\n", exeName);
	printf("Eample: ./%s /dev/ttyACM0\n", exeName);
}

void sendDatatoServer(char *str);
void socketClientCb(msgData_t *msg);
void zbSocfun(ZOCData_t *msg);

/*
关于log的使用

	logInfo("this is an info with an integer: %d",15);
	logError("this is an error in scientifc notation: %e",1023332.222);
	logDebug("this is a debug with string: %s %s", "hello", "world");
	logWarn ("this is a warning with three decimals %0.3f", 3.33333);
	logTrace("this is a trace with two decimals %0.2f", 3.33333);
 
*/


int main(int argc, char *argv[]) {
	char servername[20];
	char str[22];
	int zbSoc_fd;
	char dbFilename[MAX_DB_FILENAMR_LEN];
	sDevlist_info_t mydevinfo;
	printf("%s -- %s %s\n", argv[0], __DATE__, __TIME__);
	
	/* 
	 * Client 默认连接的IP地址和端口可变
	*/
	sprintf(str,"%s","192.168.2.1:11235");
	
	logInit("status.log");         // 初始化 log脚本  LOGFLAG_SYSLOG
	logSetFlags(LOGFLAG_FILE |
				LOGFLAG_INFO |
				LOGFLAG_ERROR |
				LOGFLAG_WARN |
				LOGFLAG_TRACE|
				LOGFLAG_DEBUG);
	
	logDebug("Begin the Application");
	
	if (argc < 2) {
		printf(
				"Expected 1 and got %d params Usage: %s <addr mode> <ep> <group name>\n",
				argc, argv[0]);
		printf(
				"Example - connect to server: %s 127.0.0.1:11235\n",
				argv[0]);
		printf("--The default IP address and port is %s--\n",str);
		printf("\n******************************************************\n");
	} else {
		for (int i=0; i < argc; i++) {
			printf("Argument %d is %s.\n", i, argv[i]);
			if(i == 1) {
				// printf("The argc[2] is %s\n",argv[2]);
				sscanf(argv[1],"%s",str);
				printf("---IP address and port is is %s---\n",str);
			}
		}
	}
	
	/*
	* Socket 客户端初始化
	*/
/* 宿主机上无串口设备 lee*/	
	// // accept only 1
	if (argc != 3)
	{
		//usage(argv[0]);
		printf("attempting to use /dev/ttyS0\n");
		
		zbSoc_fd = init_serial("/dev/ttyS0",zbSocfun);
		printf("Open zbSoc_fd : %d\n",zbSoc_fd);
	}
	else
	{
		zbSoc_fd = init_serial(argv[argc-1],zbSocfun);
		printf("Open zbSoc_fd : %d\n",zbSoc_fd);
	}

	if (zbSoc_fd == -1)
	{
		exit(-1);
	}
	
	/*
	* 初始化socket通信，（以后需要添加重连机制）
	*/
	
    sprintf(dbFilename, "%.*s/APDatabase.db",
            strrchr(argv[0], '/') - argv[0], argv[0]);
	sprintf(slogpath,"./zigbee.log");		
    printf("the dbFilename is %s, The slogpath is %s\n", dbFilename,slogpath);
	MsgQueueInit();
	APDatabaseInit(dbFilename);
	
	CheckDevicestatusInit(dbFilename);  // 初始化子设备在线状态检查函数
	/*初始化 socket*/
	heartBeatRegisterCallbackFun(sendHeartBeattoServer);
	sendStatusRegisterCallbackFun(sendOnlineStatustoServer);
	socketClientInit(str, socketClientCb,sendAPinfotoServer);

    // getClientLocalPort(&localport, &mac);
	
	printf("**** The MAC addr : %s, the port : %d ******\n",mac,localport);
	
	AP_Protocol_init(sendDatatoServer);
	
	// printf("-------sendAPinfotoServer---------------\n");
	//sendAPinfotoServer(mac,localport);
	
	/* Open zigbee net work */
	// zbSocOpenNwk(20);
	while(1) {
		// struct pollfd zbfds[1];
		char chartemp[10];
		printf("> Please input the data number:\n>");
	    scanf("%s",chartemp);
		if(strcmp("opennet",chartemp) == 0) {
			printf("\n Open the zigbee network\n");
			zbSocOpenNwk(20);
			//sendDevDatatoServer();
		} else if (strcmp("sqlite",chartemp) == 0) {
			printf("\n Send device info to server\n");
			GetDevInfofromDatabase("e75f6414004b1200",&mydevinfo);
			printf("\nThe shortAddr is %s the prdID is %s\n",mydevinfo.shortAddr,mydevinfo.prdID);
			
			CheckDevStatusfromDatabase("08",2);
			
			// sendDevinfotoServer(mac, localport);
		} else if (strcmp("start",chartemp) == 0){

		} else if (strcmp("clearnet",chartemp) == 0) {
			zbSocClearNwk();
		} else if (strcmp("cleardev",chartemp) == 0) {
			zbSocDeleteDeviceformNetwork();
			// zbSocDeleteDeviceformNetwork("bb91");
		}else {
			printf("\n Please input again! \n");
		}
		
		
	}
	sleep(60);
	
	logClose();
	socketClientClose();
	printf("\n%%%%%%%%%%%%%%%%%%%% This is Main function EXIT %%%%%%%%%%%%%%%%%%%%%\n");
	return 0;
}

/*********************************************************************
 * @fn          socketClientCb
 *
 * @brief		客户端接收数据处理函数
 *
 * @param
 *
 * @return
 */
void socketClientCb(msgData_t *msg)
{
	data_handle(msg->pData);
}

/*********************************************************************
 * @fn          zbSocfun
 *
 * @brief		客户端接收数据处理函数
 *
 * @param
 *
 * @return
 */
void zbSocfun(ZOCData_t *msg)
{
	DealwithSerialData(msg->pData);
}


/*********************************************************************
 * @fn          sendDatatoServer
 *
 * @brief 把数据发送至已经连接的服务器
 *
 * @param 需要发送的数据
 *
 * @return
 */

void sendDatatoServer(char *str) {
	msgData_t msg;
	sprintf(msg.pData,"%s",str);
	
	socketClientSendData(&msg);
}

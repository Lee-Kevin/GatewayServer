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

extern uint8_t ENABLEZIGBEE;
 

void usage(char* exeName)
{
	printf("Usage: ./%s <port>\n", exeName);
	printf("Eample: ./%s /dev/ttyACM0\n", exeName);
}

void sendDatatoServer(char *str);
void socketClientCb(msgData_t *msg);
void zbSocfun(ZOCData_t *msg);


int main(int argc, char *argv[])
{
	char servername[20];
	char str[22];
	char mac[22];
	int localport;
	int zbSoc_fd;
	char dbFilename[MAX_DB_FILENAMR_LEN];
	
	/* Database Test */
	sDevlist_info_t  test_device;
	char *addr = "1234567A";
	char *note = "Update";
	test_device.status = 0;
	test_device.addr = addr;
	test_device.note = note;
	
	printf("%s -- %s %s\n", argv[0], __DATE__, __TIME__);
	
	/* 
	 * Client 默认连接的IP地址和端口可变
	*/
	sprintf(str,"%s","192.168.1.100:11235");
	
	if (argc < 2) {
		printf(
				"Expected 1 and got %d params Usage: %s <addr mode> <ep> <group name>\n",
				argc, argv[0]);
		printf(
				"Example - connect to server: %s 127.0.0.1:11235\n",
				argv[0]);
		printf("--The default IP address and port is %s--\n",str);
		printf("\n******************************************************\n",str);
		
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
		//if (zbSoc_fd<0 )
		//	{zbSoc_fd = zbSocOpen("/dev/null");}
		//init_serial();
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
	APDatabaseInit(dbFilename);
	

	//UpdateDatatoDatabase(dbFilename,&test_device);
	
	/*初始化 socket*/
//	socketClientInit(str, socketClientCb);
	
//    getClientLocalPort(&localport, &mac);
	
	// printf("**** The MAC addr : %s, the port : %d ******\n",mac,localport);
	
	// AP_Protocol_init(sendDatatoServer);
	
	// printf("-------sendAPinfotoServer---------------\n");
	// sendAPinfotoServer(mac,localport);
	
	zbSocOpenNwk(20);
	while(1) {
		// struct pollfd zbfds[1];
		char chartemp[10];
		
		// zbfds[0].fd  = zbSoc_fd;    /* 将Zigbee串口的描述符赋给 poll zbfds数组 */
		// zbfds[0].events = POLLIN; 
		// poll(zbfds,1,-1);           /* 阻塞等待串口接收数据 */
		// //zbSocOpenNwk(20);
		// if (zbfds[0].revents) {
			// printf("Message from ZLL SoC\n");
			// //zbSocProcessRpc();
			// myzbSocProcessRpc();
		// //	zbSocOpenNwk(20);
		// }
		
		if (ENABLEZIGBEE == 1) { /* 允许设备入网 */
			ENABLEZIGBEE = 0;
			sendDevDataOncetoServer();
		}
		// sendDevDataOncetoServer();
		// hello();
		printf("> Please input the data number:\n>");
	    scanf("%s",chartemp);
		if(strcmp("2.1.1",chartemp) == 0) {
			printf("\n Send device data to server\n");
			//sendDevDatatoServer();
		} else if (strcmp("2.1.2",chartemp) == 0) {
			printf("\n Send device info to server\n");
			sendDevinfotoServer(mac, localport);
		} else if (strcmp("start",chartemp) == 0){
			while(1) {
			//	sendDevDatatoServer();
				sleep(5);
			}
		} else {
			printf("\n Please input again! \n");
		}
		
		
	}
	sleep(60);

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

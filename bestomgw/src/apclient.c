/**************************************************************************************************
 Filename:       apclient.c
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
 
/* 管理Zigbee设备相关函数 */
uint8_t tlIndicationCb(epInfo_t *epInfo);
uint8_t newDevIndicationCb(epInfo_t *epInfo);
uint8_t zclGetStateCb(uint8_t state, uint16_t nwkAddr, uint8_t endpoint);
uint8_t zclGetLevelCb(uint8_t level, uint16_t nwkAddr, uint8_t endpoint);
uint8_t zclGetHueCb(uint8_t hue, uint16_t nwkAddr, uint8_t endpoint);
uint8_t zclGetSatCb(uint8_t sat, uint16_t nwkAddr, uint8_t endpoint);
uint8_t zdoSimpleDescRspCb(epInfo_t *epInfo);
uint8_t zdoLeaveIndCb(uint16_t nwkAddr);

static zbSocCallbacks_t zbSocCbs =
{ tlIndicationCb, // pfnTlIndicationCb - TouchLink Indication callback
		newDevIndicationCb, // pfnNewDevIndicationCb - New Device Indication callback
		zclGetStateCb, //pfnZclGetStateCb - ZCL response callback for get State
		zclGetLevelCb, //pfnZclGetLevelCb_t - ZCL response callback for get Level
		zclGetHueCb, // pfnZclGetHueCb - ZCL response callback for get Hue
		zclGetSatCb, //pfnZclGetSatCb - ZCL response callback for get Sat
		zdoSimpleDescRspCb, //pfnzdoSimpleDescRspCb - ZDO simple desc rsp
		zdoLeaveIndCb, // pfnZdoLeaveIndCb - ZDO Leave indication
		NULL,
		NULL
		};
		
void usage(char* exeName)
{
	printf("Usage: ./%s <port>\n", exeName);
	printf("Eample: ./%s /dev/ttyACM0\n", exeName);
}

		
		
void sendDatatoServer(char *str);
void socketClientCb(msgData_t *msg);


int main(int argc, char *argv[])
{
	char servername[20];
	char str[22];
	char mac[22];
	int localport;
	int zbSoc_fd;
	char dbFilename[MAX_DB_FILENAMR_LEN];
	printf("%s -- %s %s\n", argv[0], __DATE__, __TIME__);
	
	// int i;	

	/* 
	 * Client 默认连接的IP地址和端口可变
	*/
	sprintf(str,"%s","172.16.23.100:11235");
	
	if (argc < 1) {
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
	// if (argc != 2)
	// {
		// //usage(argv[0]);
		// printf("attempting to use /dev/ttyACM0\n");
		
		// zbSoc_fd = zbSocOpen("/dev/ttyUSB0");

		// //if (zbSoc_fd<0 )
		// //	{zbSoc_fd = zbSocOpen("/dev/null");}
		// //init_serial();

	// }
	// else
	// {
		// zbSoc_fd = zbSocOpen(argv[1]);
	// }

	// if (zbSoc_fd == -1)
	// {
		// exit(-1);
	// }

	//printf("zllMain: restoring device, group and scene lists\n");
	sprintf(dbFilename, "%.*s/devicelistfile.dat",
			strrchr(argv[0], '/') - argv[0], argv[0]);
	printf("zllMain: device DB file name %s\n", dbFilename);
	devListInitDatabase(dbFilename);
	
/*
	sprintf(dbFilename, "%.*s/grouplistfile.dat", strrchr(argv[0], '/') - argv[0],
			argv[0]);
	printf("zllMain: group DB file name %s\n", dbFilename);
	groupListInitDatabase(dbFilename);

	sprintf(dbFilename, "%.*s/scenelistfile.dat", strrchr(argv[0], '/') - argv[0],
			argv[0]);
	printf("zllMain: scene DB file name %s\n", dbFilename);
	sceneListInitDatabase(dbFilename);
*/	
	/*
	* 初始化socket通信，（以后需要添加重连机制）
	*/
	//AP_Protocol_init(sendDatatoServer); /* 初始化AP传送至Server的函数 */
	socketClientInit(str, socketClientCb);
	
    getClientLocalPort(&localport, &mac);
	
	printf("**** The MAC addr : %s, the port : %d ******\n",mac,localport);
	
	AP_Protocol_init(sendDatatoServer);
	
	printf("-------sendAPinfotoServer---------------\n");
	sendAPinfotoServer(mac,localport);
	
	
	while(1) {
		struct pollfd zbfds[1];
		int pollRet;
		char chartemp[10];
		
		zbfds[0].fd  = zbSoc_fd;    /* 将Zigbee串口的描述符赋给 poll zbfds数组 */
		zbfds[0].events = POLLIN; 
		poll(zbfds,1,1);           /* 阻塞等待串口接收数据 */
		
		if (zbfds[0].revents) {
			printf("Message from ZLL SoC\n");
			//zbSocProcessRpc();
		}
		
		if (ENABLEZIGBEE == 1) {
			ENABLEZIGBEE = 0;
			sendDevDataOncetoServer();
		}
		// sendDevDataOncetoServer();
		// hello();
		printf("> Please input the data number:\n>");
	    scanf("%s",chartemp);
		if(strcmp("2.1.1",chartemp) == 0) {
			printf("\n Send device data to server\n");
			sendDevDatatoServer();
		} else if (strcmp("2.1.2",chartemp) == 0) {
			printf("\n Send device info to server\n");
			sendDevinfotoServer(mac, localport);
		} else if (strcmp("start",chartemp) == 0){
			while(1) {
				sendDevDatatoServer();
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
	// printf("socketClientCb: cmdId:%x\n",(msg->cmdId));

	// func = srpcProcessIncoming[(msg->cmdId)];
	// if (func)
	// {
		// (*func)(msg->pData);
	// }
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

/************************************************************************
 *
 * 以下为Zigbee相关的调用函数
 *
 *
 */

uint8_t tlIndicationCb(epInfo_t *epInfo)
{
	epInfoExtended_t epInfoEx;

	epInfoEx.epInfo = epInfo;
	epInfoEx.type = EP_INFO_TYPE_NEW;

	devListAddDevice(epInfo);
	SRPC_SendEpInfo(&epInfoEx);

	return 0;
}

uint8_t newDevIndicationCb(epInfo_t *epInfo)
{
	//Just add to device list to store
	devListAddDevice(epInfo);

	return 0;
}

uint8_t zdoSimpleDescRspCb(epInfo_t *epInfo)
{
	epInfo_t *ieeeEpInfo;
	epInfo_t* oldRec;
	epInfoExtended_t epInfoEx;

	printf("zdoSimpleDescRspCb: NwkAddr:0x%04x\n End:0x%02x ", epInfo->nwkAddr,
			epInfo->endpoint);

	//find the IEEE address. Any ep (0xFF), if the is the first simpleDesc for this nwkAddr
	//then devAnnce will enter a dummy entry with ep=0, other wise get IEEE from a previous EP
	ieeeEpInfo = devListGetDeviceByNaEp(epInfo->nwkAddr, 0xFF);
	memcpy(epInfo->IEEEAddr, ieeeEpInfo->IEEEAddr, Z_EXTADDR_LEN);

	//remove dummy ep, the devAnnce will enter a dummy entry with ep=0,
	//this is only used for storing the IEEE address until the  first real EP
	//is enter.
	devListRemoveDeviceByNaEp(epInfo->nwkAddr, 0x00);

	//is this a new device or an update
	oldRec = devListGetDeviceByIeeeEp(epInfo->IEEEAddr, epInfo->endpoint);

	if (oldRec != NULL)
	{
		if (epInfo->nwkAddr != oldRec->nwkAddr)
		{
			epInfoEx.type = EP_INFO_TYPE_UPDATED;
			epInfoEx.prevNwkAddr = oldRec->nwkAddr;
			devListRemoveDeviceByNaEp(oldRec->nwkAddr, oldRec->endpoint); //theoretically, update the database record in place is possible, but this other approach is selected to provide change logging. Records that are marked as deleted soes not have to be phisically deleted (e.g. by avoiding consilidation) and thus the database can be used as connection log
		}
		else
		{
			//not checking if any of the records has changed. assuming that for a given device (ieee_addr+endpoint_number) nothing will change except the network address.
			epInfoEx.type = EP_INFO_TYPE_EXISTING;
		}
	}
	else
	{
		epInfoEx.type = EP_INFO_TYPE_NEW;
	}

	printf("zdoSimpleDescRspCb: NwkAddr:0x%04x Ep:0x%02x Type:0x%02x ", epInfo->nwkAddr,
			epInfo->endpoint, epInfoEx.type);

	if (epInfoEx.type != EP_INFO_TYPE_EXISTING)
	{
		devListAddDevice(epInfo);
		epInfoEx.epInfo = epInfo;
		SRPC_SendEpInfo(&epInfoEx);
	}

	return 0;
}

uint8_t zdoLeaveIndCb(uint16_t nwkAddr)
{
	epInfoExtended_t removeEpInfoEx;

	removeEpInfoEx.epInfo = devListRemoveDeviceByNaEp(nwkAddr, 0xFF);

	while(removeEpInfoEx.epInfo)
	{
		removeEpInfoEx.type = EP_INFO_TYPE_REMOVED;
		SRPC_SendEpInfo(&removeEpInfoEx);
		removeEpInfoEx.epInfo = devListRemoveDeviceByNaEp(nwkAddr, 0xFF);
	}

	return 0;
}

uint8_t zclGetStateCb(uint8_t state, uint16_t nwkAddr, uint8_t endpoint)
{
	SRPC_CallBack_getStateRsp(state, nwkAddr, endpoint, 0);
	return 0;
}

uint8_t zclGetLevelCb(uint8_t level, uint16_t nwkAddr, uint8_t endpoint)
{
	SRPC_CallBack_getLevelRsp(level, nwkAddr, endpoint, 0);
	return 0;
}

uint8_t zclGetHueCb(uint8_t hue, uint16_t nwkAddr, uint8_t endpoint)
{
	SRPC_CallBack_getHueRsp(hue, nwkAddr, endpoint, 0);
	return 0;
}

uint8_t zclGetSatCb(uint8_t sat, uint16_t nwkAddr, uint8_t endpoint)
{
	SRPC_CallBack_getSatRsp(sat, nwkAddr, endpoint, 0);
	return 0;
}


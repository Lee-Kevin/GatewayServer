/**************************************************************************************************
 Filename:       apclient.c
 Revised:        $Date: 2017-08-23 13:23:33 -0700 (Wed, 21 Mar 2012) $
 Revision:       $Revision: 246 $

 Description:    This file contains an apclient for the M1 Gateway sever

 
 **************************************************************************************************/
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

int keyFd;
uint8 waitRsp = 1;

#define CONSOLEDEVICE "/dev/console"

void sendOpenNetwork(uint8_t duration);
void sendDatatoServer(char *str);
void socketClientCb(msgData_t *msg);

typedef uint8_t (*srpcProcessMsg_t)(uint8_t *msg);

srpcProcessMsg_t srpcProcessIncoming[] =
{
		NULL,							//0
		NULL,							//SRPC_NEW_ZLL_DEVICE     0x0001
		NULL, 						//SRPC_RESERVED_1		      0x0002
		NULL, 						//SRPC_RESERVED_2	        0x0003
		NULL, 						//SRPC_RESERVED_3         0x0004
		NULL, 						//SRPC_RESERVED_4         0x0005
		NULL, 						//SRPC_RESERVED_5         0x0006
		NULL, 						//SRPC_GET_DEV_STATE_RSP  0x0007
		NULL, 						//SRPC_GET_DEV_LEVEL_RSP  0x0008
		NULL, 						//SRPC_GET_DEV_HUE_RSP    0x0009
		NULL, 						//SRPC_GET_DEV_SAT_RSP    0x000a
		NULL, //SRPC_ADD_GROUP_RSP      0x000b
		NULL, 						//SRPC_GET_GROUP_RSP      0x000c
		NULL, 						//SRPC_ADD_SCENE_RSP      0x000d
		NULL, 						//SRPC_GET_SCENE_RSP      0x000e
};

int main(int argc, char *argv[])
{
	uint8_t duration;
	char servername[20];
	char str[22];
	int i;	
	uint32_t tmpInt;
		
	/* 
	 * Client 默认连接的IP地址和端口可变
	*/
	sprintf(str,"%s","172.16.23.138:11235");
	duration = 60;
	
	if (argc < 2) {
		printf(
				"Expected 1 and got %d params Usage: %s <duration> <addr mode> <ep> <group name>\n",
				argc, argv[0]);
		printf(
				"Example - open network for 60s: %s 60 127.0.0.1:11235\n",
				argv[0]);
		printf("--The default IP address and port is %s--\n",str);
		printf("\n******************************************************\n",str);
		
	} else {
		for (i=0; i < argc; i++) {
			printf("Argument %d is %s.\n", i, argv[i]);
			if(i == 2) {
				// printf("The argc[2] is %s\n",argv[2]);
				sscanf(argv[2],"%s",str);
				printf("---IP address and port is is %s---\n",str);
			}
		}
		sscanf(argv[1], "%d", &tmpInt);
		duration = (uint8_t) tmpInt;
	}
	
	/*
	* Socket 客户端初始化
	*/
	
	socketClientInit(str, socketClientCb);

	//sendOpenNetwork(duration);
	sendDatatoServer("This is client want to connect");

	sleep(duration);

	socketClientClose();
	printf("\n%%%%%%%%%%%%%%%%%%%% This is Main function EXIT %%%%%%%%%%%%%%%%%%%%%\n");
	return 0;
}

/*********************************************************************
 * @fn          socketClientCb
 *
 * @brief
 *
 * @param
 *
 * @return
 */
void socketClientCb(msgData_t *msg)
{
	srpcProcessMsg_t func;
	printf("\n**************************************************\n");
	printf("\n>This is socketClientCb\n");
	printf("\n> I have got the message: %s\n",msg->pData);
	printf("\n**************************************************\n");
	// printf("socketClientCb: cmdId:%x\n",(msg->cmdId));

	// func = srpcProcessIncoming[(msg->cmdId)];
	// if (func)
	// {
		// (*func)(msg->pData);
	// }
}

void sendDatatoServer(char *str) {
	msgData_t msg;
	// int datalen = 0;
	// // while(*str++ != '\0') {
		// // datalen++;
	// // }
	
	sprintf(msg.pData,"%s",str);
	
	socketClientSendData(&msg);
}

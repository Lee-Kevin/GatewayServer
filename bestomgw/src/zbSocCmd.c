/*
 * zbSocCmd.c
 * Zigbee硬件通信相关函数
 *
 * This module contains the API for the zll SoC Host Interface.
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*********************************************************************
 * INCLUDES
 */
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <string.h>

#include <pthread.h>
#include <poll.h>

#include "zbSocCmd.h"

#include "utils.h"


#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

/*********************************************************************
 * MACROS
 */

#define APPCMDHEADER(len) \
0xFE,                                                                             \
len,   /*RPC payload Len                                      */     \
0x29, /*MT_RPC_CMD_AREQ + MT_RPC_SYS_APP        */     \
0x00, /*MT_APP_MSG                                                   */     \
0x0B, /*Application Endpoint                                  */     \
0x02, /*short Addr 0x0002                                     */     \
0x00, /*short Addr 0x0002                                     */     \
0x0B, /*Dst EP                                                             */     \
0xFF, /*Cluster ID 0xFFFF invalid, used for key */     \
0xFF, /*Cluster ID 0xFFFF invalid, used for key */     \

#define BUILD_UINT16(loByte, hiByte) \
          ((uint16_t)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))

#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
          ((uint32_t)((uint32_t)((Byte0) & 0x00FF) \
          + ((uint32_t)((Byte1) & 0x00FF) << 8) \
          + ((uint32_t)((Byte2) & 0x00FF) << 16) \
          + ((uint32_t)((Byte3) & 0x00FF) << 24)))

/*********************************************************************
 * CONSTANTS
 */
#define ZLL_MT_APP_RPC_CMD_TOUCHLINK          0x01
#define ZLL_MT_APP_RPC_CMD_RESET_TO_FN        0x02
#define ZLL_MT_APP_RPC_CMD_CH_CHANNEL         0x03
#define ZLL_MT_APP_RPC_CMD_JOIN_HA            0x04
#define ZLL_MT_APP_RPC_CMD_PERMIT_JOIN        0x05
#define ZLL_MT_APP_RPC_CMD_SEND_RESET_TO_FN   0x06
#define ZLL_MT_APP_RPC_CMD_START_DISTRIB_NWK  0x07

#define MT_APP_RSP                           0x80
#define MT_APP_ZLL_TL_IND                    0x81
#define MT_APP_ZLL_NEW_DEV_IND               0x82

#define MT_DEBUG_MSG                         0x80

#define COMMAND_LIGHTING_MOVE_TO_HUE  			0x00
#define COMMAND_LIGHTING_MOVE_TO_SATURATION 0x03
#define COMMAND_LEVEL_MOVE_TO_LEVEL 				0x00
#define COMMAND_IDENTIFY                    0x00
#define COMMAND_IDENTIFY_TRIGGER_EFFECT     0x40

/*** Foundation Command IDs ***/
#define ZCL_CMD_READ                                    0x00
#define ZCL_CMD_READ_RSP                                0x01
#define ZCL_CMD_WRITE                                   0x02
#define ZCL_CMD_WRITE_UNDIVIDED                         0x03
#define ZCL_CMD_WRITE_RSP                               0x04
//ZDO
#define MT_ZDO_SIMPLE_DESC_RSP             0x84 
#define MT_ZDO_ACTIVE_EP_RSP               0x85 
#define MT_ZDO_END_DEVICE_ANNCE_IND          0xC1
#define MT_ZDO_LEAVE_IND                     0xC9

//UTIL
#define MT_UTIL_GET_DEVICE_INFO              0x00

// General Clusters
#define ZCL_CLUSTER_ID_GEN_BASIC                             0x0000
#define ZCL_CLUSTER_ID_GEN_IDENTIFY                          0x0003
#define ZCL_CLUSTER_ID_GEN_GROUPS                            0x0004
#define ZCL_CLUSTER_ID_GEN_SCENES                            0x0005
#define ZCL_CLUSTER_ID_GEN_ON_OFF                            0x0006
#define ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL                     0x0008
// Lighting Clusters
#define ZCL_CLUSTER_ID_LIGHTING_COLOR_CONTROL                0x0300

// Data Types
#define ZCL_DATATYPE_BOOLEAN                            0x10
#define ZCL_DATATYPE_UINT8                              0x20
#define ZCL_DATATYPE_INT16                              0x29
#define ZCL_DATATYPE_INT24                              0x2a
#define ZCL_DATATYPE_CHAR_STR                           0x42

/*******************************/
/*** Generic Cluster ATTR's  ***/
/*******************************/
#define ATTRID_BASIC_MODEL_ID                             0x0005
#define ATTRID_ON_OFF                                     0x0000
#define ATTRID_LEVEL_CURRENT_LEVEL                        0x0000

/*******************************/
/*** Lighting Cluster ATTR's  ***/
/*******************************/
#define ATTRID_LIGHTING_COLOR_CONTROL_CURRENT_HUE         0x0000
#define ATTRID_LIGHTING_COLOR_CONTROL_CURRENT_SATURATION  0x0001

/*******************************/
/*** Scenes Cluster Commands ***/
/*******************************/
#define COMMAND_SCENE_STORE                               0x04
#define COMMAND_SCENE_RECALL                              0x05

/*******************************/
/*** Groups Cluster Commands ***/
/*******************************/
#define COMMAND_GROUP_ADD                                 0x00

/* The 3 MSB's of the 1st command field byte are for command type. */
#define MT_RPC_CMD_TYPE_MASK  0xE0

/* The 5 LSB's of the 1st command field byte are for the subsystem. */
#define MT_RPC_SUBSYSTEM_MASK 0x1F

#define MT_RPC_SOF         0xFE

/*******************************/
/*** Bootloader Commands ***/
/*******************************/
#define SB_FORCE_BOOT               0xF8
#define SB_FORCE_RUN               (SB_FORCE_BOOT ^ 0xFF)
#define SB_FORCE_BOOT_1             0x10
#define SB_FORCE_RUN_1             (SB_FORCE_BOOT_1 ^ 0xFF)


typedef enum {
	MT_RPC_CMD_POLL = 0x00,
	MT_RPC_CMD_SREQ = 0x20,
	MT_RPC_CMD_AREQ = 0x40,
	MT_RPC_CMD_SRSP = 0x60,
	MT_RPC_CMD_RES4 = 0x80,
	MT_RPC_CMD_RES5 = 0xA0,
	MT_RPC_CMD_RES6 = 0xC0,
	MT_RPC_CMD_RES7 = 0xE0
} mtRpcCmdType_t;

typedef enum {
	MT_RPC_SYS_RES0, /* Reserved. */
	MT_RPC_SYS_SYS,
	MT_RPC_SYS_MAC,
	MT_RPC_SYS_NWK,
	MT_RPC_SYS_AF,
	MT_RPC_SYS_ZDO,
	MT_RPC_SYS_SAPI, /* Simple API. */
	MT_RPC_SYS_UTIL,
	MT_RPC_SYS_DBG,
	MT_RPC_SYS_APP,
	MT_RPC_SYS_OTA,
	MT_RPC_SYS_ZNP,
	MT_RPC_SYS_SPARE_12,
	MT_RPC_SYS_UBL = 13, // 13 to be compatible with existing RemoTI.
	MT_RPC_SYS_MAX // Maximum value, must be last (so 14-32 available, not yet assigned).
} mtRpcSysType_t;

/************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
 
/********************************************************************
 * 添加自定义内容
 */
// 定义链表，存储串口过来的信息
struct LinkedMsg
{
	ZOCData_t message;
	void *nextMessage;
};

typedef struct LinkedMsg ZOClinkedMsg_t;

ZOClinkedMsg_t *ZoCrxBuf;
// Client data processing buffer
/* 存放串口等待处理的数据 */
ZOClinkedMsg_t *ZoCrxProcBuf;

/*********************************************************************
 * LOCAL VARIABLES
 */
int serialPortFd = 0;
uint8_t transSeqNumber = 0;

static int messageCount = 0;

ZOCRecvProcess_t zbSocProcess;



zbSocCallbacks_t zbSocCb;



/*********************************************************************
 * LOCAL FUNCTIONS
 */
void calcFcs(uint8_t *msg, int size);
static void processRpcSysAppTlInd(uint8_t *TlIndBuff);
static void processRpcSysAppZcl(uint8_t *zclRspBuff);
static void processRpcSysAppZclFoundation(uint8_t *zclRspBuff,
		uint8_t zclFrameLen, uint16_t clusterID, uint16_t nwkAddr, uint8_t endpoint);
static void processRpcSysApp(uint8_t *rpcBuff);
static void processRpcSysDbg(uint8_t *rpcBuff);
static void zbSocTransportWrite(uint8_t* buf, uint8_t len);

/******************自行添加的函数********************************/
static uint8_t checkData(uint8_t *data, uint16_t len);
static uint8_t checkRead(uint8_t *data, uint16_t len);

static pthread_t ZOCRTISThreadId;
static void *ZOCrxThreadFunc (void *ptr);
static void *ZOChandleThreadFunc (void *ptr);

// Mutex to handle rx 互斥锁， pthread_mutex_t 是一个结构体，PTHREAD_MUTEX_INITIALIZER   结构常量
pthread_mutex_t ZOCRxMutex = PTHREAD_MUTEX_INITIALIZER;

// conditional variable to notify that the AREQ is handled
static pthread_cond_t ZOCRxCond;

/*********************************************************************
 * @fn      calcFcs
 *
 * @brief   populates the Frame Check Sequence of the RPC payload.
 *
 * @param   msg - pointer to the RPC message
 *
 * @return  none
 */
void calcFcs(uint8_t *msg, int size)
{
	uint8_t result = 0;
	int idx = 1; //skip SOF
	int len = (size - 1); // skip FCS

	while ((len--) != 0)
	{
		result ^= msg[idx++];
	}

	msg[(size - 1)] = result;
}

/*********************************************************************
 * API FUNCTIONS
 */

/*********************************************************************
 * @fn      zbSocOpen
 *
 * @brief   opens the serial port to the CC253x.
 *
 * @param   devicePath - path to the UART device
 *
 * @return  status
 */
int32_t zbSocOpen(char *devicePath)
{
	struct termios tio;

	/* open the device to be non-blocking (read will return immediatly) */
	serialPortFd = open(devicePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (serialPortFd < 0)
	{
		perror(devicePath);
		printf("%s open failed\n", devicePath);
		return (-1);
	}

	//make the access exclusive so other instances will return -1 and exit
	ioctl(serialPortFd, TIOCEXCL);

	/* c-iflags
	 B115200 : set board rate to 115200
	 CRTSCTS : HW flow control (disabled below)
	 CS8     : 8n1 (8bit,no parity,1 stopbit)
	 CLOCAL  : local connection, no modem contol
	 CREAD   : enable receiving characters*/
	//tio.c_cflag = B38400 | CRTSCTS | CS8 | CLOCAL | CREAD;

tio.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL | CREAD;
	/* c-iflags
	 ICRNL   : maps 0xD (CR) to 0x10 (LR), we do not want this.
	 IGNPAR  : ignore bits with parity erros, I guess it is
	 better to ignStateore an erronious bit then interprit it incorrectly. */
	tio.c_iflag = IGNPAR & ~ICRNL;

	
	tio.c_oflag = 0;
	tio.c_lflag = 0;

    tio.c_cflag &= ~CSIZE;    
    tio.c_cflag |= CS8;     
    tio.c_cflag &= ~CSTOPB;   
    tio.c_cflag &= ~PARENB;   
    tio.c_cflag &= ~CRTSCTS;  
    tio.c_cflag |= (CLOCAL | CREAD);  
       
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  
       
    tio.c_oflag &= ~OPOST;  

	// tio.c_cc[VTIME] = 1;
	// tio.c_cc[VMIN] = 32;
	
	tcflush(serialPortFd, TCIFLUSH);
	tcsetattr(serialPortFd, TCSANOW, &tio);

	//Send the bootloader force boot incase we have a bootloader that waits
	//uint8_t forceBoot[] = {SB_FORCE_RUN, SB_FORCE_RUN_1};
	//zbSocTransportWrite(forceBoot, 2);

	return serialPortFd;
}

/*********************************************************************
 * @fn      init_serial
 *
 * @brief   init the serial port to the CC253x. and create 2 thread for communication
 *
 * @param   devicePath - path to the UART device
 *
 * @return  status
 */

int32_t init_serial(char *devicePath, ZOCRecvProcess_t fun)
{
	serialPortFd = zbSocOpen(devicePath);  
	if(serialPortFd == -1) {
		return -1;
	}
	zbSocProcess = fun;
	
	//printf("\n---------------Going to Create Thread rxThreadFunc------------------------\n");

	if (pthread_create(&ZOCRTISThreadId, NULL, ZOCrxThreadFunc, NULL))
	{
		// thread creation failed
		printf("Failed to create RTIS LNX IPC Client read thread\n");
		return -1;
	}

	/**********************************************************************
	 * Create thread which can handle new messages from the NPI server
	 **********************************************************************/

	//printf("\n---------------Going to Create Thread handleThreadFunc----------------------\n");
	
	if (pthread_create(&ZOCRTISThreadId, NULL, ZOChandleThreadFunc, NULL))
	{
		// thread creation failed
		printf("Failed to create RTIS LNX IPC Client handle thread\n");
		return -1;
	}

	
	return serialPortFd;
}		 

static void *ZOCrxThreadFunc (void *ptr) {
	struct pollfd zbfds[1];
	int done = 0;
	zbfds[0].fd  = serialPortFd;    /* 将Zigbee串口的描述符赋给 poll zbfds数组 */
	zbfds[0].events = POLLIN; 
	debug_printf("\n[DBG] Go in ZOCrxThreadFunc\n");
	do {
		poll(zbfds,1,-1);           /* 阻塞等待串口接收数据 */
		//zbSocOpenNwk(20);
		if (zbfds[0].revents) {
			
			unsigned char buffer[1024*2] = {0};//数据缓冲区
			unsigned char read_buf[1024], data[256];
			int i = 0, head = 0;
			int nready, nread;
			static int dlen = 0, tail = 0;
			
			memset(read_buf, 0, sizeof(read_buf));

			nread = read(serialPortFd, read_buf, sizeof(read_buf));
			if (nread > 0 && nread < (sizeof(buffer) - tail)) {
				memcpy(&buffer[head], read_buf, nread);
				
				if(dlen == 0) {  						  /* 对于第一包数据，需要找到帧头 */
					i = 0;
					while(i < nread && buffer[i] != 0x55 && buffer[i+1] != 0x3A) i++;  // 找到帧头
					if (buffer[i] == 0x55 && buffer[i+1] == 0x3A) {
						memset(data, 0, sizeof(data));
						dlen = buffer[i+2];       		  /* 串口数据里的第三个字节代表 帧长度 */
						tail += (nread-i);                /* 尾部为实际数据的尾部 */
						memcpy(data, &buffer[i], tail);  
						//printf("[DBG], the data len is %d, the index is %d, the tail is %d\n",dlen, i, tail);
					}			
				} else {                                  /* 非第一包数据 */
					memcpy(&data[tail], &buffer[i], nread);  
					tail += nread;
					// printf("[DBG] **the tail is %d\n",tail);
					if(tail >= dlen) { 
						if( checkRead(data, dlen) ) {
							debug_printf("[DBG] Pass the checkRead \n");
							ZOClinkedMsg_t *newMessage = (ZOClinkedMsg_t *) malloc(sizeof(ZOClinkedMsg_t));
							if(newMessage == NULL) {
								done = 1;
								printf("[ERR] Could not allocate memory for ZOC message\n");
								break;
							} else {
								messageCount++;
								memset(newMessage,0,sizeof(ZOClinkedMsg_t));
								memcpy(&(newMessage->message), data, dlen);
							}
							
							debug_printf("[DBG] The data recived is:\n");
							for (int j=0; j<dlen; j++) {
								debug_printf("%2x-",data[j]);
							}
							debug_printf("\n");
							
							// 把读取出来的数据帧，放入链表中
							if(ZoCrxBuf == NULL) {
								ZoCrxBuf = newMessage;
							} else {
								ZOClinkedMsg_t *searchList = ZoCrxBuf;
								while (searchList->nextMessage != NULL) {
									searchList = searchList->nextMessage;
								}
								searchList->nextMessage = newMessage;
							}
						} else {
							printf("Don't pass the checkRead \n");
						}
						dlen = 0;
						tail = 0;
					}
				}
			}	
		}
		
		if (ZoCrxBuf != NULL) {
		    pthread_mutex_lock(&ZOCRxMutex);
			// 把接收链表中的数据，传送给等待处理的链表
		    ZoCrxProcBuf = ZoCrxBuf;			
				// Clear receiving buffer for new messages
		    ZoCrxBuf = NULL;
			debug_printf("[DBG] Copied message list (processed %d messages)...\n",
						messageCount);
				
			debug_printf("[MUTEX] Unlock Mutex (Read)\n");
				// Then unlock the thread so the handle can handle the AREQ
			pthread_mutex_unlock(&ZOCRxMutex);
				
			debug_printf("[MUTEX] Signal message read (Read)...\n");
			
			/*
			* 发送信号给另外一个正在处于阻塞等待状态的线程handle thread. 使其脱离阻塞状态，继续执行。
			* Signal to the handle thread an AREQ message is ready
			*/
			pthread_cond_signal(&ZOCRxCond);
			printf("\n-----I have already send the signal ZOCRxCond ---\n");
			debug_printf("[MUTEX] Signal message read (Read)... sent\n");
		}
		
	} while(!done);

	printf("\n[ERR] Exit ZOCrxThreadFunc\n");
	return ptr;
}

static void *ZOChandleThreadFunc (void *ptr) {
	uint8_t done = 0, tryLockFirstTimeOnly = 0;
	printf("\n[DBG] This is ZOChandleThreadFunc \n");
	
	do {
		if(tryLockFirstTimeOnly == 0) {
			debug_printf("[MUTEX] Lock ZOC Mutex (Handle)\n");
			pthread_mutex_lock(&ZOCRxMutex);

			debug_printf("\n[MUTEX] AREQ Lock (Handle)\n");
			tryLockFirstTimeOnly = 1; 	
		}
		debug_printf("[MUTEX] Wait for ZOC Rx Cond (Handle) signal...\n");
		pthread_cond_wait(&ZOCRxCond, &ZOCRxMutex);
		debug_printf("[MUTEX] ZOC (Handle) has unlock\n");
		
		ZOClinkedMsg_t *searchList = ZoCrxProcBuf, *clearList;
		while(searchList != NULL) {
			debug_printf("[DBG] The data need to handle is:\n");
			for (int j=0; j<searchList->message.pData[2]; j++) {
				debug_printf("%2x-",searchList->message.pData[j]);
			}
			if(zbSocProcess != NULL) {
				zbSocProcess((ZOClinkedMsg_t *)&(searchList->message));
			}
			// 用完一部分空间，便释放一部分空间
			clearList = searchList;
			searchList = searchList->nextMessage;
			messageCount--;
			memset(clearList,0,sizeof(ZOClinkedMsg_t));
			free(clearList);
		}
	} while(!done);
	
	return ptr;
}



void zbSocClose(void)
{
	tcflush(serialPortFd, TCOFLUSH);
	close(serialPortFd);

	return;
}

/*********************************************************************
 * @fn      zbSocTransportWrite
 *
 * @brief   Write to the the serial port to the CC253x.
 *
 * @param   fd - file descriptor of the UART device
 *
 * @return  status
 */
static void zbSocTransportWrite(uint8_t* buf, uint8_t len)
{

//printf("-->%s,%d serialPortFd=%d \n",__FUNCTION__,__LINE__,serialPortFd);
	write (serialPortFd, buf, len);
	//prt_debug("[DBG] serial write ",buf, len);
	tcflush(serialPortFd, TCOFLUSH);
	return;
}

/*********************************************************************
 * @fn      zbSocRegisterCallbacks
 *
 * @brief   opens the serial port to the CC253x.
 *
 * @param   devicePath - path to the UART device
 *
 * @return  status
 */
void zbSocRegisterCallbacks(zbSocCallbacks_t zbSocCallbacks)
{
	//copy the callback function pointers
	memcpy(&zbSocCb, &zbSocCallbacks, sizeof(zbSocCallbacks_t));
	return;
}

/*********************************************************************
 * @fn      zbSocTouchLink
 *
 * @brief   Send the touchLink command to the CC253x.
 *
 * @param   none
 *
 * @return  none
 */
void zbSocTouchLink(void)
{

}

/*********************************************************************
 * @fn      zbSocBridgeStartNwk
 *
 * @brief   Send the start network command to the CC253x.
 *
 * @param   none
 *
 * @return  none
 */
void zbSocBridgeStartNwk(void)
{
}

/*********************************************************************
 * @fn      zbSocResetToFn
 *
 * @brief   Send the reset to factory new command to the CC253x.
 *
 * @param   none
 *
 * @return  none
 */
void zbSocResetToFn(void)
{

}

/*********************************************************************
 * @fn      zbSocSendResetToFn
 *
 * @brief   Send the reset to factory new command to a ZLL device.
 *
 * @param   none
 *
 * @return  none
 */
void zbSocSendResetToFn(void)
{

}

/*********************************************************************
 * @fn      zbSocOpenNwk
 *
 * @brief   Send the open network command to a ZLL device.
 *
 * @param   none
 *
 * @return  none
 */
void zbSocOpenNwk(uint8_t duration) {
	uSOC_packet_t openNetWork;
	
	openNetWork.frame.payload_len = 0x01;
	openNetWork.frame.DeviceID    = 0xFD;
	openNetWork.frame.DeviceType  = ~0xFD;
	memset(openNetWork.frame.payload,0,sizeof(openNetWork.frame.payload));
	openNetWork.frame.payload[0] = 0xFF;
	memset(openNetWork.frame.shortAddr,0,sizeof(openNetWork.frame.shortAddr));
	memset(openNetWork.frame.longAddr,0,sizeof(openNetWork.frame.longAddr));
	zbSocSendCommand(&openNetWork);
}

/*********************************************************************
 * @fn      zbSocSendIdentify
 *
 * @brief   Send identify command to a ZLL light.
 *
 * @param   identifyTime - in s.
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSendIdentify(uint16_t identifyTime, uint16_t dstAddr,
		uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      COMMAND_IDENTIFY_TRIGGER_EFFECT
 *
 * @brief   Send identify command to a ZLL light.
 *
 * @param   effect - effect.
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSendIdentifyEffect(uint8_t effect, uint8_t effectVarient, uint16_t dstAddr,
		uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocSetState
 *
 * @brief   Send the on/off command to a ZLL light.
 *
 * @param   state - 0: Off, 1: On.
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSetState(uint8_t state, uint16_t dstAddr, uint8_t endpoint,
		uint8_t addrMode)
{
}

/*********************************************************************
 * @fn      zbSocSetLevel
 *
 * @brief   Send the level command to a ZLL light.
 *
 * @param   level - 0-128 = 0-100%
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSetLevel(uint8_t level, uint16_t time, uint16_t dstAddr,
		uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocSetHue
 *
 * @brief   Send the hue command to a ZLL light.
 *
 * @param   hue - 0-128 represent the 360Deg hue color wheel : 0=red, 42=blue, 85=green  
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSetHue(uint8_t hue, uint16_t time, uint16_t dstAddr, uint8_t endpoint,
		uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocSetSat
 *
 * @briefSend the satuartion command to a ZLL light.
 *   
 * @param   sat - 0-128 : 0=white, 128: fully saturated color  
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSetSat(uint8_t sat, uint16_t time, uint16_t dstAddr, uint8_t endpoint,
		uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocSetHueSat
 *
 * @brief   Send the hue and satuartion command to a ZLL light.
 *
 * @param   hue - 0-128 represent the 360Deg hue color wheel : 0=red, 42=blue, 85=green  
 * @param   sat - 0-128 : 0=white, 128: fully saturated color  
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocSetHueSat(uint8_t hue, uint8_t sat, uint16_t time, uint16_t dstAddr,
		uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocAddGroup
 *
 * @brief   Add Group.
 *
 * @param   groupId - Group ID of the Scene.
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast. 
 *
 * @return  none
 */
void zbSocAddGroup(uint16_t groupId, uint16_t dstAddr, uint8_t endpoint,
		uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocStoreScene
 *
 * @brief   Store Scene.
 * 
 * @param   groupId - Group ID of the Scene.
 * @param   sceneId - Scene ID of the Scene.
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast. 
 *
 * @return  none
 */
void zbSocStoreScene(uint16_t groupId, uint8_t sceneId, uint16_t dstAddr,
		uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocRecallScene
 *
 * @brief   Recall Scene.
 *
 * @param   groupId - Group ID of the Scene.
 * @param   sceneId - Scene ID of the Scene.
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be controled. 
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast. 
 
 * @return  none
 */
void zbSocRecallScene(uint16_t groupId, uint8_t sceneId, uint16_t dstAddr,
		uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocBind
 *
 * @brief   Recall Scene.
 *
 * @param   
 *
 * @return  none
 */
void zbSocBind(uint16_t srcNwkAddr, uint8_t srcEndpoint, uint8_t srcIEEE[8],
		uint8_t dstEndpoint, uint8_t dstIEEE[8], uint16_t clusterID)
{
	
}

void zbSocGetInfo(void)
{

}

/*********************************************************************
 * @fn      zbSocGetState
 *
 * @brief   Send the get state command to a ZLL light.
 *
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be sent the command.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocGetState(uint16_t dstAddr, uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocGetLevel
 *
 * @brief   Send the get level command to a ZLL light.
 *
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be sent the command.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocGetLevel(uint16_t dstAddr, uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocGetHue
 *
 * @brief   Send the get hue command to a ZLL light.
 *
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be sent the command.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocGetHue(uint16_t dstAddr, uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocGetSat
 *
 * @brief   Send the get saturation command to a ZLL light.
 *
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be sent the command.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocGetSat(uint16_t dstAddr, uint8_t endpoint, uint8_t addrMode)
{

}

/*********************************************************************
 * @fn      zbSocGetModel
 *
 * @brief   Send the get saturation command to a ZLL light.
 *
 * @param   dstAddr - Nwk Addr or Group ID of the Light(s) to be sent the command.
 * @param   endpoint - endpoint of the Light.
 * @param   addrMode - Unicast or Group cast.
 *
 * @return  none
 */
void zbSocGetModel(uint16_t dstAddr, uint8_t endpoint, uint8_t addrMode)
{

}

/*************************************************************************************************
 * @fn      processRpcSysAppTlInd()
 *
 * @brief  process the TL Indication from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
static void processRpcSysAppTlInd(uint8_t *TlIndBuff)
{
	
}

/*************************************************************************************************
 * @fn      processRpcSysAppZcl()
 *
 * @brief  process the ZCL Rsp from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
static void processRpcSysAppZcl(uint8_t *zclRspBuff)
{

}

/*************************************************************************************************
 * @fn      processRpcSysAppZclFoundation()
 *
 * @brief  process the ZCL Rsp from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
static void processRpcSysAppZclFoundation(uint8_t *zclRspBuff,
		uint8_t zclFrameLen, uint16_t clusterID, uint16_t nwkAddr, uint8_t endpoint)
{

}

/*************************************************************************************************
 * @fn      processRpcSysZdoEndDeviceAnnceInd
 *
 * @brief   read and process the RPC ZDO message from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
void processRpcSysZdoEndDeviceAnnceInd(uint8_t *EndDeviceAnnceIndBuff)
{


}

/*************************************************************************************************
 * @fn      processRpcSysZdoActiveEPRsp
 *
 * @brief   read and process the RPC ZDO message from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
void processRpcSysZdoActiveEPRsp(uint8_t *ActiveEPRspBuff)
{

}

/*************************************************************************************************
 * @fn      processRpcSysZdoSimpleDescRsp
 *
 * @brief   read and process the RPC ZDO message from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
void processRpcSysZdoSimpleDescRsp(uint8_t *SimpleDescRspBuff)
{
}

void processRpcSysZdoLeaveInd(uint8_t *LeaveIndRspBuff)
{

}

void processRpcUtilGetDevInfoRsp(uint8_t *GetDevInfoRsp)
{

}

/*************************************************************************************************
 * @fn      processRpcSysApp()
 *
 * @brief   read and process the RPC App message from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
static void processRpcSysApp(uint8_t *rpcBuff)
{

}

/*************************************************************************************************
 * @fn      processRpcSysZdo()
 *
 * @brief   read and process the RPC ZDO messages from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
void processRpcSysZdo(uint8_t *rpcBuff)
{

}

/*************************************************************************************************
 * @fn      processRpcSysUtil()
 *
 * @brief   read and process the RPC UTIL messages from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
void processRpcSysUtil(uint8_t *rpcBuff)
{

}

/* @fn      processRpcSysDbg()
 *
 * @brief   read and process the RPC debug message from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
static void processRpcSysDbg(uint8_t *rpcBuff)
{

}

/*************************************************************************************************
 * @fn      zbSocProcessRpc()
 *
 * @brief   read and process the RPC from the ZLL controller
 *
 * @param   none
 *
 * @return  length of current Rx Buffer
 *************************************************************************************************/
void zbSocProcessRpc(void)
{

}

int serial_recv_process(unsigned char *read_buf, unsigned int n)
{
	
	printf("***This is serial_recv_process \n");
	
	return 0;
}

/*************************************************************************************************
 * @fn      zbSocSendCommand()
 *
 * @brief   read and process the data from the Zigbee controller
 *
 * @param   none
 *
 * @return  
 *************************************************************************************************/

void zbSocSendCommand(uSOC_packet_t *packet) {
	uSOC_packet_t __packet = *packet;
	// uSOC_packet_t __packet = *packet;
	__packet.frame.head_1		 = 0x55;
	__packet.frame.head_2		 = 0x3A;
	__packet.frame.length		 = 0x20;
	__packet.frame.seqnum		 = transSeqNumber++;
	__packet.frame.version[0]	 = 0x01;
	__packet.frame.version[1]    = 0x00;
	__packet.frame.cmdtype 		 = 0x02;  // 系统下发命令
	// __packet.frame.payload_len= 0x55;
	// __packet.frame.DeviceID   = 0x55;
	// __packet.frame.DeviceType = 0x55;
	// __packet.frame.payload    = 0x55;
    __packet.frame.addrFlag  	 = 0x3B;
    // __packet.frame.shortAddr  = 0x3B;
    // __packet.frame.longAddr   = 0x3B;
    __packet.frame.checkSum      = checkData(__packet.data, sizeof(__packet.data)-1);
	//	printf("[DBG] The packetData size is %d\n", sizeof(__packet.data));
	printf("[DBG] The packetData is :");
	for (int i=0; i<sizeof(__packet.data); i++) {
		printf("%2x-",__packet.data[i]);
	}
	
	zbSocTransportWrite(__packet.data, sizeof(__packet.data));
}

/*************************************************************************************************
 * @fn      uint8_t checkData(uint8_t * data, uint16_t len) 
 *
 * @brief   计算校验和
 *
 * @param   none
 * 
 * @return  1 通过校验
 * 			0 未通过校验
 *************************************************************************************************/

uint8_t checkData(uint8_t *data, uint16_t len) {
	uint16_t sum = 0;
	for (uint16_t i=0; i<len-1; i++) {
		sum += data[i];
	}
	printf("[DBG] The checkdata should be is %2x \n", sum&0xFF);
	return sum&0xFF;
}

/*************************************************************************************************
 * @fn      uint8_t checkRead(uint8_t * data, uint16_t len) 
 *
 * @brief   check the data from zigbee
 *
 * @param   none
 * 
 * @return  1 通过校验
 * 			0 未通过校验
 *************************************************************************************************/

uint8_t checkRead(uint8_t *data, uint16_t len) {
	uint16_t sum = 0;
	for (uint16_t i=0; i<len-2; i++) {
		sum += data[i];
	}
	printf("[DBG] 3the checkdata should be is %x \n", sum&255);
	if ( (uint8_t)(sum&0xff) == data[len-1] ) {
		return 1;
	} else {
		return 0;
	}
}
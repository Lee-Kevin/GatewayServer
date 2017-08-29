 /**************************************************************************************************
  Filename:       socket_client.c
  Revised:        $Date: 2012-03-21 17:37:33 -0700 (Wed, 21 Mar 2012) $
  Revision:       $Revision: 246 $

  Description:    This file contains Linux platform specific RemoTI (RTI) API
                  Surrogate implementation

  Copyright (C) {2012} Texas Instruments Incorporated - http://www.ti.com/


   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

     Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the
     distribution.

     Neither the name of Texas Instruments Incorporated nor the names of
     its contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  The following guide and a small portion of the code from Beej's Guide to Unix 
  IPC was used in the development of this software:  
  http://beej.us/guide/bgipc/output/html/multipage/intro.html#audience. 
  The code is Public Domain   
**************************************************************************************************/

/**************************************************************************************************
 *                                           Includes
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>


#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#ifndef NPI_UNIX
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include "socket_client.h"

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

#define msg_memcpy(src, dst, len)	memcpy(src, dst, len)


/**************************************************************************************************
 *                                        Externals
 **************************************************************************************************/

/**************************************************************************************************
 *                                           Constant
 **************************************************************************************************/

/**************************************************************************************************
 *                                        Type definitions
 **************************************************************************************************/

struct LinkedMsg
{
	msgData_t message;
	void *nextMessage;
};

typedef struct LinkedMsg linkedMsg;

#define TX_BUF_SIZE			(2 * (sizeof(msgData_t)))


/**************************************************************************************************
 *                                        Global Variables
 **************************************************************************************************/

/**************************************************************************************************
 *                                        Local Variables
 **************************************************************************************************/

// Client socket handle
int sClientFd;
// Client data transmission buffers
// char socketBuf[2][TX_BUF_SIZE];
char socketBuf[2][MY_AP_MAX_BUF_LEN];
// Client data received buffer
linkedMsg *rxBuf;
// Client data processing buffer
/* 存放客户端等待处理的数据 */
linkedMsg *rxProcBuf;

// Message count to keep track of incoming and processed messages
static int messageCount = 0;

static pthread_t RTISThreadId;
static void *rxThreadFunc (void *ptr);
static void *handleThreadFunc (void *ptr);

// Mutex to handle rx 互斥锁， pthread_mutex_t 是一个结构体，PTHREAD_MUTEX_INITIALIZER   结构常量
pthread_mutex_t clientRxMutex = PTHREAD_MUTEX_INITIALIZER;

// conditional variable to notify that the AREQ is handled
static pthread_cond_t clientRxCond;

#ifndef NPI_UNIX
struct addrinfo *resAddr;
#endif

socketClientCb_t socketClientRxCb;

/**************************************************************************************************
 *                                     Local Function Prototypes
 **************************************************************************************************/
static void initSyncRes(void);
static void delSyncRes(void);



/**************************************************************************************************
 *
 * @fn          socketClientInit
 *
 * @brief       This function initializes RTI Surrogate
 *
 * input parameters
 *
 * @param       ipAddress - path to the NPI Server Socket
 *
 * output parameters
 *
 * None.
 *
 * @return      1 if the surrogate module started off successfully.
 *              0, otherwise.
 *
 **************************************************************************************************/
int socketClientInit(const char *devPath, socketClientCb_t cb)
{
	int res = 1, i = 0;
	const char *ipAddress = "", *port = "";
	char *pStr, strTmp[128];
	
	/*
	* 1. 接收注册的回调函数，然后在信息处理线程里面进行处理 
	* 2. 在handleThreadFunc函数中调用
	* 3. 该函数的参数为 接收到的，需要处理的信息
	*/
	socketClientRxCb = cb;
	
	strncpy(strTmp, devPath, 128);
	// use strtok to split string and find IP address and port;
	// the format is = IPaddress:port
	// Get first token
	pStr = strtok (strTmp, ":");
	while ( pStr != NULL)
	{
		if (i == 0)
		{
			// First part is the IP address
			ipAddress = pStr;
		}
		else if (i == 1)
		{
			// Second part is the port
			port = pStr;
		}
		i++;
		if (i > 2)
			break;
		// Now get next token
		pStr = strtok (NULL, " ,:;-|");
	}

	/**********************************************************************
	 * Initiate synchronization resources
	 * 初始化需要同步的资源，互斥锁，条件变量等
	 */
	initSyncRes();

	/**********************************************************************
	 * Connect to the NPI server
	 **********************************************************************/

#ifdef NPI_UNIX
    int len;
    struct sockaddr_un remote;

    if ((sClientFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
#else
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

	if (port == NULL)
	{
		// Fall back to default if port was not found in the configuration file
		printf("Warning! Port not sent to RTIS. Will use default port: %s", DEFAULT_PORT);

	    if ((res = getaddrinfo(ipAddress, DEFAULT_PORT, &hints, &resAddr)) != 0)
	    {
	    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
	    	res = 2;
	    }
	    else
	    {
	    	// Because of inverted logic on return value
	    	res = 1;
	    }
	}
	else
	{
	    printf("Port: %s\n\n", port);
	    if ((res = getaddrinfo(ipAddress, port, &hints, &resAddr)) != 0)
	    {
	    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
	    	res = 2;
	    }
	    else
	    {
	    	// Because of inverted logic on return value
	    	res = 1;
	    }
	}

    printf("IP addresses for %s:\n\n", ipAddress);

    struct addrinfo *p;
    char ipstr[INET6_ADDRSTRLEN];
    for(p = resAddr;p != NULL; p = p->ai_next) {
        void *addr;
        char *ipver;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6:
        if (p->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        // convert the IP to a string and print it:
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        printf("  %s: %s\n", ipver, ipstr);
    }


    if ((sClientFd = socket(resAddr->ai_family, resAddr->ai_socktype, resAddr->ai_protocol)) == -1) {
        perror("socket");
        exit(1);
    }
#endif

    printf("Trying to connect...\n");

#ifdef NPI_UNIX
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, ipAddress);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(sClientFd, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        res = 0;
    }
#else
    if (connect(sClientFd, resAddr->ai_addr, resAddr->ai_addrlen) == -1) {
        perror("connect");
		printf("\nThere is something wrong\n");
        res = 0;
    }
#endif

    if (res == 1) {
		printf("Connected.\n");
		/*
		* 当Client连接至server之后，获取本地端口
		*/
		// printf("Want to get the client port.\n");
		// char guest_ip[20];
		// struct sockaddr_in guest;
		// socklen_t guest_len = sizeof(guest);
		// getsockname(sClientFd, (struct sockaddr *)&guest, &guest_len);  
		// //inet_ntop(AF_INET, &guest.sin_addr, guest_ip, sizeof(guest_ip)); 
		// printf("Client  %s:%d\n",guest_ip, ntohs(guest.sin_port)); 
		//getClientLocalPort();
	}
    	


	int no = 0;
	// allow out-of-band data
	if (setsockopt(sClientFd, SOL_SOCKET, SO_OOBINLINE, &no, sizeof(int)) == -1)
	{
		perror("setsockopt");
		res = 0;
	}

	/**********************************************************************
	 * Create thread which can read new messages from the NPI server
	 **********************************************************************/

	//printf("\n---------------Going to Create Thread rxThreadFunc------------------------\n");

	if (pthread_create(&RTISThreadId, NULL, rxThreadFunc, NULL))
	{
		// thread creation failed
		printf("Failed to create RTIS LNX IPC Client read thread\n");
		return -1;
	}

	/**********************************************************************
	 * Create thread which can handle new messages from the NPI server
	 **********************************************************************/

	//printf("\n---------------Going to Create Thread handleThreadFunc----------------------\n");
	
	if (pthread_create(&RTISThreadId, NULL, handleThreadFunc, NULL))
	{
		// thread creation failed
		printf("Failed to create RTIS LNX IPC Client handle thread\n");
		return -1;
	}

	return res;
}

static void *handleThreadFunc (void *ptr)
{
	int done = 0, tryLockFirstTimeOnly = 0;
	
	// Handle message from socket
	printf("\n #############I am in the handleThreadFunc ##############\n");
	do {
		
		/*
		* 1. 在该函数进行尝试上锁的操作，如果不能上锁，将对资源进行等待。
		* 2. 执行pthread_cond_wait(&clientRxCond, &clientRxMutex); 将进行解锁操作。
		* 3. 为了防止其他线程正在上锁的资源被异常解锁，将会进行尝试上锁操作
		* 4. 只存在线程第一次运行时，其他时刻，可以先解锁互斥锁，然后释放信号量
		*/

		if (tryLockFirstTimeOnly == 0) {
			// Lock mutex
			debug_printf("[MUTEX] Lock AREQ Mutex (Handle)\n");
			pthread_mutex_lock(&clientRxMutex);

			debug_printf("\n[MUTEX] AREQ Lock (Handle)\n");
			tryLockFirstTimeOnly = 1;  
		}
		
		// Conditional wait for the response handled in the Rx handling thread,
		debug_printf("[MUTEX] Wait for Rx Cond (Handle) signal...\n");
		pthread_cond_wait(&clientRxCond, &clientRxMutex);
		debug_printf("[MUTEX] (Handle) has lock\n");

		// Walk through all received AREQ messages before releasing MUTEX
		linkedMsg *searchList = rxProcBuf, *clearList;
		while (searchList != NULL)
		{
			debug_printf("\n\n[DBG] Processing \t@ 0x%.16X next \t@ 0x%.16X\n",
					(unsigned int)searchList,
					(unsigned int)(searchList->nextMessage));

		  if( socketClientRxCb != NULL)
		  {
			  debug_printf("[MUTEX] Calling socketClientRxCb (Handle)...\n");
			  socketClientRxCb((msgData_t *)&(searchList->message));
			}

			debug_printf("[MUTEX] (Handle) (message @ 0x%.16X)...\n", (unsigned int)searchList);

			clearList = searchList;
			// Set search list to next message
			searchList = searchList->nextMessage;
			// Free processed buffer
			if (clearList == NULL)
			{
				// Impossible error, must abort
				done = 1;
				printf("[ERR] clearList buffer was already free\n");
				break;
			}
			else
			{
				messageCount--;
				debug_printf("[DBG] Clearing \t\t@ 0x%.16X (processed %d messages)...\n",
						(unsigned int)clearList,
						messageCount);
				memset(clearList, 0, sizeof(linkedMsg));				
				free(clearList);								
			}
		}
		debug_printf("[MUTEX] Signal message(s) handled (Handle) (processed %d messages)...\n", messageCount);

		debug_printf("[DBG] Finished processing (processed %d messages)...\n",
				messageCount);

	} while (!done);
    printf("\n-----I am going to exit handleThreadFunc-----\n");
	return ptr;
}

static void *rxThreadFunc (void *ptr)
{
	int done = 0, n;

	/* thread loop */

	struct pollfd ufds[1];
	int pollRet;
	int tmplen;
	ufds[0].fd = sClientFd;
	ufds[0].events = POLLIN | POLLPRI;
	printf("\n-----I am in the rxThreadFunc-----\n");
	// Read from socket
	do {
		/*
		* poll(pollfd *ufds，unsigned int nfds, int timeout)
		* The unit of timeout is ms.
		*/
		pollRet = poll((struct pollfd*)&ufds, 1, 1);
		// printf("\n-----The pollRet is :%d-----\n",pollRet);
		if (pollRet == -1) {
			// Error occured in poll()
			perror("poll");
		} else if (pollRet == 0) {
			// Timeout, could still be AREQ to process
		} else {
			/*
			* 普通或者优先带数据可读，
			*/
			if (ufds[0].revents & POLLIN) {
				n = recv(sClientFd,
						socketBuf[0],
						MY_AP_MAX_BUF_LEN,
						0); // normal data 
						
			} // 每次读SRPC_FRAME_HDR_SZ --（2）---个字节到缓冲区
			/*
			* 高优先级数据可读
			*/
			if (ufds[0].revents & POLLPRI) {
				/*
				* int recv( SOCKET s,     char FAR *buf,      int len,     int flags     );   
				* -------------------------------------------------------------------------
				* 1.指定接收端socket描述符
				* 2.用来存放接受数据的缓冲区
				* 3.标明缓冲区大小
				* 4. 一般置0
				* recv 函数返回的是缓冲区中的数据个数
				*/
				n = recv(sClientFd,
						socketBuf[0],
						MY_AP_MAX_BUF_LEN,
						MSG_OOB); // out-of-band data
			}
			// printf("\n**************************************************\n");
			// printf("The data that client recived is:\n %s",socketBuf[0]);
			// printf("\n**************************************************\n");
			
			tmplen = strlen(socketBuf[0]);
			
			// tmplen = 0;
			// while(socketBuf[0][tmplen] !='\0') {
				// tmplen++;
			// }
			printf("\nThe data len n is : %d, tmplen is : %d\n",n,tmplen);
			if (n <= 0)  /* 接收函数出错, n=0 代表server 关闭 */
			{
				if (n < 0)
					perror("recv");
				done = 1; // 退出循环
			} else if (tmplen > 0) {
				printf("\nThe data recive successfully\n");
				
				linkedMsg *newMessage = (linkedMsg *) malloc(sizeof(linkedMsg));
				
				if (newMessage == NULL) {
						// Serious error, must abort
					done = 1;
					printf("[ERR] Could not allocate memory for AREQ message\n");
					break;
				} else {
					messageCount++;
					memset(newMessage, 0, sizeof(linkedMsg));
					// debug_printf("\n[DBG] Allocated \t@ 0x%.16X (received\040 %d messages), size:%x...\n",
							// (unsigned int)newMessage,
							// messageCount,
							// sizeof(linkedMsg));
								
					memcpy(&(newMessage->message),
							(uint8_t*)&(socketBuf[0][0]),
							tmplen);
					memset(socketBuf[0], 0, sizeof(socketBuf[0]));
					//printf("\n> I have already copy the message: %s\n",newMessage->message.pData);
					
					// Place message in read list
					if (rxBuf == NULL) {
						// First message in list
						/* 第一次接收到的信息，放在链表的第一个位置 */
						rxBuf = newMessage;				
					} else { // 后面接收的信息，继续放在链表后方
						linkedMsg *searchList = rxBuf;
						// Find last entry and place it here
						while (searchList->nextMessage != NULL)
						{
							searchList = searchList->nextMessage;
						}
						searchList->nextMessage = newMessage;
					}
				}
				
			} else {
				debug_printf("[MUTEX] Unlock Rx Mutex (Read)\n");
				// Then unlock the thread so the handle can handle the AREQ
				pthread_mutex_unlock(&clientRxMutex);
			}

			
			// /********************************************************************************/
			// else if (n == SRPC_FRAME_HDR_SZ) /* 如果前方帧头接收正确 */
			// {
				// // We have received the header, now read out length byte and process it
				// n = recv(sClientFd,
						// (uint8_t*)&(socketBuf[0][SRPC_FRAME_HDR_SZ]),
						// ((msgData_t *)&(socketBuf[0][0]))->len,
						// 0);
				// if (n == ((msgData_t *)&(socketBuf[0][0]))->len) // 接收到正确的数据
				// {
					// int i;
					// debug_printf("Received %d bytes,\t cmdId 0x%.2X, pData:\t",
							// ((msgData_t *)&(socketBuf[0][0]))->len,
							// ((msgData_t *)&(socketBuf[0][0]))->cmdId);
					// for (i = SRPC_FRAME_HDR_SZ; i < (n + SRPC_FRAME_HDR_SZ); i++)
					// {
						// debug_printf(" 0x%.2X", (uint8_t)socketBuf[0][i]);
					// }
					// debug_printf("\n");

  				// // Allocate memory for new message 结构体指针
					// linkedMsg *newMessage = (linkedMsg *) malloc(sizeof(linkedMsg));
					
					// //debug_printf("Freeing new message (@ 0x%.16X)...\n", (unsigned int)newMessage);
					// //free(newMessage);
					
					// if (newMessage == NULL)
					// {
						// // Serious error, must abort
						// done = 1;
						// printf("[ERR] Could not allocate memory for AREQ message\n");
						// break;
					// }
					// else
					// {
						// messageCount++;
						// memset(newMessage, 0, sizeof(linkedMsg));
						// debug_printf("\n[DBG] Allocated \t@ 0x%.16X (received\040 %d messages), size:%x...\n",
								// (unsigned int)newMessage,
								// messageCount,
								// sizeof(linkedMsg));
					// }
					
					// debug_printf("Filling new message (@ 0x%.16X)...\n", (unsigned int)newMessage);

					// // Copy AREQ message into AREQ buffer
					// memcpy(&(newMessage->message),
							// (uint8_t*)&(socketBuf[0][0]),
							// (((msgData_t *)&(socketBuf[0][0]))->len + SRPC_FRAME_HDR_SZ));

					// // Place message in read list
					// if (rxBuf == NULL) {
						// // First message in list
						// /* 第一次接收到的信息，放在链表的第一个位置 */
						// rxBuf = newMessage;				
					// } else { // 后面接收的信息，继续放在链表后方
						// linkedMsg *searchList = rxBuf;
						// // Find last entry and place it here
						// while (searchList->nextMessage != NULL)
						// {
							// searchList = searchList->nextMessage;
						// }
						// searchList->nextMessage = newMessage;
					// }
				// }
				// else
				// {
					// // Serious error
					// printf("ERR: Incoming Rx has incorrect length field; %d\n",
							// ((msgData_t *)&(socketBuf[0][0]))->len);

					// debug_printf("[MUTEX] Unlock Rx Mutex (Read)\n");
					// // Then unlock the thread so the handle can handle the AREQ
					// pthread_mutex_unlock(&clientRxMutex);
				// }
		  // }
		  // else
		  // {
		  	// // Impossible. ... ;)
		  // }
		/***********************************************************************************/  
		}

		/*
		* Handle thread must make sure it has finished its list. See if there are new messages to move over
		* 清空rxBuf 并把接收的数据内容交由rxProcBuf，并向 handle thread 传递信号量 clientRxCond
		*/
		
		if (rxBuf != NULL)
		{
		    pthread_mutex_lock(&clientRxMutex);
		    
				// Move list over for processing
		    rxProcBuf = rxBuf;			
				
				// Clear receiving buffer for new messages
		    rxBuf = NULL;

			debug_printf("[DBG] Copied message list (processed %d messages)...\n",
						messageCount);
				
			debug_printf("[MUTEX] Unlock Mutex (Read)\n");
				// Then unlock the thread so the handle can handle the AREQ
			pthread_mutex_unlock(&clientRxMutex);
				
			debug_printf("[MUTEX] Signal message read (Read)...\n");
			/*
			* 发送信号给另外一个正在处于阻塞等待状态的线程handle thread. 使其脱离阻塞状态，继续执行。
			* Signal to the handle thread an AREQ message is ready
			*/
			pthread_cond_signal(&clientRxCond);  
			printf("\n-----I have already send the signal clientRxCond ---\n");
			debug_printf("[MUTEX] Signal message read (Read)... sent\n");
		}

	} while (!done);
	printf("\n-----I am going to exit rxThreadFunc-----\n");
	return ptr;
}

/* Initialize thread synchronization resources */
static void initSyncRes(void)
{
  // initialize all mutexes
  // 动态方式 初始化互斥锁
  pthread_mutex_init(&clientRxMutex, NULL);

  // initialize all conditional variables
  // 初始化条件变量
  pthread_cond_init(&clientRxCond, NULL);
}

/* Destroy thread synchronization resources */
static void delSyncRes(void)
{
  // In Linux, there is no dynamically allocated resources
  // and hence the following calls do not actually matter.

  // destroy all conditional variables
  pthread_cond_destroy(&clientRxCond);

  // destroy all mutexes
  pthread_mutex_destroy(&clientRxMutex);

}

/**************************************************************************************************
 *
 * @fn          socketClientClose
 *
 * @brief       This function stops RTI surrogate module
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 *
 **************************************************************************************************/
void socketClientClose(void)
{
	// Close the NPI socket connection

	close(sClientFd);

	// Delete synchronization resources
	delSyncRes();

#ifndef NPI_UNIX
	freeaddrinfo(resAddr); // free the linked-list
#endif //NPI_UNIX

}

/**************************************************************************************************
 *
 * @fn          socketClientSendData
 *
 * @brief       This function sends a message asynchronously over the socket
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 *
 **************************************************************************************************/
void socketClientSendData (msgData_t *pMsg)
{
	int i;
	int datalen=0;
	
	// while(pMsg->pData[datalen] != '\0') {
		// datalen++;
	// }
	datalen = strlen(pMsg->pData);
	// debug_printf("trying to send %d bytes,\t cmdId 0x%.2X, pData:",
			// pMsg->len,
			// pMsg->cmdId);
	// debug_printf("\t");
	// for (i = 0; i < pMsg->len; i++)
	// {
		// debug_printf(" 0x%.2X", pMsg->pData[i]);
	// }
	// debug_printf("\n");

	// if (send(sClientFd, ((uint8_t*)pMsg), pMsg->len + SRPC_FRAME_HDR_SZ , 0) == -1)
	// {
		// perror("send");
		// exit(1);
	// }	
	for (i = 0; i < datalen; i++)
	{
		debug_printf(" 0x%.2X", pMsg->pData[i]);
	}
	debug_printf("\n");

	if (send(sClientFd, ((uint8_t*)pMsg), datalen , 0) == -1)
	{
		perror("send");
		exit(1);
	}
}

	
	/*
	struct sockaddr_in
	{
		unsigned short sin_family;
		unsigned short sin_port;
		struct in_addr sin_addr;
		char sin_zero[8];
	};
	*/


void getClientLocalPort(int *port, char **macaddr) {
	
	struct sockaddr_in guest;
	socklen_t guest_len = sizeof(guest);
	getsockname(sClientFd, (struct sockaddr *)&guest, &guest_len);  
	debug_printf("[DBG]Client  Port :%d\n",ntohs(guest.sin_port));
	
    struct ifreq ifr;  

    strcpy(ifr.ifr_name, "br-lan");  
    ioctl(sClientFd, SIOCGIFHWADDR, &ifr);  
  
    int i;  
    char mac[18];  
    for(i = 0; i < 6; ++i)  
    {  
		if (i == 5) {
			sprintf(mac + 2*i, "%02x", (unsigned char)ifr.ifr_hwaddr.sa_data[i]);  
		} else {
			sprintf(mac + 2*i, "%02x", (unsigned char)ifr.ifr_hwaddr.sa_data[i]);  
		}
    }  
    debug_printf("[DBG]MAC: %s\n",mac);
	
	*port = ntohs(guest.sin_port);
	sprintf(macaddr,"%s",mac);
}



/**************************************************************************************************
 **************************************************************************************************/

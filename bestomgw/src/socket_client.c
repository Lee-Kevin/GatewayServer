 /**************************************************************************************************
  Filename:       socket_client.c
  Revised:        $Date: 2017-10-11
  Revision:       $Revision: 246 $
  Author:		  Jiankai Li

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

#include <sys/select.h>
#include <time.h>

#ifndef NPI_UNIX
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include "socket_client.h"
#include "devicestatus.h"

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
extern uint8_t CheckDevicestatusErrFlag;
 char mac[23];
int localport;
 
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
static uint8_t SocketErrFlag = 1;

static pthread_t SocketThreadId;  // Socket 线程相关的Thread ID

static pthread_t RTISThreadId;
static void *SocketThreadFunc (void *ptr);
static void *rxThreadFunc (void *ptr);
static void *handleThreadFunc (void *ptr);
static void *heartBeatThreadFunc (void *ptr);
static void *sendDeviceStatusFunc (void *ptr);

// Mutex to handle rx 互斥锁， pthread_mutex_t 是一个结构体，PTHREAD_MUTEX_INITIALIZER   结构常量
pthread_mutex_t clientRxMutex = PTHREAD_MUTEX_INITIALIZER;

// There are variables about the heartbeat

#define HeartBeatTimeOut           2
#define HeartBeatTimeInterval      60
#define HeartBeatTimeOutCount      2
pthread_cond_t heartBeatCond;
pthread_mutex_t heartBeatMutex = PTHREAD_MUTEX_INITIALIZER;

// conditional variable to notify that the AREQ is handled
static pthread_cond_t clientRxCond;
static pthread_cond_t socketErrCond;

#ifndef NPI_UNIX
struct addrinfo *resAddr;
#endif

socketClientCb_t    socketClientRxCb;
socketSendApinfo_t  socketApSendinfo;
socketHeartbeat_t   socketHeartbeatFun;
socketSendStatus_t  socketSendStatusFun;
/**************************************************************************************************
 *                                     Local Function Prototypes
 **************************************************************************************************/
static void initSyncRes(void);
static void delSyncRes(void);

/* 注册发送心跳包的调用函数 */

void heartBeatRegisterCallbackFun(socketHeartbeat_t heartbeatfun) {
	socketHeartbeatFun = heartbeatfun;
}

/* 注册发送设备在线状态函数 */

void sendStatusRegisterCallbackFun(socketSendStatus_t sendstatusfun) {
	socketSendStatusFun = sendstatusfun;
}

int socketClientInit(const char *devPath, socketClientCb_t cb,socketSendApinfo_t apsendtoserver) {
	int res = 1;
	/*socketApsendInfo_tsocketSendApinfo_t
	* 1. 接收注册的回调函数，然后在信息处理线程里面进行处理 
	* 2. 在handleThreadFunc函数中调用
	* 3. 该函数的参数为 接收到的，需要处理的信息
	*/
	socketClientRxCb = cb;
	socketApSendinfo = apsendtoserver;
	
	if (pthread_create(&SocketThreadId, NULL, SocketThreadFunc, (void *)devPath)) {
		// thread creation failed
		printf("Failed to create SocketThreadFunc handle thread\n");
		return -1;
	}
	
	return res;
}


static void *SocketThreadFunc (void *ptr) {
	int res = 1, i = 0,tryLockFirstTimeOnly = 0;
	static uint8_t j = 0;
	const char *ipAddress = "", *port = "";
	char *pStr, strTmp[128];
	uint8_t timeInterval[] = {10,20,30,40,50,60,90};
	strncpy(strTmp, (char *)ptr, 128);
	printf("\n #############I am in the SocketThreadFunc ##############\n");
	printf("\nThe strtmp is %s ",strTmp);
	
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
	
	/* 循环进行连接操作 */
	
	do {
		do {
			if ((sClientFd = socket(resAddr->ai_family, resAddr->ai_socktype, resAddr->ai_protocol)) == -1) {
				perror("socket");
				printf("\n[ERR] There is something error with socket connect");
				res = 0;
				// exit(1);
			} else {
				res = 1;
			}
			printf("\nTrying to connect...\n");
			
			if (connect(sClientFd, resAddr->ai_addr, resAddr->ai_addrlen) == -1) {
				perror("connect");
				printf("\nThere is something wrong\n");
				res = 0;
			} else {
				res = 1;
			}
			
			if (res == 1) {
				printf("Connected.\n");
			} else {
				printf("Trying to connect to server after %ds .\n",timeInterval[j]);
				sleep(timeInterval[j]);
				if(++j == 7) {
					j = 0;
				}
			}
		} while (res !=1);

		SocketErrFlag = 0;
		CheckDevicestatusErrFlag = 1;  // Socket 第一次连接成功
		int no = 0;
		// allow out-of-band data
		if (setsockopt(sClientFd, SOL_SOCKET, SO_OOBINLINE, &no, sizeof(int)) == -1)
		{
			perror("setsockopt");
			res = 0;
		}
		
		getClientLocalPort(&localport, &mac);
		
		socketApSendinfo(mac,localport);
		
		initSyncRes();
		
		
		if (pthread_create(&RTISThreadId, NULL, sendDeviceStatusFunc, NULL))
		{
			// thread creation failed
			printf("Failed to create sendDeviceStatusFunc \n");
			// return -1;
		}
		
		if (pthread_create(&RTISThreadId, NULL, rxThreadFunc, NULL))
		{
			// thread creation failed
			printf("Failed to create RTIS LNX IPC Client read thread\n");
			// return -1;
		}
		
		if (pthread_create(&RTISThreadId, NULL, handleThreadFunc, NULL))
		{
			// thread creation failed
			printf("Failed to create RTIS LNX IPC Client handle thread\n");
			// return -1;
		}
		
		if (pthread_create(&RTISThreadId, NULL, heartBeatThreadFunc, NULL))
		{
			// thread creation failed
			printf("Failed to create heartBeatThreadFunc \n");
			// return -1;
		}
		

		if (tryLockFirstTimeOnly == 0) {
			// Lock mutex
			debug_printf("[MUTEX] Lock AREQ Mutex (Handle)\n");
			pthread_mutex_lock(&clientRxMutex);

			debug_printf("\n[MUTEX] AREQ Lock (Handle)\n");
			tryLockFirstTimeOnly = 1;  
		}
		printf("\n\n[DBG] The RTISThreadId is %d\n\n",RTISThreadId);
		pthread_cond_wait(&socketErrCond, &clientRxMutex);
		CheckDevicestatusErrFlag = 2;                      // Socket 断开之后，暂时停止子设备的在线状态检查
		debug_printf("\n[DBG] Wait for the thread end...\n");
		// pthread_join(RTISThreadId,&handleThreadFunc);
		// pthread_join(RTISThreadId,&heartBeatThreadFunc);
		// pthread_join(RTISThreadId,&rxThreadFunc);
		debug_printf("\n\n[DBG] The SocketThreadFunc get the socketErrCond\n");
		j = 0; // the index of the time 
		sClientFd = 0;
		
	} while(1);
}

/*********************************************************************
 * @fn    sendDeviceStatusFunc
 *
 * @brief 发送设备在线状态函数
 *
 * @param ptr, 从父线程传递来的参数
 *
 * @return
 */


static void *sendDeviceStatusFunc (void *ptr) {
	uint8_t done = 0;

	debug_printf("\n[DBG] -----I am in the sendDeviceStatusFunc---------\n");	
	do {

		
		if(SocketErrFlag == 1) {
			done = 1; // there is something wrong
			break;
		} else {
			if (NULL != socketSendStatusFun) {
				socketSendStatusFun();
				// sleep(1);
			}
		}
	}while (!done);
	
	debug_printf("[DBG] I am going to exit heartBeatThreadFunc");
	SocketErrFlag = 1;  // Socket 发生错误  
}

/*********************************************************************
 * @fn    heartBeatThreadFunc
 *
 * @brief 发送心跳包函数
 *
 * @param ptr, 从父线程传递来的参数
 *
 * @return
 */


static void *heartBeatThreadFunc (void *ptr) {
	uint8_t done = 0, tryLockFirstTimeOnly = 0;
	static uint8_t timeoutCount = 0;
	struct timeval temp;
	struct timespec timeout;
	
	srand((int)time(0));  //设置随机数种子

	do {
		if (tryLockFirstTimeOnly == 0) {
			// Lock mutex
			debug_printf("[MUTEX] Lock AREQ Mutex (Handle)\n");
			pthread_mutex_lock(&heartBeatMutex);

			debug_printf("\n[MUTEX] AREQ Lock (Handle)\n");
			tryLockFirstTimeOnly = 1;  
		}
		temp.tv_sec = HeartBeatTimeInterval; // 5s
		
		uint32_t tempdata = (int)(1000.0*random()/(RAND_MAX+1.0));
		temp.tv_usec = 1000*tempdata;
		debug_printf("\n[DBG] I am in the heartBeatThreadFunc\n");	
		debug_printf("[DBG] The Random data is %d\n",tempdata);
		
		select(0,NULL,NULL,NULL,&temp);
		
		if(SocketErrFlag == 1) {
			done = 1; // there is something wrong
			break;
		} else {
			if (NULL != socketHeartbeatFun) {
				socketHeartbeatFun(mac);
				gettimeofday(&temp,NULL);  // get now time 
				timeout.tv_sec = temp.tv_sec + HeartBeatTimeOut;
				if (ETIMEDOUT == pthread_cond_timedwait(&heartBeatCond,&heartBeatMutex,&timeout)) {
					if (++timeoutCount >= HeartBeatTimeOutCount) {
						// 超时处理
						// SocketErrFlag = 1;
						// break;
					}
					
					debug_printf("[DBG] HeartBeatTimeOut \n");
				} else {
					timeoutCount = 0;
					debug_printf("[DBG] HeartBeat Recive the ack from server \n");
				}
			}	
		}
	}while (!done);
	
	debug_printf("[DBG] I am going to exit heartBeatThreadFunc");
	SocketErrFlag = 1;  // Socket 发生错误  
}


static void *handleThreadFunc (void *ptr)
{
	int done = 0, tryLockFirstTimeOnly = 0;
	
	// Handle message from socket
	printf("\n #############I am in the handleThreadFunc ##############\n");
	
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);            /* 设置线程的状态是可以被取消的 */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);	   /* 立即执行，马上退出线程       */
	
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
		if (SocketErrFlag == 1) {
			done = 1;
			break;
		}
		
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
	pthread_mutex_unlock(&clientRxMutex);
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
	
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);            /* 设置线程的状态是可以被取消的 */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);	   /* 立即执行，马上退出线程       */
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
		
		if(SocketErrFlag == 1) {
			done = 1;
			break;
		}

	} while (!done);
	SocketErrFlag = 1;  // Socket 发生错误
	pthread_mutex_unlock(&clientRxMutex);
	pthread_cond_signal(&clientRxCond); 
	/* 释放Socket 错误信号 */
	pthread_mutex_unlock(&clientRxMutex);
	pthread_cond_signal(&socketErrCond); 
	printf("\n-----I am going to exit rxThreadFunc-----\n");
	return ptr;
}

/* Initialize thread synchronization resources */
static void initSyncRes(void)
{
  // initialize all mutexes
  // 动态方式 初始化互斥锁
  pthread_mutex_init(&clientRxMutex, NULL);
  pthread_mutex_init(&heartBeatMutex, NULL);

  // initialize all conditional variables
  // 初始化条件变量
  pthread_cond_init(&clientRxCond, NULL);
  pthread_cond_init(&socketErrCond, NULL);
  pthread_cond_init(&heartBeatCond, NULL);
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
	static uint8_t i = 0;
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
	printf("\n[DBG] The sClientFd is %d",sClientFd);
	datalen = strlen(pMsg->pData);
	if(SocketErrFlag == 1) {
		printf("\n[ERR] The sClientFd is %d",sClientFd);
	} else {
		if (send(sClientFd, ((uint8_t*)pMsg), datalen , 0) == -1)
		{
			perror("send");
			debug_printf("\n[ERR] There is something error on socket, please init the socket again\n");
		}
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

    strcpy(ifr.ifr_name, "eth0");  
    ioctl(sClientFd, SIOCGIFHWADDR, &ifr);  
    int i;  
    char mac[18];  
    for(i = 0; i < 6; ++i)  
    {  
		sprintf(mac + 2*i, "%02x", (unsigned char)ifr.ifr_hwaddr.sa_data[i]);  
    }  
    debug_printf("[DBG]MAC: %s\n",mac);
	
	*port = ntohs(guest.sin_port);
	sprintf(macaddr,"%s",mac);
}



/**************************************************************************************************
 **************************************************************************************************/

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

 // int socketClientInit(const char *devPath, socketClientCb_t cb)
// {
	// int res = 1, i = 0;
	// const char *ipAddress = "", *port = "";
	// char *pStr, strTmp[128];
	
	// /*
	// * 1. 接收注册的回调函数，然后在信息处理线程里面进行处理 
	// * 2. 在handleThreadFunc函数中调用
	// * 3. 该函数的参数为 接收到的，需要处理的信息
	// */
	// socketClientRxCb = cb;
	
	// // if (pthread_create(&SocketThreadId, NULL, SocketThreadFunc, (void *)devPath)) {
		// // // thread creation failed
		// // printf("Failed to create SocketThreadFunc handle thread\n");
		// // return -1;
	// // }
	
	// strncpy(strTmp, devPath, 128);
	// // use strtok to split string and find IP address and port;
	// // the format is = IPaddress:port
	// // Get first token
	// pStr = strtok (strTmp, ":");
	// while ( pStr != NULL)
	// {
		// if (i == 0)
		// {
			// // First part is the IP address
			// ipAddress = pStr;
		// }
		// else if (i == 1)
		// {
			// // Second part is the port
			// port = pStr;
		// }
		// i++;
		// if (i > 2)
			// break;
		// // Now get next token
		// pStr = strtok (NULL, " ,:;-|");
	// }

	// /**********************************************************************
	 // * Initiate synchronization resources
	 // * 初始化需要同步的资源，互斥锁，条件变量等
	 // */
	// initSyncRes();

	// /**********************************************************************
	 // * Connect to the NPI server
	 // **********************************************************************/

// #ifdef NPI_UNIX
    // int len;
    // struct sockaddr_un remote;

    // if ((sClientFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        // perror("socket");
        // exit(1);
    // }
// #else
    // struct addrinfo hints;

    // memset(&hints, 0, sizeof(hints));
    // hints.ai_family = AF_UNSPEC;
    // hints.ai_socktype = SOCK_STREAM;

	// if (port == NULL)
	// {
		// // Fall back to default if port was not found in the configuration file
		// printf("Warning! Port not sent to RTIS. Will use default port: %s", DEFAULT_PORT);

	    // if ((res = getaddrinfo(ipAddress, DEFAULT_PORT, &hints, &resAddr)) != 0)
	    // {
	    	// fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
	    	// res = 2;
	    // }
	    // else
	    // {
	    	// // Because of inverted logic on return value
	    	// res = 1;
	    // }
	// }
	// else
	// {
	    // printf("Port: %s\n\n", port);
	    // if ((res = getaddrinfo(ipAddress, port, &hints, &resAddr)) != 0)
	    // {
	    	// fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
	    	// res = 2;
	    // }
	    // else
	    // {
	    	// // Because of inverted logic on return value
	    	// res = 1;
	    // }
	// }

    // printf("IP addresses for %s:\n\n", ipAddress);

    // struct addrinfo *p;
    // char ipstr[INET6_ADDRSTRLEN];
    // for(p = resAddr;p != NULL; p = p->ai_next) {
        // void *addr;
        // char *ipver;

        // // get the pointer to the address itself,
        // // different fields in IPv4 and IPv6:
        // if (p->ai_family == AF_INET) { // IPv4
            // struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            // addr = &(ipv4->sin_addr);
            // ipver = "IPv4";
        // } else { // IPv6
            // struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            // addr = &(ipv6->sin6_addr);
            // ipver = "IPv6";
        // }

        // // convert the IP to a string and print it:
        // inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        // printf("  %s: %s\n", ipver, ipstr);
    // }


    // if ((sClientFd = socket(resAddr->ai_family, resAddr->ai_socktype, resAddr->ai_protocol)) == -1) {
        // perror("socket");
        // exit(1);
    // }
// #endif

    // printf("Trying to connect...\n");

// #ifdef NPI_UNIX
    // remote.sun_family = AF_UNIX;
    // strcpy(remote.sun_path, ipAddress);
    // len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    // if (connect(sClientFd, (struct sockaddr *)&remote, len) == -1) {
        // perror("connect");
        // res = 0;
    // }
// #else
    // if (connect(sClientFd, resAddr->ai_addr, resAddr->ai_addrlen) == -1) {
        // perror("connect");
		// printf("\nThere is something wrong\n");
        // res = 0;
    // }
// #endif

    // if (res == 1) {
		// printf("Connected.\n");
	// }
    	


	// int no = 0;
	// // allow out-of-band data
	// if (setsockopt(sClientFd, SOL_SOCKET, SO_OOBINLINE, &no, sizeof(int)) == -1)
	// {
		// perror("setsockopt");
		// res = 0;
	// }

	// /**********************************************************************
	 // * Create thread which can read new messages from the NPI server
	 // **********************************************************************/

	// //printf("\n---------------Going to Create Thread rxThreadFunc------------------------\n");

	// if (pthread_create(&RTISThreadId, NULL, rxThreadFunc, NULL))
	// {
		// // thread creation failed
		// printf("Failed to create RTIS LNX IPC Client read thread\n");
		// return -1;
	// }

	// /**********************************************************************
	 // * Create thread which can handle new messages from the NPI server
	 // **********************************************************************/

	// //printf("\n---------------Going to Create Thread handleThreadFunc----------------------\n");
	
	// if (pthread_create(&RTISThreadId, NULL, handleThreadFunc, NULL))
	// {
		// // thread creation failed
		// printf("Failed to create RTIS LNX IPC Client handle thread\n");
		// return -1;
	// }

	// printf("\n\n[DBG] The RTISThreadId is %d\n\n",RTISThreadId);
	
	// return res;
// }
 
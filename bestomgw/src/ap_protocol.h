#ifndef _AP_PROTOCOL_H
#define _AP_PROTOCOL_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

/*
* 函数指针，用户的信息处理函数
*/
typedef void (*sendDatatoServer_t)( char *msg ); 

void data_handle(char* data);

int sendAPinfotoServer(char *mac, int port);

void AP_Protocol_init(sendDatatoServer_t sendfunc);
int sendDevinfotoServer(char *mac, int port);
int sendDevDatatoServer();

/*api result*/
enum protocol_result{
	PROTOCOL_OK = 0,
	PROTOCOL_FAILED,
	PROTOCOL_NO_RSP,
};

/*response result*/
enum rsp_result{
	RSP_FAILED = 0,
	RSP_OK = 1,
};

/* The Frame struct */
struct sFrame_head {
    uint8_t sn;
    uint8_t version;
    uint8_t netflag;
    uint8_t cmdtype;
	uint16_t pdutype;
    char *pdu;
};
typedef struct sFrame_head sFrame_head_t;

/* The Frame struct */
struct device_write {
    char *devId;
    uint16_t type;
    uint16_t value;
    void *next;
};
typedef struct device_write devWriteData_t;


/*******************************************************************************
 网络标识类型定义
*******************************************************************************/
/* 广域网 */
#define NET_TYPE_WAN                             0x01
/* 局域网 */
#define NET_TYPE_LAN                             0x02

/*******************************************************************************
 命令类型定义
*******************************************************************************/
/* 上传 */
#define CMD_TYPE_UPLOAD                          0x01
/* 下发 */
#define CMD_TYPE_SENDDOWN                        0x02
/* 回复 */
#define CMD_TYPE_REPLY                           0x03
/* 事件 */
#define CMD_TYPE_EVENT                           0x04


// int _test_data(void);

/*Download*********************************************************************/
/*APP request AP information*/
#define TYPE_REQ_AP_INFO                         0x0003
/*APP enable/disable device access into net*/
#define TYPE_DEV_NET_CONTROL                     0x0004
/*device write*/
#define TYPE_DEV_WRITE                           0x0005
/*device read*/
#define TYPE_DEV_READ                            0x0006
/*APP request device information */
#define TYPE_REQ_DEV_INFO                        0x000E

/*Upload*********************************************************************/

/*AP report device data to M1*//*M1 report device data to APP*/
#define TYPE_REPORT_DATA                         0x1002
/*M1 report device information to APP*/
#define TYPE_M1_REPORT_AP_INFO					 0x1003
/*M1 report added AP & dev information to APP*/
#define TYPE_M1_REPORT_ADDED_INFO				 0x1004
/*M1 report AP information to APP*/
#define TYPE_M1_REPORT_DEV_INFO					 0x1005
/*M1 report AP information to APP*/
#define TYPE_AP_REPORT_AP_INFO                   0x1006
/*AP report device information to M1*/
#define TYPE_AP_REPORT_DEV_INFO                  0x1007

/*write added device information */
#define TYPE_ECHO_DEV_INFO                       0x4005
/*request added device information*/
#define TYPE_REQ_ADDED_INFO                      0x4006


/*common response*/
#define TYPE_COMMON_RSP							 0x8004




#ifdef __cplusplus
}
#endif

#endif /* _AP_PROTOCOL_H */

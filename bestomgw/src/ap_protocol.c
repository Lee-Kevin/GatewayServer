
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "socket_client.h"
#include "ap_protocol.h"
#include "cJSON.h"

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif



#define VERSION                       "1.0"
#define APNAME                        "ap1"

sendDatatoServer_t APSendMsgtoServer;

sFrame_head_t  Frame_Common;
sFrame_head_t  *pFrame_Common = &Frame_Common;

char *version[]={"1.0","1.1"};


/*
* Local function
*/
static void Frame_packet_send (sFrame_head_t* _pFrame_Common);

/*
* 初始化AP向Server发送数据的函数
*/
void AP_Protocol_init(sendDatatoServer_t sendfunc) {
	APSendMsgtoServer = sendfunc;
}

/*********************************************************************
 * @fn          sendAPinfotoServer
 *
 * @brief 当Client与Server建立通信之后，发送的第一个包
 *
 * @param
 *
 * @return
 */

int sendAPinfotoServer(char *mac, int port) {
    /*
	* 创建JSON对象，存储AP信息
	{  
	   "sn":1,
	   "version":"1.0",
	   "netFlag":1,
	   "cmdType":1,
	   "pdu":{  
		  "pduType":4102,
		  "devDat":{  
			 "port":6688,
			 "apId":"11111111",
			 "apName":"ap1"
		  }
	   }
	}
	*/
	// char * macAddr;
	sFrame_head_t Frame_toSend;
	
	printf(">This is sendAPinfotoServer\n");
    cJSON * pduJsonObject = NULL;
    cJSON * devDataJsonArray = NULL;
    cJSON * devDataObject= NULL;
    cJSON * devArray = NULL;
    cJSON*  devObject = NULL;	
	
    pduJsonObject = cJSON_CreateObject();
    if(NULL == pduJsonObject)
    {
        // create object faild, exit
        cJSON_Delete(pduJsonObject);
        return PROTOCOL_FAILED;
    }
    /*add pdu type to pdu object*/
    cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_AP_REPORT_AP_INFO);
	
	devDataObject = cJSON_CreateObject();
	if (NULL == devDataObject) {
		        // create object faild, exit
        cJSON_Delete(pduJsonObject);
        return PROTOCOL_FAILED;
	}
	cJSON_AddItemToObject(pduJsonObject,"devDat",devDataObject);
	cJSON_AddNumberToObject(devDataObject,"port", port);
	cJSON_AddStringToObject(devDataObject,"apId", mac);
	cJSON_AddStringToObject(devDataObject,"apName",APNAME);
	
	//char * p = cJSON_Print(pJsonRoot);
	
	//debug_printf("The data is ");
	debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));
	
	Frame_toSend.sn         = 1;
	Frame_toSend.version    = 0;
	Frame_toSend.netflag    = NET_TYPE_WAN;
	Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
	Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
	
	Frame_packet_send(&Frame_toSend);
	
	return PROTOCOL_OK;
}

 

void data_handle(char* data)
{
    // int rc = PROTOCOL_NO_RSP;
    // payload_t pdu;
    // rsp_data_t rspData;
    // cJSON* rootJson = NULL;
    // cJSON* pduJson = NULL;
    // cJSON* pduTypeJson = NULL;
    // cJSON* snJson = NULL;
    // cJSON* pduDataJson = NULL;
    // int pduType;

    // rootJson = cJSON_Parse(data);
    // if(NULL == rootJson){
        // printf("rootJson null\n");
        // return;
    // }
    // pduJson = cJSON_GetObjectItem(rootJson, "pdu");
    // if(NULL == pduJson){
        // printf("pdu null\n");
        // return;
    // }
    // pduTypeJson = cJSON_GetObjectItem(pduJson, "pduType");
    // if(NULL == pduTypeJson){
        // printf("pduType null\n");
        // return;
    // }
    // pduType = pduTypeJson->valueint;
    // rspData.pduType = pduType;

    // snJson = cJSON_GetObjectItem(rootJson, "sn");
    // if(NULL == snJson){
        // printf("sn null\n");
        // return;
    // }
    // rspData.sn = snJson->valueint;

    // pduDataJson = cJSON_GetObjectItem(pduJson, "devData");
    // if(NULL == pduDataJson){
        // printf("devData null”\n");

    // }
    // /*pdu*/ 
    // pdu.fdClient = package.fdClient;
    // pdu.pdu = pduDataJson;

    // rspData.fdClient = package.fdClient;
    // printf("pduType:%x\n",pduType);
    // switch(pduType){
                    // case TYPE_REPORT_DATA: rc = AP_report_data_handle(pdu); break;
                    // case TYPE_DEV_READ: APP_read_handle(pdu); break;
                    // case TYPE_DEV_WRITE: rc = APP_write_handle(pdu); break;
                    // case TYPE_ECHO_DEV_INFO: rc = APP_echo_dev_info_handle(pdu); break;
                    // case TYPE_REQ_ADDED_INFO: APP_req_added_dev_info_handle( package.fdClient); break;
                    // case TYPE_DEV_NET_CONTROL: rc = APP_net_control(pdu); break;
                    // case TYPE_REQ_AP_INFO: M1_report_ap_info(package.fdClient); break;
                    // case TYPE_REQ_DEV_INFO: M1_report_dev_info(pdu); break;
                    // case TYPE_AP_REPORT_DEV_INFO: rc = AP_report_dev_handle(pdu); break;
                    // case TYPE_AP_REPORT_AP_INFO: rc = AP_report_ap_handle(pdu); break;

        // default: printf("pdu type not match\n");break;
    // }

    // if(rc != M1_PROTOCOL_NO_RSP){
        // if(rc == M1_PROTOCOL_OK)
            // rspData.result = RSP_OK;
        // else
            // rspData.result = RSP_FAILED;
        // common_rsp(rspData);
    // }

    // cJSON_Delete(rootJson);
}

void Frame_packet_send (sFrame_head_t* _pFrame_Common) {
	
	sFrame_head_t _frame_head = * _pFrame_Common;
	
    cJSON * pJsonRoot = NULL; 
    cJSON * pduJsonObject = NULL;
    cJSON * devDataJsonArray = NULL;
    cJSON * devDataObject= NULL;
    cJSON * devArray = NULL;
    cJSON*  devObject = NULL;	
	
	/*get sql data json*/
    pJsonRoot = cJSON_CreateObject();
    if(NULL == pJsonRoot)
    {
        printf("pJsonRoot NULL\n");
        cJSON_Delete(pJsonRoot);
       // return PROTOCOL_FAILED;
    }
    cJSON_AddNumberToObject(pJsonRoot, "sn", _frame_head.sn);
    cJSON_AddStringToObject(pJsonRoot, "version", version[_frame_head.version]);
    cJSON_AddNumberToObject(pJsonRoot, "netFlag", _frame_head.netflag);
    cJSON_AddNumberToObject(pJsonRoot, "cmdType", _frame_head.cmdtype);
	
	pduJsonObject = cJSON_Parse(_frame_head.pdu);
	cJSON_AddItemToObject(pJsonRoot, "pdu", pduJsonObject);
	
	debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pJsonRoot));
    if (NULL == APSendMsgtoServer) {
		debug_printf("[ERR] Please init the APSendMsgtoServer func \n");
	} else {
		APSendMsgtoServer((char *)cJSON_PrintUnformatted(pJsonRoot));
		debug_printf("[DBG] SEND packet to server successfully \n");
	}
}
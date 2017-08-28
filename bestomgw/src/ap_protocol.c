
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
#define SUCCESS                       1
#define FAILED                        0

sendDatatoServer_t APSendMsgtoServer;

sFrame_head_t  Frame_Common;
sFrame_head_t  *pFrame_Common = &Frame_Common;

char *version[]={"1.0","1.1"};


/*
* Local function
*/
static void Frame_packet_send (sFrame_head_t* _pFrame_Common);
static void Frame_packet_recv (sFrame_head_t* _pFrame_Common);
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
	cJSON_AddItemToObject(pduJsonObject,"devData",devDataObject);
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
	cJSON_Delete(pduJsonObject);
	return PROTOCOL_OK;
}



/*********************************************************************
 * @fn    sendCommReplytoServer
 *
 * @brief 通用回复函数
 *
 * @param 需要回复的包 ，还有结果
 *
 * @return
 */

int sendCommReplytoServer(sFrame_head_t* reply_packet, int result) {

	sFrame_head_t Frame_toSend;
	
	sFrame_head_t Frame_toReply = *reply_packet;
	
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
    cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_COMMON_RSP);
	
	devDataObject = cJSON_CreateObject();
	if (NULL == devDataObject) {
		        // create object faild, exit
        cJSON_Delete(pduJsonObject);
        return PROTOCOL_FAILED;
	}
	
	cJSON_AddItemToObject(pduJsonObject,"devData",devDataObject);
	cJSON_AddNumberToObject(devDataObject,"sn", Frame_toReply.sn);
	cJSON_AddNumberToObject(devDataObject,"pduType", Frame_toReply.pdutype);
	cJSON_AddNumberToObject(devDataObject,"result",result);
	
	debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));
	
	Frame_toSend.sn         = 1;
	Frame_toSend.version    = 0;
	Frame_toSend.netflag    = NET_TYPE_WAN;
	Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
	Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
	
	Frame_packet_send(&Frame_toSend);
	cJSON_Delete(pduJsonObject);
	return PROTOCOL_OK;
}





void data_handle(char* data)
{
    int rc = PROTOCOL_NO_RSP;
    msgData_t pdu;
    sFrame_head_t Frame_RecData;
    cJSON* rootJson = NULL;
    cJSON* pduJson = NULL;

    cJSON* snJson = NULL;
	cJSON* verJson = NULL;
	cJSON* netflagJson = NULL;
	cJSON* cmdtypeJson = NULL;
    cJSON* pduTypeJson = NULL;
    cJSON* pduDataJson = NULL;
	
	printf("**************************************************\n");
	printf("> I have got the message:\n %s\n",data);
	printf("**************************************************\n");	
	
    rootJson = cJSON_Parse(data);
    if(NULL == rootJson){
		debug_printf("[DBG] rootJson null\n");
        return;
    }
    snJson = cJSON_GetObjectItem(rootJson, "sn");
    if(NULL == snJson){
        printf("sn null\n");
        return;
    }
	verJson = cJSON_GetObjectItem(rootJson,"version");
    if(NULL == verJson){
        printf("version null\n");
        return;
    }	
	netflagJson = cJSON_GetObjectItem(rootJson,"netFlag");
    if(NULL == netflagJson){
        printf("netFlag null\n");
        return;
    }
	cmdtypeJson = cJSON_GetObjectItem(rootJson,"netFlag");
    if(NULL == netflagJson){
        printf("version null\n");
        return;
    }
    pduJson = cJSON_GetObjectItem(rootJson, "pdu");
    if(NULL == pduJson){
        printf("pdu null\n");
        return;
    }
    pduTypeJson = cJSON_GetObjectItem(pduJson, "pduType");
    if(NULL == pduTypeJson){
        printf("pduType null\n");
        return;
    }
    pduDataJson = cJSON_GetObjectItem(pduJson, "devData");
    if(NULL == pduDataJson){
        printf("devData null”\n");
		return;

    }

	Frame_RecData.sn      = snJson->valueint;
	Frame_RecData.version = verJson->valueint;
	Frame_RecData.netflag = netflagJson->valueint;
	Frame_RecData.cmdtype = cmdtypeJson->valueint;
    Frame_RecData.pdutype = pduTypeJson->valueint;
	/* pdu 装载的内容为元素“devData” 里的内容 不同于发送时装载的内容 */
	Frame_RecData.pdu     = (char *)(cJSON_PrintUnformatted(pduDataJson));
	
	Frame_packet_recv(&Frame_RecData);
    cJSON_Delete(rootJson);
}

/*********************************************************************
 * @fn     Frame_packet_recv
 *
 * @brief  数据包接收处理函数
 *
 * @param  sFrame_head_t * 
 *
 * @return
 */
 
void Frame_packet_recv (sFrame_head_t* _pFrame_Common) {
	
	sFrame_head_t _frame_head = * _pFrame_Common;
	
	/*get sql data json*/
	cJSON* pJsondevData = NULL;
	cJSON* devDataJson = NULL;
    cJSON* devIdJson = NULL;
    cJSON* paramJson = NULL;
    cJSON* paramDataJson = NULL;
    cJSON* typeJson = NULL;
    cJSON* valueJson = NULL;
	uint8_t myresult = 0;
	/*
	** 提取数据包里的“devData" 内容
	*/
	//printf("I am going to deal with cJSON_Parse\n");
    pJsondevData = cJSON_Parse(_frame_head.pdu);

    if(NULL == pJsondevData)
    {
        printf("pJsonRoot NULL\n");
        cJSON_Delete(pJsondevData);
       // return PROTOCOL_FAILED;
    }
	
	printf"-----------------------------Frame_packet_recv-----------------------------\n");
	
	switch(_frame_head.pdutype) {
	/* 通用回复处理 */
	case  TYPE_COMMON_RSP:  
		debug_printf("[DBG] Got the ACK from Server Code:%x\n",TYPE_COMMON_RSP);
		break;
	/* AP 使能、失使能子设备入网 */	
	case TYPE_DEV_NET_CONTROL: 
		debug_printf("[DBG] This is TYPE_DEV_NET_CONTROL %x\n",TYPE_DEV_NET_CONTROL);
		int result = pJsondevData -> valueint;
		debug_printf("[DBG] The result is : %d\n",result);
		if(result) {
			printf("Enable the device to join the ZigBee network\n");
		} else {
			printf("Disable the device to join the ZigBee network\n");
		}
		myresult = SUCCESS;
		break;	
	/* 子设备控制命令 写入*/
	case TYPE_DEV_WRITE:
		debug_printf("[DBG] This is TYPE_DEV_WRITE %x\n",TYPE_DEV_WRITE);
		int number1 = cJSON_GetArraySize(pJsondevData);
		printf("The Array Size is %d",number1);
		for(int i = 0; i < number1; i++){
			
			devDataJson = cJSON_GetArrayItem(pJsondevData,i);
			devIdJson = cJSON_GetObjectItem(devDataJson, "devId");
			printf("devId:%s\n",devIdJson->valuestring);
			paramJson = cJSON_GetObjectItem(devDataJson, "param");
			int number2 = cJSON_GetArraySize(paramJson);
			printf(" number2:%d\n",number2);

			for(int j = 0; j < number2; j++){
				paramDataJson = cJSON_GetArrayItem(paramJson, j);
				typeJson = cJSON_GetObjectItem(paramDataJson, "type");
				printf("  type:%d\n",typeJson->valueint);
				valueJson = cJSON_GetObjectItem(paramDataJson, "value");
				printf("  value:%d\n",valueJson->valueint);				
			}
		}	
		break;	
	default:
		break;
	}
	printf"-----------------------------Frame_packet_recv end-----------------------------\n");
	if (PROTOCOL_OK == sendCommReplytoServer(&_frame_head,myresult)) {
	//	debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pJsonRoot));
	} else {
		
	}
	
	// pduJsonObject = cJSON_Parse(_frame_head.pdu);
	// cJSON_AddItemToObject(pJsonRoot, "pdu", pduJsonObject);
	
	// debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pJsonRoot));
    // if (NULL == APSendMsgtoServer) {
		// debug_printf("[ERR] Please init the APSendMsgtoServer func \n");
	// } else {
		// APSendMsgtoServer((char *)cJSON_PrintUnformatted(pJsonRoot));
		// debug_printf("[DBG] SEND packet to server successfully \n");
	// }
}


/*********************************************************************
 * @fn     Frame_packet_send
 *
 * @brief  数据包打包发送
 *
 * @param
 *
 * @return
 */

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


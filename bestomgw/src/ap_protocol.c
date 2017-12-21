/**************************************************************************************************
 Filename:       ap_protocol.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-08-30 13:23:33 -0700 
 Revision:       

 Description:    This is a file that can deal with the data from M1

 
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>
#include "socket_client.h"
#include "ap_protocol.h"
#include <unistd.h>
#include "cJSON.h"
#include "zbSocCmd.h"
#include "database.h"
#include <sys/ipc.h>
#include "devicestatus.h"
#include "msgqueue.h"

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

#define VERSION                       "1.0"
#define APNAME                        "A2"
#define SUCCESS                       1
#define FAILED                        0

extern int QueueIndex;
sendDatatoServer_t APSendMsgtoServer;

sFrame_head_t  Frame_Common;
sFrame_head_t  *pFrame_Common = &Frame_Common;

char *version[]={"1.0","1.1"};

/*
* Global variable
*/
extern pthread_cond_t heartBeatCond;
extern pthread_mutex_t heartBeatMutex;
extern int DataBaseQueueIndex;
// zbSocDeleteDeviceformNetwork(char* addr)

/*
* Local function
*/
static uint8_t checkData(uint8_t *data, uint16_t len);
static void Frame_packet_send (sFrame_head_t* _pFrame_Common);
static void Frame_packet_recv (sFrame_head_t* _pFrame_Common);

static void DataWritetoDev(devWriteData_t *data_towrite);

/*
* 初始化AP向Server发送数据的函数
*/
void AP_Protocol_init(sendDatatoServer_t sendfunc) {
	APSendMsgtoServer = sendfunc;
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
	
	
//	printf("> I have got the message:\n %s\n",data);
	
	#if PACKET_HEAD
		printf("**************************************************\n");
		
		for (int i=0;i<4;i++) {
			printf("%02x--",data[i]);
		}
		// uint32_t len = strlen(&data[4]);
		// for (int i=0; i<len; i++) {
			// printf("%02x",data[4+i]);
		// }
		// printf("the Data is %s",&data[4]);
		rootJson = cJSON_Parse(&data[4]);
	#else 
		rootJson = cJSON_Parse(data);
	#endif
	debug_printf("\n[DBG] The data Recived is %s\n",(char *)cJSON_PrintUnformatted(rootJson));
	printf("**************************************************\n");	
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
	debug_printf("\n[DBG] Frame_RecData.pdu %s\n",(char *)(cJSON_PrintUnformatted(pduDataJson)));
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
		return;
    }
	
	printf("\n-----------------------------Frame_packet_recv-----------------------------\n");
	
	switch(_frame_head.pdutype) {
	/* 通用回复处理 */
	case  TYPE_COMMON_RSP: {
		cJSON* snJson = NULL;
		cJSON* pduTypeJson = NULL;
		
		snJson = cJSON_GetObjectItem(pJsondevData, "sn");
		if(NULL == snJson){
			printf("sn null\n");
			return;
		}
		pduTypeJson = cJSON_GetObjectItem(pJsondevData, "pduType");
		if(NULL == snJson){
			printf("pduType null\n");
			return;
		}
		/* 得到心跳包回复 */
		if(pduTypeJson->valueint == TYPE_AP_SEND_HEARTBEAT_INFO) {
			// 心跳回复处理
			debug_printf("[DBG] Got the heart beat ACK from Server\n");
			pthread_mutex_unlock(&heartBeatMutex);
			pthread_cond_signal(&heartBeatCond);
		} else {
			debug_printf("[DBG] Got the ACK from Server and Doing Nothing now\n");
		}
	} 
		break;
	/* AP 使能、失使能子设备入网 */	
	case TYPE_DEV_NET_CONTROL:
		if (PROTOCOL_OK == sendCommReplytoServer(&_frame_head,myresult)) {
		} else { /* 发送回复命令失败 */
		}
		debug_printf("[DBG] This is TYPE_DEV_NET_CONTROL %x\n",TYPE_DEV_NET_CONTROL);
		int result = pJsondevData -> valueint;
		debug_printf("[DBG] The result is : %d\n",result);
		if(result) {
			printf("Enable the device to join the ZigBee network\n");
			zbSocOpenNwk(20);
		} else {
			printf("Disable the device to join the ZigBee network\n");
			zbSocCloseNwk();
		}
		myresult = SUCCESS;

		break;	
	/* 子设备控制命令 写入*/
	case TYPE_DEV_WRITE:
		debug_printf("[DBG] This is TYPE_DEV_WRITE %x\n",TYPE_DEV_WRITE);
		if (PROTOCOL_OK == sendCommReplytoServer(&_frame_head,myresult)) {
		} else { /* 发送回复命令失败 */
		}
		int number1 = cJSON_GetArraySize(pJsondevData);
		//printf("The Array Size is %d",number1);
		for(int i = 0; i < number1; i++){
			devWriteData_t WriteData;
			devDataJson = cJSON_GetArrayItem(pJsondevData,i);
			devIdJson = cJSON_GetObjectItem(devDataJson, "devId");
			sprintf(WriteData.devId,"%s",devIdJson->valuestring);
			paramJson = cJSON_GetObjectItem(devDataJson, "param");
			int number2 = cJSON_GetArraySize(paramJson);
			for(int j = 0; j < number2; j++){
				paramDataJson = cJSON_GetArrayItem(paramJson, j);
				typeJson = cJSON_GetObjectItem(paramDataJson, "type");
				WriteData.type = typeJson->valueint;
			//	printf("  type:%d\n",typeJson->valueint);
				valueJson = cJSON_GetObjectItem(paramDataJson, "value");
				WriteData.value = valueJson->valueint;
			//	printf("  value:%d\n",valueJson->valueint);	
				DataWritetoDev(&WriteData);
			}
		}
		myresult = SUCCESS;
		if (PROTOCOL_OK == sendCommReplytoServer(&_frame_head,myresult)) {
		//	debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pJsonRoot));
		} else {
		}
		break;	
	/* 删除子设备 */
	case TYPE_DEV_DELETE: {
		uint8_t _prdID;
		int msgerr;
		debug_printf("[DBG] This is TYPE_DEV_DELETE %x\n",TYPE_DEV_DELETE);
		if (PROTOCOL_OK == sendCommReplytoServer(&_frame_head,myresult)) {
		} else { /* 发送回复命令失败 */
		}
		
		myDataBaseMsg_t msg;
		memset(&msg,0,sizeof(myDataBaseMsg_t));
		msg.msgType = QUEUE_TYPE_DATABASE_SEND;
		msg.Option_type = DATABASE_OPTION_SELECT;
		
		debug_printf("\n[DBG] This is TYPE_DEV_DELETE %x\n",TYPE_DEV_DELETE);

		typeJson = cJSON_GetObjectItem(pJsondevData, "type");
		if(NULL == typeJson){
			printf("typeJson null\n");
			return;
		}
		devIdJson = cJSON_GetObjectItem(pJsondevData, "id");
		if(NULL == devIdJson){
			printf("devIdJson null\n");
			return;
		}
		sprintf(msg.devlist.devID,"%s",devIdJson->valuestring);
		
		debug_printf("\n[DBG] the devid is %s",devIdJson->valuestring);
		debug_printf("\n[DBG]TYPE_DEV_DELETE Begin to send the data to DataBaseQueueIndex\n");
		
		msgerr = msgsnd(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t),0);
		debug_printf("\n[DBG] The TYPE_DEV_DELETE msg err is %d\n",msgerr);
		if ( -1 ==  msgerr) {
			debug_printf("\n[ERR]Can't send the msg \n");
			return;
		} else {
			memset(&msg,0,sizeof(myDataBaseMsg_t));
			msgerr = msgrcv(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), QUEUE_TYPE_DATABASE_GETINFO, MSG_NOERROR);
			if(msgerr >= sizeof(myDataBaseMsg_t)) {
				debug_printf("\n[DBG] I got the msg prd id  %s",msg.devlist.prdID);
				debug_printf("\n[DBG] I got the msg short addr %s",msg.devlist.shortAddr);
				sscanf(msg.devlist.prdID,"%02x",&_prdID);
			}
		}
		if(_prdID != 0) {
			debug_printf("\n[DBG] The prdID is 0x%02x.\n",_prdID);
			zbSocDeleteDevformNetwork(msg.devlist.shortAddr);
			// zbSocDeleteDeviceformNetwork(msg.devlist.shortAddr);
		} else {
			debug_printf("\n[ERR] Can't delete the device can't find the addr.\n");
		}
		
		
	}
	break;
	default:
		break;
	}
	printf("-----------------------------Frame_packet_recv end-----------------------------\n");
	cJSON_Delete(pJsondevData);
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
	cJSON_AddNumberToObject(devDataObject,"pID", AP_ID);
	cJSON_AddStringToObject(devDataObject,"apName",APNAME);
	cJSON_AddStringToObject(devDataObject,"apId", mac);

	
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
	
	//printf(">This is sendCommReplytoServer\n");
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
	
	//debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));
	debug_printf("[DBG] sendCommReplytoServer");
	
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
 * @fn    sendOnlineStatustoServer
 *
 * @brief 从队列 QueueIndex 里读取设备状态信息， 然后发送子设备的状态信息至M1
 *
 * @param 无，需要使用队列全部变量
 *
 * @return
 */

uint8_t sendOnlineStatustoServer() {

	sFrame_head_t Frame_toSend;
	myMsg_t msg;
	int size;
	// pdu_content_t _content_toSend = *devicepdu;
	
	//printf(">This is sendAPinfotoServer\n");
    cJSON * pduJsonObject = NULL;
    // cJSON * devParamJsonArray = NULL;
    cJSON * devDataArrayObject= NULL;
    // cJSON * devArray = NULL;
    cJSON*  devObject = NULL;	
	
	size = msgrcv(QueueIndex, &msg, sizeof(myMsg_t), 1, IPC_NOWAIT);  // 非阻塞式接收消息
    if ( size <= 0 ) // 出错 
    {  
		// debug_printf("\n[DBG] There is nothing recived");
        return PROTOCOL_OK;  
    }  else {
		pduJsonObject = cJSON_CreateObject();
		if(NULL == pduJsonObject)
		{
			// create object faild, exit
			cJSON_Delete(pduJsonObject);
			return PROTOCOL_FAILED;
		}
		/*add pdu type to pdu object*/
		cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_REPORT_DATA);
		
		devDataArrayObject = cJSON_CreateArray();
		if (NULL == devDataArrayObject) {
					// create object faild, exit
			cJSON_Delete(devDataArrayObject);
			return PROTOCOL_FAILED;
		}
		cJSON_AddItemToObject(pduJsonObject,"devData",devDataArrayObject);
		
		
		for (int j=0; j<1; j++) {
			
			cJSON* devDataArrayContent = cJSON_CreateObject();
			
			/*
			** 每一次循环，都往数组里面添加新的内容 
			*/
			cJSON_AddStringToObject(devDataArrayContent,"devName", msg.msgName);
			cJSON_AddStringToObject(devDataArrayContent,"devId", msg.msgDevID);
		
			cJSON * devParamJsonArray = cJSON_CreateArray();
			if (NULL == devParamJsonArray) {
				cJSON_Delete(devParamJsonArray);
				return PROTOCOL_FAILED;
			}
			cJSON_AddItemToObject(devDataArrayContent,"param",devParamJsonArray);
			
			//devparam_t *searchList = &_content_toSend.param, *clearList;
			// devparam_t *searchList = _content_toSend.param, *clearList;
			
			for(int i=0; i<1; i++) {
				cJSON* arrayObject = cJSON_CreateObject();
				
				if(NULL == arrayObject) {
					cJSON_Delete(arrayObject);
					return PROTOCOL_FAILED;
				}
				cJSON_AddNumberToObject(arrayObject,"type", PARAM_TYPE_ONINE_STATUS);
				cJSON_AddNumberToObject(arrayObject,"value",msg.value);
				// memset(clearList, 0, sizeof(devparam_t));
				cJSON_AddItemToArray(devParamJsonArray,arrayObject);
			}
			
			cJSON_AddItemToArray(devDataArrayObject,devDataArrayContent);
		}
		//debug_printf("\n[DBG] After create jsonArray\n");
		//debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));	
		Frame_toSend.sn         = 1;
		Frame_toSend.version    = 0;
		Frame_toSend.netflag    = NET_TYPE_WAN;
		Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
		Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
		
		//debug_printf("\n[DBG] before Frame_packet_send \n");
		Frame_packet_send(&Frame_toSend);
		//debug_printf("\n[DBG] After Frame_packet_send \n");
		cJSON_Delete(pduJsonObject);
		//debug_printf("\n[DBG] After cJSON_Delete \n");
		return PROTOCOL_OK;
	}
}


/*********************************************************************
 * @fn    sendDevinfotoServer
 *
 * @brief 发送子设备基本信息到Server
 *
 * @param mac, port
 *
 * @return
 */

int sendDevinfotoServer(char *mac, int port, pdu_content_t *devicepdu) {

	sFrame_head_t Frame_toSend;
	pdu_content_t _content_toSend = *devicepdu;
	debug_printf(">This is sendAPinfotoServer\n");
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
    cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_AP_REPORT_DEV_INFO);
	
	devDataObject = cJSON_CreateObject();
	if (NULL == devDataObject) {
		        // create object faild, exit
        cJSON_Delete(pduJsonObject);
        return PROTOCOL_FAILED;
	}
	cJSON_AddItemToObject(pduJsonObject,"devData",devDataObject);
	// cJSON_AddNumberToObject(devDataObject,"port", port);
	cJSON_AddStringToObject(devDataObject,"apName",APNAME);
	cJSON_AddStringToObject(devDataObject,"apId", mac);
	
	devDataJsonArray = cJSON_CreateArray();
	if (NULL == devDataJsonArray) {
	    cJSON_Delete(devDataJsonArray);
        return PROTOCOL_FAILED;
	}
	cJSON_AddItemToObject(devDataObject,"dev",devDataJsonArray);
	for(int i=0; i<1; i++) {
		cJSON* arrayObject = cJSON_CreateObject();
		if(NULL == arrayObject) {
			cJSON_Delete(arrayObject);
			return PROTOCOL_FAILED;
		}
		cJSON_AddNumberToObject(arrayObject,"pId",_content_toSend.prdID);
		cJSON_AddStringToObject(arrayObject,"devId",_content_toSend.deviceID);
		cJSON_AddStringToObject(arrayObject,"devName", _content_toSend.deviceName);			

		cJSON_AddItemToArray(devDataJsonArray,arrayObject);
	}
	
	// debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));
	
	Frame_toSend.sn         = 1;
	Frame_toSend.version    = 0;
	Frame_toSend.netflag    = NET_TYPE_WAN;
	Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
	Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
	debug_printf("\n[DBG] Before the  Frame_packet_send \n");
	Frame_packet_send(&Frame_toSend);
	cJSON_Delete(pduJsonObject);
	return PROTOCOL_OK;
}


/*********************************************************************
 * @fn    sendDevDatatoServer
 *
 * @brief 发送子设备数据到Server
 *
 * @param mac, port
 *
 * @return
 */

 int sendDevDatatoServer(pdu_content_t *devicepdu) {

	sFrame_head_t Frame_toSend;
	pdu_content_t _content_toSend = *devicepdu;
	
	//printf(">This is sendAPinfotoServer\n");
    cJSON * pduJsonObject = NULL;
    // cJSON * devParamJsonArray = NULL;
    cJSON * devDataArrayObject= NULL;
    // cJSON * devArray = NULL;
    cJSON*  devObject = NULL;	
	
    pduJsonObject = cJSON_CreateObject();
    if(NULL == pduJsonObject)
    {
        // create object faild, exit
        cJSON_Delete(pduJsonObject);
        return PROTOCOL_FAILED;
    }
    /*add pdu type to pdu object*/
    cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_REPORT_DATA);
	
	devDataArrayObject = cJSON_CreateArray();
	if (NULL == devDataArrayObject) {
		        // create object faild, exit
        cJSON_Delete(devDataArrayObject);
        return PROTOCOL_FAILED;
	}
	cJSON_AddItemToObject(pduJsonObject,"devData",devDataArrayObject);
	
	
	for (int j=0; j<1; j++) {
		
		cJSON* devDataArrayContent = cJSON_CreateObject();
		
		/*
		** 每一次循环，都往数组里面添加新的内容 
		*/
		cJSON_AddStringToObject(devDataArrayContent,"devName", _content_toSend.deviceName);
		cJSON_AddStringToObject(devDataArrayContent,"devId", _content_toSend.deviceID);
	
		cJSON * devParamJsonArray = cJSON_CreateArray();
		if (NULL == devParamJsonArray) {
			cJSON_Delete(devParamJsonArray);
			return PROTOCOL_FAILED;
		}
		cJSON_AddItemToObject(devDataArrayContent,"param",devParamJsonArray);
		
		//devparam_t *searchList = &_content_toSend.param, *clearList;
		devparam_t *searchList = _content_toSend.param, *clearList;
		
		for(int i=0; i<_content_toSend.paramNum; i++) {
			cJSON* arrayObject = cJSON_CreateObject();
			
			if(NULL == arrayObject) {
				cJSON_Delete(arrayObject);
				return PROTOCOL_FAILED;
			}
			cJSON_AddNumberToObject(arrayObject,"type", searchList->type);
			cJSON_AddNumberToObject(arrayObject,"value",searchList->value);
			clearList = searchList;
			searchList = searchList->next;
			memset(clearList, 0, sizeof(devparam_t));
			cJSON_AddItemToArray(devParamJsonArray,arrayObject);
		}
		
		cJSON_AddItemToArray(devDataArrayObject,devDataArrayContent);
	}
	//debug_printf("\n[DBG] After create jsonArray\n");
	//debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));	
	Frame_toSend.sn         = 1;
	Frame_toSend.version    = 0;
	Frame_toSend.netflag    = NET_TYPE_WAN;
	Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
	Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
	
	//debug_printf("\n[DBG] before Frame_packet_send \n");
	Frame_packet_send(&Frame_toSend);
	//debug_printf("\n[DBG] After Frame_packet_send \n");
	cJSON_Delete(pduJsonObject);
	//debug_printf("\n[DBG] After cJSON_Delete \n");
	return PROTOCOL_OK;
}


/*********************************************************************
 * @fn    sendHeartBeattoServer
 *
 * @brief 发送心跳包数据到Server
 *
 * @param mac, port
 *
 * @return
 */

 int sendHeartBeattoServer(char * macAddr) {
	sFrame_head_t Frame_toSend;

	//printf(">This is sendAPinfotoServer\n");
    cJSON * pduJsonObject = NULL;
    pduJsonObject = cJSON_CreateObject();
    if(NULL == pduJsonObject)
    {
        // create object faild, exit
        cJSON_Delete(pduJsonObject);
        return PROTOCOL_FAILED;
    }
    /*add pdu type to pdu object*/
    cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_AP_SEND_HEARTBEAT_INFO);
	cJSON_AddStringToObject(pduJsonObject,"devData",macAddr);


	Frame_toSend.sn         = 1;
	Frame_toSend.version    = 0;
	Frame_toSend.netflag    = NET_TYPE_WAN;
	Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
	Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
	
	//debug_printf("\n[DBG] before Frame_packet_send \n");
	Frame_packet_send(&Frame_toSend);
	//debug_printf("\n[DBG] After Frame_packet_send \n");
	cJSON_Delete(pduJsonObject);
	//debug_printf("\n[DBG] After cJSON_Delete \n");
	return PROTOCOL_OK;
}


/*********************************************************************
 * @fn     DataWritetoDev
 *
 * @brief  发送设备数据至串口
 *
 * @param
 *
 * @return
 */

void DataWritetoDev(devWriteData_t *data_towrite) {
	devWriteData_t DatatoWrite = *data_towrite;
	uSOC_packet_t ZocCmd_packet;
	int err;
	uint8_t devID;
	uint32_t value = DatatoWrite.value;
	// sDevlist_info_t mydevinfo;
	/* 在此获得短地址及设备类型ID */
	
	myDataBaseMsg_t msg;
	memset(&msg,0,sizeof(myDataBaseMsg_t));
	
	msg.msgType = QUEUE_TYPE_DATABASE_SEND;
	msg.Option_type = DATABASE_OPTION_SELECT;
	sprintf(msg.devlist.devID,"%s",DatatoWrite.devId);
	debug_printf("\n[DBG]DataWritetoDev Begin to send the data to DataBaseQueueIndex");
	if ( -1 == (msgsnd(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), 0)) ) {
		debug_printf("\n[ERR]Can't send the msg \n");
	} else {
		memset(&msg,0,sizeof(myDataBaseMsg_t));
		
		err = msgrcv(DataBaseQueueIndex, &msg, sizeof(myDataBaseMsg_t), QUEUE_TYPE_DATABASE_GETINFO, MSG_NOERROR);
		
		if(-1 == err) {
			debug_printf("\n[ERR] DataWritetoDev msgrcv Something Err");
			return;
		} else {  // get the message
			debug_printf("\n[DBG] I got the msg short addr %s",msg.devlist.shortAddr);
			sscanf(msg.devlist.prdID,"%02x",&devID);
		}
	}
	ZocCmd_packet.frame.DeviceType  = 0x01;
	memset(ZocCmd_packet.frame.payload,0,sizeof(ZocCmd_packet.frame.payload));
	
	switch(devID) {
		
		case DEVICE_ID_S10: {  // S11 设备
			// static uint8_t payload;
			uint8_t s10_temp_value;
			ZocCmd_packet.frame.DeviceID    = 0x20;
			ZocCmd_packet.frame.payload_len = 0x07;
			
			ZocCmd_packet.frame.payload[1] = 0x07;
			ZocCmd_packet.frame.payload[2] = 0xC0;
			ZocCmd_packet.frame.payload[3] = 0x00;
			
			switch (DatatoWrite.type) {
				case PARAM_TYPE_DEV_SWITCH: 
					ZocCmd_packet.frame.payload[0] = 0xBE; 
					value ? (s10_temp_value=0xFF) : (s10_temp_value=0x00);
					ZocCmd_packet.frame.payload[4] = s10_temp_value;
					break;
				case PARAM_TYPE_S10_MODE:  
					ZocCmd_packet.frame.payload[0] = 0xC0; 
					ZocCmd_packet.frame.payload[4] = value+1;
					break;
				case PARAM_TYPE_S10_SPEED:   
					ZocCmd_packet.frame.payload[0] = 0xC2;
					ZocCmd_packet.frame.payload[4] = (value+1)*3;
					break;
				default: break;
			}
			ZocCmd_packet.frame.payload[5] = 0x01;
			ZocCmd_packet.frame.payload[6] = checkData(ZocCmd_packet.frame.payload,ZocCmd_packet.frame.payload_len);
		}
		break;
		
		case DEVICE_ID_S11: {  // S11 设备
			// static uint8_t payload;
			uint8_t s11_temp_value;
			ZocCmd_packet.frame.DeviceID    = 0x20;
			ZocCmd_packet.frame.payload_len = 0x07;
			
			ZocCmd_packet.frame.payload[1] = 0x07;
			ZocCmd_packet.frame.payload[2] = 0xC1;
			ZocCmd_packet.frame.payload[3] = 0x00;
			
			switch (DatatoWrite.type) {
				case PARAM_TYPE_DEV_SWITCH: 
					ZocCmd_packet.frame.payload[0] = 0xC3; 
					value ? (s11_temp_value=0xFF) : (s11_temp_value=0x00);
					ZocCmd_packet.frame.payload[4] = s11_temp_value;
					break;
				case PARAM_TYPE_S11_LIGHT:  
					ZocCmd_packet.frame.payload[0] = 0xC4; 
					ZocCmd_packet.frame.payload[4] = value*254/100;
					break;
				case PARAM_TYPE_S11_TEMP:   
					ZocCmd_packet.frame.payload[0] = 0xC5;
					ZocCmd_packet.frame.payload[4] = value*255/100;
					break;
				default: break;
			}
			ZocCmd_packet.frame.payload[5] = 0x01;
			ZocCmd_packet.frame.payload[6] = checkData(ZocCmd_packet.frame.payload,ZocCmd_packet.frame.payload_len);
		}
		break;
		case DEVICE_ID_S12: {  // S12 设备
			// static uint8_t payload;
			ZocCmd_packet.frame.DeviceID    = 0x20;
			ZocCmd_packet.frame.payload_len = 0x08;
			ZocCmd_packet.frame.payload[0] = 0xC6;
			ZocCmd_packet.frame.payload[1] = 0x08;
			ZocCmd_packet.frame.payload[2] = 0xC2;
			ZocCmd_packet.frame.payload[3] = 0x00;

			switch (DatatoWrite.type) {
				case PARAM_TYPE_S12_SWITCH:  ZocCmd_packet.frame.payload[4] = 0xFF; break;
				case PARAM_TYPE_S12_REALY_1: ZocCmd_packet.frame.payload[4] = 0x01; break;
				case PARAM_TYPE_S12_REALY_2: ZocCmd_packet.frame.payload[4] = 0x02; break;
				case PARAM_TYPE_S12_REALY_3: ZocCmd_packet.frame.payload[4] = 0x03; break;
				case PARAM_TYPE_S12_REALY_4: ZocCmd_packet.frame.payload[4] = 0x04; break;		
				default: break;
			}
			ZocCmd_packet.frame.payload[5] = value;
			ZocCmd_packet.frame.payload[6] = 0x01;
			ZocCmd_packet.frame.payload[7] = checkData(ZocCmd_packet.frame.payload,ZocCmd_packet.frame.payload_len);
		}
		break;
		case DEVICE_ID_S6: {  // 红外学习
			if (DatatoWrite.type == PARAM_TYPE_S6_KEY) {
				ZocCmd_packet.frame.payload_len = 0x05;
				switch ((value>>8)&0xFF) {
					case 0x01: break; // 新增功能
					case 0x02: 		  // 执行功能
						ZocCmd_packet.frame.payload[0] = 0xFF; 
						ZocCmd_packet.frame.payload[1] = (value&0xFF); 
						break; 
					case 0x03:        // 学习功能
						ZocCmd_packet.frame.payload[0] = 0x00; 
						ZocCmd_packet.frame.payload[1] = (value&0xFF); 	
					break;
					case 0x04: break; // 删除功能
					default:   break;
				}
			}
			
		} 
		break;
		case DEVICE_ID_RELAY: {  // S12 设备
			static uint8_t payload;
			ZocCmd_packet.frame.payload_len = 0x03;
			ZocCmd_packet.frame.DeviceID    = devID;
			switch (DatatoWrite.type) {
				case PARAM_TYPE_S12_SWITCH:  value?(payload=0x00):(payload=0xFF);    break;
				case PARAM_TYPE_S12_REALY_1: value?(payload&=~0x01):(payload|=0x01); break;
				case PARAM_TYPE_S12_REALY_2: value?(payload&=~0x02):(payload|=0x02); break;
				case PARAM_TYPE_S12_REALY_3: value?(payload&=~0x04):(payload|=0x04); break;
				case PARAM_TYPE_S12_REALY_4: value?(payload&=~0x08):(payload|=0x08); break;			
				default: break;
			}
			ZocCmd_packet.frame.payload[0] = payload;
		}
		
		default: printf("\n[ERR] There is something wrong with DevID"); return; break;
	}

	
	// memset(ZocCmd_packet.frame.shortAddr,0,sizeof(ZocCmd_packet.frame.shortAddr));
	memset(ZocCmd_packet.frame.longAddr,0,sizeof(ZocCmd_packet.frame.longAddr));
	
	for (uint8_t i=0; i<2; i++) {
		sscanf(msg.devlist.shortAddr+2*i,"%02x",ZocCmd_packet.frame.shortAddr+i);
	}	
	//memset(ZocCmd_packet.frame.longAddr,0,sizeof(ZocCmd_packet.frame.longAddr));
	zbSocSendCommand(&ZocCmd_packet);
	usleep(180000);
	printf("\n[DBG] End of sleep\n");
}

/*********************************************************************
 * @fn     checkData
 *
 * @brief  校验数据
 *
 * @param
 *
 * @return
 */

uint8_t checkData(uint8_t *data, uint16_t len) {
	uint16_t sum = 0;
	for (uint16_t i=0; i<len-1; i++) {
		sum += data[i];
	}
	printf("[DBG] The checkdata should be is %2x \n", sum&0xFF);
	return sum&0xFF;
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
	cJSON_Delete(pJsonRoot);
	
}

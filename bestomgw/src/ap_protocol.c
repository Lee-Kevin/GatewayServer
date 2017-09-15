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
#include "socket_client.h"
#include "ap_protocol.h"
#include "cJSON.h"

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

/* temp */
uint8_t ENABLEZIGBEE = 0;

/*
* Local function
*/
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
	
	printf("-----------------------------Frame_packet_recv-----------------------------\n");
	
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
			ENABLEZIGBEE = 1;
		} else {
			printf("Disable the device to join the ZigBee network\n");
			ENABLEZIGBEE = 0;
		}
		myresult = SUCCESS;

		if (PROTOCOL_OK == sendCommReplytoServer(&_frame_head,myresult)) {
		//	debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pJsonRoot));
		} else {
			
		}
		break;	
	/* 子设备控制命令 写入*/
	case TYPE_DEV_WRITE:
		debug_printf("[DBG] This is TYPE_DEV_WRITE %x\n",TYPE_DEV_WRITE);
		int number1 = cJSON_GetArraySize(pJsondevData);
		//printf("The Array Size is %d",number1);
		for(int i = 0; i < number1; i++){
			
			devWriteData_t WriteData;
			
			devDataJson = cJSON_GetArrayItem(pJsondevData,i);
			devIdJson = cJSON_GetObjectItem(devDataJson, "devId");
			
			WriteData.devId = devIdJson->valuestring;
			
			//printf("WriteData.devId: %s",WriteData.devId);
			
			//printf("devId:%s\n",devIdJson->valuestring);
			paramJson = cJSON_GetObjectItem(devDataJson, "param");
			int number2 = cJSON_GetArraySize(paramJson);
			//printf(" number2:%d\n",number2);

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
	default:
		break;
	}
	printf("-----------------------------Frame_packet_recv end-----------------------------\n");
	
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
	//printf(">This is sendAPinfotoServer\n");
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
	cJSON_AddNumberToObject(devDataObject,"port", port);
	cJSON_AddStringToObject(devDataObject,"apId", mac);
	cJSON_AddStringToObject(devDataObject,"apName",APNAME);
	
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
		cJSON_AddStringToObject(arrayObject,"devId",_content_toSend.deviceID);
		cJSON_AddStringToObject(arrayObject,"devName", _content_toSend.deviceName);			

		cJSON_AddItemToArray(devDataJsonArray,arrayObject);
	}
	
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
			// debug_printf("\n[DBG] Before free\n");
			
			//free(clearList);
			
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
 
// int sendDevDatatoServer() {

	// sFrame_head_t Frame_toSend;
	
	// static uint8_t temp = 0;
	
	// //printf(">This is sendAPinfotoServer\n");
    // cJSON * pduJsonObject = NULL;
    // // cJSON * devParamJsonArray = NULL;
    // cJSON * devDataArrayObject= NULL;
    // // cJSON * devArray = NULL;
    // cJSON*  devObject = NULL;	
	
    // pduJsonObject = cJSON_CreateObject();
    // if(NULL == pduJsonObject)
    // {
        // // create object faild, exit
        // cJSON_Delete(pduJsonObject);
        // return PROTOCOL_FAILED;
    // }
    // /*add pdu type to pdu object*/
    // cJSON_AddNumberToObject(pduJsonObject, "pduType", TYPE_REPORT_DATA);
	
	// devDataArrayObject = cJSON_CreateArray();
	// if (NULL == devDataArrayObject) {
		        // // create object faild, exit
        // cJSON_Delete(devDataArrayObject);
        // return PROTOCOL_FAILED;
	// }
	// cJSON_AddItemToObject(pduJsonObject,"devData",devDataArrayObject);
	
	// /*
	// * 用来测试
	// */
	// if(temp++ == 50) {
		
		// temp = 0;
	// }
	
	// for (int j=0; j<1; j++) {
		
		// cJSON* devDataArrayContent = cJSON_CreateObject();
		
		// /*
		// ** 每一次循环，都往数组里面添加新的内容 
		// */
		// cJSON_AddStringToObject(devDataArrayContent,"devName", "s4");
		// cJSON_AddStringToObject(devDataArrayContent,"devId", "12345678");
	
		// cJSON * devParamJsonArray = cJSON_CreateArray();
		// if (NULL == devParamJsonArray) {
			// cJSON_Delete(devParamJsonArray);
			// return PROTOCOL_FAILED;
		// }
		// cJSON_AddItemToObject(devDataArrayContent,"param",devParamJsonArray);
		// for(int i=0; i<4; i++) {
			// cJSON* arrayObject = cJSON_CreateObject();
			// if(NULL == arrayObject) {
				// cJSON_Delete(arrayObject);
				// return PROTOCOL_FAILED;
			// }
			// if(i == 0) {
				// /* 温度 */
				// cJSON_AddNumberToObject(arrayObject,"type", 0x4006);
				// cJSON_AddNumberToObject(arrayObject,"value", temp);			
			// } else if( i==1 ){
				// /* 湿度 */
				// cJSON_AddNumberToObject(arrayObject,"type", 0x4007);
				// cJSON_AddNumberToObject(arrayObject,"value", 50+temp);		
			// } else if (i==2) {
				// /* 电量百分比 */
				// cJSON_AddNumberToObject(arrayObject,"type", 0x4004);
				// cJSON_AddNumberToObject(arrayObject,"value", 30+temp);		
			// } else if (i==3) {
				// /* 在线状态 */
				// cJSON_AddNumberToObject(arrayObject,"type", 0x4014);
				// cJSON_AddNumberToObject(arrayObject,"value", 1);	
			// }
			// cJSON_AddItemToArray(devParamJsonArray,arrayObject);
		// }
		
		// cJSON_AddItemToArray(devDataArrayObject,devDataArrayContent);
	// }
	// debug_printf("[DBG] The data Print is %s\n",(char *)cJSON_PrintUnformatted(pduJsonObject));	
	// Frame_toSend.sn         = 1;
	// Frame_toSend.version    = 0;
	// Frame_toSend.netflag    = NET_TYPE_WAN;
	// Frame_toSend.cmdtype    = CMD_TYPE_UPLOAD;
	// Frame_toSend.pdu        = (char *)cJSON_PrintUnformatted(pduJsonObject);
	
	// Frame_packet_send(&Frame_toSend);
	// cJSON_Delete(pduJsonObject);
	// return PROTOCOL_OK;
// }
	
/*********************************************************************
 * @fn    sendDevDataOncetoServer
 *
 * @brief 当使能子设备入网以后，把子设备的初始值发送给M1
 *
 * @param 
 *
 * @return
 */

int sendDevDataOncetoServer() {

	sFrame_head_t Frame_toSend;
	
	static uint8_t temp = 0;
	
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
	
	/*
	* 用来测试
	*/
	if(temp++ == 50) {
		
		temp = 0;
	}
	
	for (int j=0; j<2; j++) {
		
		cJSON* devDataArrayContent = cJSON_CreateObject();
		
		/*
		** 每一次循环，都往数组里面添加新的内容 
		*/
		if (j == 1) {
			cJSON_AddStringToObject(devDataArrayContent,"devName", "s4");
			cJSON_AddStringToObject(devDataArrayContent,"devId", "12345678");
		
			cJSON * devParamJsonArray = cJSON_CreateArray();
			if (NULL == devParamJsonArray) {
				cJSON_Delete(devParamJsonArray);
				return PROTOCOL_FAILED;
			}
			cJSON_AddItemToObject(devDataArrayContent,"param",devParamJsonArray);
			for(int i=0; i<4; i++) {
				cJSON* arrayObject = cJSON_CreateObject();
				if(NULL == arrayObject) {
					cJSON_Delete(arrayObject);
					return PROTOCOL_FAILED;
				}
				if(i == 0) {
					/* 温度 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x4006);
					cJSON_AddNumberToObject(arrayObject,"value", temp);			
				} else if( i==1 ){
					/* 湿度 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x4007);
					cJSON_AddNumberToObject(arrayObject,"value", 50+temp);		
				} else if (i==2) {
					/* 电量百分比 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x4004);
					cJSON_AddNumberToObject(arrayObject,"value", 30+temp);		
				} else if (i==3) {
					/* 在线状态 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x4014);
					cJSON_AddNumberToObject(arrayObject,"value", 1);	
				}
				cJSON_AddItemToArray(devParamJsonArray,arrayObject);
			}
		} else {
			cJSON_AddStringToObject(devDataArrayContent,"devName", "s12");
			cJSON_AddStringToObject(devDataArrayContent,"devId", "12345679");
		
			cJSON * devParamJsonArray = cJSON_CreateArray();
			if (NULL == devParamJsonArray) {
				cJSON_Delete(devParamJsonArray);
				return PROTOCOL_FAILED;
			}
			cJSON_AddItemToObject(devDataArrayContent,"param",devParamJsonArray);
			for(int i=0; i<6; i++) {
				cJSON* arrayObject = cJSON_CreateObject();
				if(NULL == arrayObject) {
					cJSON_Delete(arrayObject);
					return PROTOCOL_FAILED;
				}
				if(i == 0) {
					/* 温度 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x200D);
					cJSON_AddNumberToObject(arrayObject,"value", 1);			
				} else if( i==1 ){
					/* 湿度 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x2023);
					cJSON_AddNumberToObject(arrayObject,"value", 0);		
				} else if (i==2) {
					/* 电量百分比 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x2024);
					cJSON_AddNumberToObject(arrayObject,"value", 1);		
				} else if (i==3) {
					/* 电量百分比 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x2025);
					cJSON_AddNumberToObject(arrayObject,"value", 1);		
				}else if (i==4) {
					/* 电量百分比 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x2026);
					cJSON_AddNumberToObject(arrayObject,"value", 1);		
				}else if (i==5) {
					/* 在线状态 */
					cJSON_AddNumberToObject(arrayObject,"type", 0x4014);
					cJSON_AddNumberToObject(arrayObject,"value", 1);	
				}
				cJSON_AddItemToArray(devParamJsonArray,arrayObject);
			}
		}
		
		cJSON_AddItemToArray(devDataArrayObject,devDataArrayContent);
	}
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
	printf("------------This is DataWritetoDev-------------\n");
	debug_printf("[DBG] The Device ID is %s\n",DatatoWrite.devId);
	debug_printf("[DBG] The Para Type is %x\n",DatatoWrite.type);
	debug_printf("[DBG] The value is %x\n",DatatoWrite.value);
	printf("------------This is DataWritetoDev END-------------\n");
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


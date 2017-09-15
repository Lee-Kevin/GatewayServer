/**************************************************************************************************
 Filename:       interface_protocol.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-09-13 13:23:33 -0700 
 Revision:       

 Description:    This is a file that are interfaces for the protocol

 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include "socket_client.h"
#include "ap_protocol.h"
#include "interface_protocol.h"
#include "database.h"
#include "zbSocCmd.h"

#define __BIG_DEBUG__

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif


/*
* 本地函数声明
* 
*/
static void UploadS4Data(uint8_t *data);


static uint8_t allowNetFlag = 0;
/*
* 变量声明
*/

char mac[23];
int localport;

/*********************************************************************
 * @fn     DealwithSerialData
 *
 * @brief  处理Zigbee串口过来的数据
 *
 * @param  串口数据帧
 *
 * @return
 */


void DealwithSerialData(uint8_t *data) {
	printf("\nThis is DealwithSerialData\n ");
	debug_printf("[DBG] The data recived is:\n");
	for (int j=0; j<data[2]; j++) {
		debug_printf("%02x ",data[j]);
	}
	
	/**
	* 记录设备信息并储存至数据库，并把设备名称和设备ID上传至M1
	*/
	if (allowNetFlag && (data[FRAME_CMD_DEV_ID] < 0x80)) {  // 过来的信息，是有关设备的数据
		uint8_t result = 0;
		sDevlist_info_t _devInfo;
		pdu_content_t pdu_device_info;
		char devID[21];

		for(uint8_t i=0; i<10; i++) {
			sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+i]);
		}
		devID[20] = '\0';
		switch(data[FRAME_CMD_DEV_ID]) {
			case DEVICE_ID_S4: _devInfo.devName = "s4"; break;
			case DEVICE_ID_RELAY: _devInfo.devName = "s12"; break;
			default: printf("\n[ERR] Unknow Device"); break;
		}
		_devInfo.devID 		= devID;
		_devInfo.status		= 1;
		_devInfo.power      = 0;
		_devInfo.note		= "Join the net work";
		result = InsertDatatoDatabase(&_devInfo);
		if(!result) { // 插入数据库成功
			//getClientLocalPort(&localport,&mac);
			pdu_device_info.deviceName = _devInfo.devName;
			pdu_device_info.deviceID   = _devInfo.devID;
			pdu_device_info.param      = 0;
			pdu_device_info.param	   = NULL;
			// sendDevinfotoServer(mac,localport,&pdu_device_info);
			sendDevinfotoServer("1234512345",11235,&pdu_device_info);
			
		}
	}
	
	switch(data[6]) {
	case CMD_TYPE_DEVICE_UPLOAD:
		debug_printf("\n[DBG] CMD_TYPE_DEVICE_UPLOAD\n");
		DealwithUploadData(data);
		
		break;
	case CMD_TYPE_DEVICE_EVENT:
		debug_printf("\n[DBG] CMD_TYPE_EVENT\n");
		if(data[FRAME_CMD_DEV_ID] == CMD_ID_ALLOW_NETWORK) {
			allowNetFlag = 0;
			debug_printf("\n[DBG] Don't allow zigbee Net\n");
		}

		break;
	case CMD_TYPE_DEVICE_ACK:
		debug_printf("\n[DBG] CMD_TYPE_DEVICE_ACK\n");
		if(data[FRAME_CMD_DEV_ID] == CMD_ID_ALLOW_NETWORK) {
			if (data[PAYLOAD_START] == 0xFF) {
				allowNetFlag = 1;
				debug_printf("\n[DBG] allow zigbee Net  \n");
			} else if (data[PAYLOAD_START] == 0x00){
				allowNetFlag = 0;
				debug_printf("\n[DBG] Don't allow zigbee Net\n");
			}

		}
		break;
	default:
		break;
	}
	
}

void DealwithUploadData(uint8_t *data) {
	switch(data[FRAME_CMD_DEV_ID]) { // 区分具体设备
	case DEVICE_ID_S4:
    	UploadS4Data(data);
		break;
	case DEVICE_ID_RELAY:
		break;
	default:
		break;
	}
	
}


void UploadS4Data(uint8_t *data) {
	char devID[21];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	for(uint8_t i=0; i<10; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+i]);
	}
	devID[20] = '\0';
		
	uint16_t TM_DATA = data[PAYLOAD_START]*256 + data[PAYLOAD_START+1];
	float temp = ((175.72*TM_DATA)/65536) - 46.85;
	uint16_t RH_DATA = (data[PAYLOAD_START+2]<<8) + data[PAYLOAD_START+3];
	float humidity = (125*RH_DATA)/65536 - 6;
			
	float battery = data[PAYLOAD_START+9]*1.15*3/256;
	if (battery<=1.8) battery = 1.8;
	uint8_t precent = ((battery-1.8)/1.2)*100>100 ? 100 : (battery-1.8)/1.2*100;

	
	
	_pdu_content.deviceName  = "s4";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 3;
			
		// _pdu_content.param.type       = PARAM_TYPE_S4_TEMP;
		// _pdu_content.param.value      =(uint8_t)(temp+0.5);
			
    debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];
		// _pdu_content.param = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	//		debug_printf("\n[DBG] DealwithUploadData after malloc\n");
	//memset(newdevParam, 0, _pdu_content.paramNum*sizeof(devparam_t));
		// memset(_pdu_content.param, 0, _pdu_content.paramNum*sizeof(devparam_t));
		// _pdu_content.param->type     = PARAM_TYPE_S4_HUMI;
		// _pdu_content.param->value 	 = (uint8_t)(humidity+0.5);
		// _pdu_content.param->next 	 = newparam;
		
	newdevParam[0].type      = PARAM_TYPE_S4_HUMI;
	newdevParam[0].value     = (uint8_t)(humidity+0.5);
	newdevParam[0].next	  = &newdevParam[1];
	newdevParam[1].type	  = PARAM_TYPE_S4_TEMP;
	newdevParam[1].value     = (uint8_t)(temp+0.5);;
	newdevParam[1].next      = &newdevParam[2];		
	newdevParam[2].type	  = PARAM_TYPE_POWER_STATUS;
	newdevParam[2].value     = precent;
	newdevParam[2].next      = NULL;
	debug_printf("\n[DBG] newdevParam[2]->type = %d\n",newdevParam[2].type);	
	debug_printf("\n[DBG] newdevParam[2]->value = %d\n",newdevParam[2].value);
	_pdu_content.param        =  &newdevParam[0];		
	// newdevParam[0]->type      = PARAM_TYPE_S4_HUMI;
	// newdevParam[0]->value     = (uint8_t)(humidity+0.5);
	// newdevParam[0]->next	  = newdevParam[1];
	// newdevParam[1]->type	  = PARAM_TYPE_S4_TEMP;
	// newdevParam[1]->value     = (uint8_t)(temp+0.5);;
	// newdevParam[1]->next      = newdevParam[2];		
	// newdevParam[2]->type	  = PARAM_TYPE_POWER_STATUS;
	// newdevParam[2]->value     = precent;
	// newdevParam[2]->next      = NULL;
	// debug_printf("\n[DBG] newdevParam[2]->type = %d\n",newdevParam[2]->type);		
	// _pdu_content.param        =  newdevParam[0];
	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	// sDevlist_info_t  _Devlist_info;
	// _Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	// _Devlist_info.devID  = devID;
	
}


// void UploadS4Data(uint8_t *data) {
	// char devID[21];
    // pdu_content_t _pdu_content;  
	// //pdu_content_t _pdu_content;
	// for(uint8_t i=0; i<10; i++) {
		// sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+i]);
	// }
	// devID[20] = '\0';
		
	// uint16_t TM_DATA = data[PAYLOAD_START]*256 + data[PAYLOAD_START+1];
	// float temp = ((175.72*TM_DATA)/65536) - 46.85;
	// uint16_t RH_DATA = (data[PAYLOAD_START+2]<<8) + data[PAYLOAD_START+3];
	// float humidity = (125*RH_DATA)/65536 - 6;
			
	// float battery = data[PAYLOAD_START+9]*1.15*3/256;
	// if (battery<=1.8) battery = 1.8;
	// uint8_t precent = (battery-1.8)/1.2>100 ? 100 : (battery-1.8)/1.2>100;

			
	// _pdu_content.deviceName  = "s4";
	// _pdu_content.deviceID    = devID;
	// _pdu_content.paramNum    = 3;
			
		// // _pdu_content.param.type       = PARAM_TYPE_S4_TEMP;
		// // _pdu_content.param.value      =(uint8_t)(temp+0.5);
			
    // debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
		// // devparam_t *newparam;
		// // _pdu_content.param = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	// //		debug_printf("\n[DBG] DealwithUploadData after malloc\n");
	// memset(newdevParam, 0, _pdu_content.paramNum*sizeof(devparam_t));
		// // memset(_pdu_content.param, 0, _pdu_content.paramNum*sizeof(devparam_t));
		// // _pdu_content.param->type     = PARAM_TYPE_S4_HUMI;
		// // _pdu_content.param->value 	 = (uint8_t)(humidity+0.5);
		// // _pdu_content.param->next 	 = newparam;
		
	// newdevParam[0]->type      = PARAM_TYPE_S4_HUMI;
	// newdevParam[0]->value     = (uint8_t)(humidity+0.5);
	// newdevParam[0]->next	  = newdevParam[1];
	// newdevParam[1]->type	  = PARAM_TYPE_S4_TEMP;
	// newdevParam[1]->value     = (uint8_t)(temp+0.5);;
	// newdevParam[1]->next      = newdevParam[2];		
	// newdevParam[2]->type	  = PARAM_TYPE_POWER_STATUS;
	// newdevParam[2]->value     = precent;
	// newdevParam[2]->next      = NULL;
	// debug_printf("\n[DBG] newdevParam[2]->type = %d\n",newdevParam[2]->type);		
	// _pdu_content.param        =  newdevParam[0];
	// sendDevDatatoServer(&_pdu_content);
	// debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	// // sDevlist_info_t  _Devlist_info;
	// // _Devlist_info.status = 1;
	// // _Devlist_info.note   = "update online";
	// // _Devlist_info.devID  = devID;
	
// }



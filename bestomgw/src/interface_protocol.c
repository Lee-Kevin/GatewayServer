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
static void UploadS5Data(uint8_t *data);



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
		char devID[23];
		sprintf(devID,"%02x",data[FRAME_CMD_DEV_ID]);
		for(uint8_t i=0; i<10; i++) {
			sprintf(devID+2+2*i,"%02x",data[SHORT_ADDR_START+i]);
		}
		devID[22] = '\0';
		switch(data[FRAME_CMD_DEV_ID]) {
			case DEVICE_ID_S1:     _devInfo.devName = "s1";  break;
			case DEVICE_ID_S4:     _devInfo.devName = "s4";  break;
			case DEVICE_ID_RELAY:  _devInfo.devName = "s12"; break;
			case DEVICE_ID_S3:     _devInfo.devName = "s3";  break;
			case DEVICE_ID_S5:     _devInfo.devName = "s5";  break;
			case DEVICE_ID_S6:     _devInfo.devName = "s6";  break;
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
			pdu_device_info.paramNum   = 0;
			pdu_device_info.param	   = NULL;
			sendDevinfotoServer(mac,localport,&pdu_device_info);
			// sendDevinfotoServer("1234512345",11235,&pdu_device_info);
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
		case DEVICE_ID_S1: UploadS1Data(data);  break;			// 人体红外  
		case DEVICE_ID_S3: UploadS3Data(data);  break;          // 有源面板
		case DEVICE_ID_S4: UploadS4Data(data);  break;			// 温湿度
		case DEVICE_ID_S5: UploadS5Data(data);	break;		    // 光照度
		case DEVICE_ID_RELAY:				break;
		default:	break;
	}
	
}


/*************************************************************************************************
 * @fn      UploadS1Data()
 *
 * @brief   上传人体红外信息至M1
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS1Data(uint8_t *data) {
	char devID[23];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	sprintf(devID,"%02x",data[FRAME_CMD_DEV_ID]);
	for(uint8_t i=0; i<10; i++) {
		sprintf(devID+2+2*i,"%02x",data[SHORT_ADDR_START+i]);
	}
	devID[22] = '\0';
	
	uint8_t PEOPLE_DATA = data[PAYLOAD_START];
	
	float battery = data[PAYLOAD_START+9]*1.15*3/256;
	if (battery<=1.8) battery = 1.8;
	uint8_t precent = ((battery-1.8)/1.2)*100>100 ? 100 : (battery-1.8)/1.2*100;

	
	
	_pdu_content.deviceName  = "s1";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 2;
			
		// _pdu_content.param.type       = PARAM_TYPE_S4_TEMP;
		// _pdu_content.param.value      =(uint8_t)(temp+0.5);
			
    debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];
	newdevParam[0].type = PARAM_TYPE_S1_PEOPLE;
	if(PEOPLE_DATA == 0xFF ) {
		newdevParam[0].value = 1;
	} else {
		newdevParam[0].value = 0;
	}
	
	newdevParam[0].next	 	 = &newdevParam[1];
	newdevParam[1].type      = PARAM_TYPE_POWER_STATUS;
	newdevParam[1].value     = precent;
	newdevParam[1].next	 	 = NULL;
	_pdu_content.param        =  &newdevParam[0];		
	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	// sDevlist_info_t  _Devlist_info;
	// _Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	// _Devlist_info.devID  = devID;
	
}

/*************************************************************************************************
 * @fn      UploadS3Data()
 *
 * @brief   上传按键信息至M1
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS3Data(uint8_t *data) {
	char devID[23];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	sprintf(devID,"%02x",data[FRAME_CMD_DEV_ID]);
	for(uint8_t i=0; i<10; i++) {
		sprintf(devID+2+2*i,"%02x",data[SHORT_ADDR_START+i]);
	}
	devID[22] = '\0';
	
	uint8_t KEY_DATA = data[PAYLOAD_START];
	float battery = data[PAYLOAD_START+9]*1.15*3/256;
	if (battery<=1.8) battery = 1.8;
	uint8_t precent = ((battery-1.8)/1.2)*100>100 ? 100 : (battery-1.8)/1.2*100;

	
	
	_pdu_content.deviceName  = "s3";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 2;
			
		// _pdu_content.param.type       = PARAM_TYPE_S4_TEMP;
		// _pdu_content.param.value      =(uint8_t)(temp+0.5);
			
    debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];

	switch (KEY_DATA) {
		case 0x01: 	newdevParam[0].type = PARAM_TYPE_S3_KEY1; newdevParam[0].value = 1; break;
		case 0x02: 	newdevParam[0].type = PARAM_TYPE_S3_KEY2; newdevParam[0].value = 1; break;
		case 0x04: 	newdevParam[0].type = PARAM_TYPE_S3_KEY3; newdevParam[0].value = 1; break;
		case 0x08: 	newdevParam[0].type = PARAM_TYPE_S3_KEY4; newdevParam[0].value = 1; break;
		default:    printf("[ERR] Something err on S3 happened"); 						break;
	}
	newdevParam[0].next	 	 = &newdevParam[1];
	newdevParam[1].type      = PARAM_TYPE_POWER_STATUS;
	newdevParam[1].value     = precent;
	newdevParam[1].next	 	 = NULL;
	_pdu_content.param        =  &newdevParam[0];		
	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	// sDevlist_info_t  _Devlist_info;
	// _Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	// _Devlist_info.devID  = devID;
	
}

/*************************************************************************************************
 * @fn      UploadS5Data()
 *
 * @brief   上传光照传感器信息至M1
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS5Data(uint8_t *data) {
	char devID[23];
	float param = 0.319; // 太阳光 0.282 荧光灯 0.146
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	sprintf(devID,"%02x",data[FRAME_CMD_DEV_ID]);
	for(uint8_t i=0; i<10; i++) {
		sprintf(devID+2+2*i,"%02x",data[SHORT_ADDR_START+i]);
	}
	devID[22] = '\0';
	
	uint16_t LIGHT_DATA = data[PAYLOAD_START]*256 + data[PAYLOAD_START+1];
	float lux = LIGHT_DATA/param;
			
	float battery = data[PAYLOAD_START+9]*1.15*3/256;
	if (battery<=1.8) battery = 1.8;
	uint8_t precent = ((battery-1.8)/1.2)*100>100 ? 100 : (battery-1.8)/1.2*100;

	
	
	_pdu_content.deviceName  = "s5";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 2;
			
		// _pdu_content.param.type       = PARAM_TYPE_S4_TEMP;
		// _pdu_content.param.value      =(uint8_t)(temp+0.5);
			
    debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];

	newdevParam[0].type      = PARAM_TYPE_S5_LIGHT;
	newdevParam[0].value     = (uint32_t)(lux+0.5);
	newdevParam[0].next	 	 = &newdevParam[1];
	newdevParam[1].type      = PARAM_TYPE_POWER_STATUS;
	newdevParam[1].value     = precent;
	newdevParam[1].next	 	 = NULL;
	_pdu_content.param        =  &newdevParam[0];		

	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	// sDevlist_info_t  _Devlist_info;
	// _Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	// _Devlist_info.devID  = devID;
	
}



/*************************************************************************************************
 * @fn      UploadS4Data()
 *
 * @brief   上传S4温湿度传感器信息至M1
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS4Data(uint8_t *data) {
	char devID[23];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	sprintf(devID,"%02x",data[FRAME_CMD_DEV_ID]);
	for(uint8_t i=0; i<10; i++) {
		sprintf(devID+2+2*i,"%02x",data[SHORT_ADDR_START+i]);
	}
	devID[22] = '\0';
		
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

	// 温度扩大十倍
	newdevParam[0].type      = PARAM_TYPE_S4_HUMI;
	newdevParam[0].value     = (uint8_t)(humidity+0.5);
	newdevParam[0].next	  = &newdevParam[1];
	newdevParam[1].type	  = PARAM_TYPE_S4_TEMP;
	newdevParam[1].value     = (uint16_t)(temp*10+0.5);
	newdevParam[1].next      = &newdevParam[2];		
	newdevParam[2].type	  = PARAM_TYPE_POWER_STATUS;
	newdevParam[2].value     = precent;
	newdevParam[2].next      = NULL;
	debug_printf("\n[DBG] newdevParam[2]->type = %d\n",newdevParam[2].type);	
	debug_printf("\n[DBG] newdevParam[2]->value = %d\n",newdevParam[2].value);
	_pdu_content.param        =  &newdevParam[0];		

	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	// sDevlist_info_t  _Devlist_info;
	// _Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	// _Devlist_info.devID  = devID;
	
}


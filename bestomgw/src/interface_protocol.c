/**************************************************************************************************
 Filename:       interface_protocol.c
 Author:         Jiankai Li
 Revised:        $Date: 2017-09-13 13:23:33 -0700 
 Revision:       

 Description:    This is a file that deal with the data from UART

 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
//#include <sqlite3.h>
#include "sqlite3.h"
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

static void UploadS1Data(uint8_t *data); 
static void UploadS2Data(uint8_t *data);
static void UploadS3Data(uint8_t *data);
static void UploadS4Data(uint8_t *data);
static void UploadS5Data(uint8_t *data);
static void UploadS8Data(uint8_t *data);

static void test(uint8_t flag);

static uint8_t allowNetFlag = 0;
/*
* 变量声明
*/

extern char mac[23];
extern int localport;

// char mac[23];
// int localport;

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
	uint8_t OFFSET = 0;
	printf("\nThis is DealwithSerialData\n ");
	debug_printf("[DBG] The data recived is:\n");
	for (int j=0; j<data[2]; j++) {
		debug_printf("%02x ",data[j]);
	}
	OFFSET = data[2] - 0x20;  // 当遇到长度可变字节时，负载后面的字段，需要增加偏移量
	/**
	* 记录设备信息并储存至数据库，并把设备名称和设备ID上传至M1 允许入网后执行的操作
	*/
	if (allowNetFlag && (data[FRAME_CMD_DEV_ID] < 0x80) ) {  // 过来的信息，是有关设备的数据
		uint8_t result = 0;
		uint16_t tempprdID = 0x0000;
		sDevlist_info_t _devInfo;
		pdu_content_t pdu_device_info;
		char prdID[3];
		char shortAddr[5];
		char devID[17];
		if(data[FRAME_CMD_DEV_ID] == DEVICE_ID_UART) {
			sprintf(prdID,"%02x",data[FRAME_CMD_DEV_ID]);  
		} else {
			sprintf(prdID,"%02x",data[FRAME_CMD_DEV_ID]); 
		}
		prdID[2] = '\0';

		for(uint8_t i=0; i<2; i++) {
			sprintf(shortAddr+2*i,"%02x",data[OFFSET+SHORT_ADDR_START+i]);
		}
		shortAddr[4] = '\0';

		switch(data[FRAME_CMD_DEV_ID]) {
			case DEVICE_ID_S1:     sprintf(_devInfo.devName,"%s","s1");  break;
			case DEVICE_ID_S2:     sprintf(_devInfo.devName,"%s","s2");  break;
			case DEVICE_ID_S4:     sprintf(_devInfo.devName,"%s","s4");  break;
			case DEVICE_ID_RELAY:  sprintf(_devInfo.devName,"%s","s12"); break;
			case DEVICE_ID_S3:     sprintf(_devInfo.devName,"%s","s3");  break;
			case DEVICE_ID_S5:     sprintf(_devInfo.devName,"%s","s5");  break;
			case DEVICE_ID_S6:     sprintf(_devInfo.devName,"%s","s6");  break;
			case DEVICE_ID_S8:     sprintf(_devInfo.devName,"%s","s8");  break;
			// case DEVICE_ID_UART:   sprintf(_devInfo.devName,"%s","sx");  break;
			case DEVICE_ID_UART:   sprintf(_devInfo.devName,"%s","sx");  break;
			default: printf("\n[ERR] Unknow Device"); break;
		}
		if(data[FRAME_CMD_DEV_ID] != DEVICE_ID_S2) {
			for(uint8_t i=0; i<8; i++) {
				sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i+OFFSET]);
			}
			devID[16] = '\0';
		} else {
			for(uint8_t i=0; i<8; i++) {   // S2的帧结构有所不同 负载只有4字节
				sprintf(devID+2*i,"%02x",data[PAYLOAD_START+5+i]);
			}
			devID[16] = '\0';
		}

		
		
		sprintf(_devInfo.prdID,"%s",prdID);
		sprintf(_devInfo.shortAddr,"%s",shortAddr);
		sprintf(_devInfo.devID,"%s",devID);
		
		// _devInfo.prdID      = prdID;
		// _devInfo.shortAddr  = shortAddr;
		// _devInfo.devID 		= devID;
		_devInfo.status		= 1;
		_devInfo.power      = 0;
		sprintf(_devInfo.note,"%s","Join the net work");
		// _devInfo.note		= "Join the net work";
		debug_printf("\n[DBG] Begin the InsertDatatoDatabase \n");
		result = InsertDatatoDatabase(&_devInfo); // result 为true时，代表已经有数据
		
		if(!result) { // 插入数据库成功
			debug_printf("\n[DBG] Begin to getClientLocalPort \n");
			getClientLocalPort(&localport,&mac);
			
			pdu_device_info.deviceName = _devInfo.devName;
			pdu_device_info.deviceID   = _devInfo.devID;
			switch(data[DEVICE_TYPE_ID]) {
				case DEVICE_TYPE_DC_POWER:  tempprdID |= 0x8000; break;
				case DEVICE_TYPE_LOW_POWER: tempprdID |= 0x4000; break;
				case DEVICE_TYPE_NO_POWER:  tempprdID |= 0x4000; break;
				default: break;
			}
			if (data[FRAME_CMD_DEV_ID]<0x17) {
				tempprdID |= 0x1000;
			} else {
				tempprdID |= 0x2000;
			}
			pdu_device_info.prdID   = tempprdID | data[FRAME_CMD_DEV_ID];
			pdu_device_info.paramNum   = 0;
			pdu_device_info.param	   = NULL;
			debug_printf("\n[DBG] Before sendDevinfotoServer \n");
			sendDevinfotoServer(mac,localport,&pdu_device_info);
		}
	}
	
	switch(data[FRAME_CMD_TYPE_ID]) {
	case CMD_TYPE_DEVICE_UPLOAD:
		debug_printf("\n[DBG] CMD_TYPE_DEVICE_UPLOAD\n");
		DealwithUploadData(data);
		break;
	case CMD_TYPE_DEVICE_EVENT:
		debug_printf("\n[DBG] CMD_TYPE_EVENT\n");
		if(data[FRAME_CMD_DEV_ID] == CMD_ID_ALLOW_NETWORK) {
			allowNetFlag = 0;
			debug_printf("\n[DBG] Don't allow zigbee Net\n");
		} else {      // 处理心跳包数据
			char devID[17];
			for(uint8_t i=0; i<8; i++) {
				sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
			}
			devID[16] = '\0';
			sDevlist_info_t  _Devlist_info;
			_Devlist_info.status = 1;
			// _Devlist_info.note   = "update online";
			sprintf(_Devlist_info.note,"%s","update online");
			sprintf(_Devlist_info.devID,"%s",devID);
			UpdateDevStatustoDatabase(&_Devlist_info);
		}
		break;
	case CMD_TYPE_DEVICE_ACK:
		debug_printf("\n[DBG] CMD_TYPE_DEVICE_ACK\n");
		DealwithAckData(data);
		break;
	case CMD_TYPE_DEVICE_TEST:
		debug_printf("\n[DBG] This is device test");
	default:
		break;
	}
}


void DealwithAckData(uint8_t *data) {
	switch(data[FRAME_CMD_DEV_ID]) {
		case CMD_ID_ALLOW_NETWORK:  
			if (data[PAYLOAD_START] == 0xFF) {
				allowNetFlag = 1;
				debug_printf("\n[DBG] allow zigbee Net  \n");
			} else if (data[PAYLOAD_START] == 0x00){
				allowNetFlag = 0;
				debug_printf("\n[DBG] Don't allow zigbee Net\n");
			}
			break;
		case DEVICE_ID_RELAY:
			UploadS12Data(data);
			break;
		default:
			debug_printf("\n[ERR] Something Err with ack data\n");
			break;
	}
}


void DealwithUploadData(uint8_t *data) {
	switch(data[FRAME_CMD_DEV_ID]) { // 区分具体设备
		case DEVICE_ID_S1: UploadS1Data(data);  break;			// 人体红外  
		case DEVICE_ID_S2: UploadS2Data(data);  break;			// 门磁
		case DEVICE_ID_S3: UploadS3Data(data);  break;          // 有源面板
		case DEVICE_ID_S4: UploadS4Data(data);  break;			// 温湿度
		case DEVICE_ID_S5: UploadS5Data(data);	break;		    // 光照度
		case DEVICE_ID_S8: UploadS8Data(data);	break;		    // 光照度
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
	char devID[17];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	for(uint8_t i=0; i<8; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
	}
	devID[16] = '\0';
	
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
	
	//test(PEOPLE_DATA);
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
	
}

void test(uint8_t flag) {
	uSOC_packet_t ZocCmd_packet;
	uint8_t devID;
	ZocCmd_packet.frame.DeviceID    = 0x17;
	ZocCmd_packet.frame.DeviceType  = 0x01;
	memset(ZocCmd_packet.frame.payload,0,sizeof(ZocCmd_packet.frame.payload));
	if(flag == 0xFF) {
		ZocCmd_packet.frame.payload[0] = 0xfe;
	} else {
		ZocCmd_packet.frame.payload[0] = 0xff;
	}

	// memset(ZocCmd_packet.frame.shortAddr,0,sizeof(ZocCmd_packet.frame.shortAddr));
	memset(ZocCmd_packet.frame.longAddr,0,sizeof(ZocCmd_packet.frame.longAddr));
	ZocCmd_packet.frame.shortAddr[0] = 0xfb;
	ZocCmd_packet.frame.shortAddr[1] = 0xf2;
	zbSocSendCommand(&ZocCmd_packet);	
}

/*************************************************************************************************
 * @fn      UploadS2Data()
 *
 * @brief   上传门磁信息至M1
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS2Data(uint8_t *data) {
	char devID[17];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;

	for(uint8_t i=0; i<8; i++) {   // S2的帧结构有所不同 负载只有4字节
		sprintf(devID+2*i,"%02x",data[PAYLOAD_START+5+i]);
	}
	
	devID[16] = '\0';
	
	uint8_t DOOR_STATUS = data[PAYLOAD_START];
	
	float battery = data[PAYLOAD_START+3]*1.15*3/256;
	if (battery<=1.8) battery = 1.8;
	uint8_t precent = ((battery-1.8)/1.2)*100>100 ? 100 : (battery-1.8)/1.2*100;

	
	
	_pdu_content.deviceName  = "s2";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 2;
			
    debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];
	newdevParam[0].type = PARAM_TYPE_S2_DOORSTATUS;
	if(DOOR_STATUS == 0x01 ) {
		newdevParam[0].value = 1;  // 门窗开启
	} else {
		newdevParam[0].value = 0;  // 门窗关闭
	}
	
	newdevParam[0].next	 	 = &newdevParam[1];
	newdevParam[1].type      = PARAM_TYPE_POWER_STATUS;
	newdevParam[1].value     = precent;
	newdevParam[1].next	 	 = NULL;
	_pdu_content.param        =  &newdevParam[0];		
	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
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
	char devID[17];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	for(uint8_t i=0; i<8; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
	}
	devID[16] = '\0';
	
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
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
	
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
	char devID[17];
	float param = 0.319; // 太阳光 0.282 荧光灯 0.146
    pdu_content_t _pdu_content;  
	for(uint8_t i=0; i<8; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
	}
	devID[16] = '\0';
	
	uint16_t LIGHT_DATA = data[PAYLOAD_START+1]*256 + data[PAYLOAD_START];
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
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
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
	char devID[17];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	for(uint8_t i=0; i<8; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
	}
	devID[16] = '\0';
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
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
	
}

/*************************************************************************************************
 * @fn      UploadS8Data()
 *
 * @brief   上传S8空气质量监测模块信息至M1
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS8Data(uint8_t *data) {
	char devID[17];
    pdu_content_t _pdu_content;  
	//pdu_content_t _pdu_content;
	for(uint8_t i=0; i<8; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
	}
	devID[16] = '\0';
	
	uint16_t TM_DATA = data[PAYLOAD_START]*256 + data[PAYLOAD_START+1];
	float temp = ((175.72*TM_DATA)/65536) - 46.85;
	uint16_t RH_DATA = (data[PAYLOAD_START+2]<<8) + data[PAYLOAD_START+3];
	float humidity = (125*RH_DATA)/65536 - 6;
	uint16_t PM2_5 = (data[PAYLOAD_START+4]<<8) + data[PAYLOAD_START+5];
	uint16_t CO_2  = (data[PAYLOAD_START+6]<<8) + data[PAYLOAD_START+7];
	uint16_t CH_4  = (data[PAYLOAD_START+8]<<8) + data[PAYLOAD_START+9];
	
	
	_pdu_content.deviceName  = "s8";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 5;
			
    debug_printf("\n[DBG] the device ID is %s\n",devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];

	// 温度扩大十倍
	newdevParam[0].type      = PARAM_TYPE_S4_TEMP;
	newdevParam[0].value     = (uint16_t)(temp*10+0.5);
	newdevParam[0].next	  	 = &newdevParam[1];
	newdevParam[1].type	  	 = PARAM_TYPE_S4_HUMI;
	newdevParam[1].value     = (uint8_t)(humidity+0.5);
	newdevParam[1].next      = &newdevParam[2];		
	newdevParam[2].type	  	 = PARAM_TYPE_S8_PM25;
	newdevParam[2].value     = PM2_5;
	newdevParam[2].next      = &newdevParam[3];
	newdevParam[3].type      = PARAM_TYPE_S8_CO2;
	newdevParam[3].value     = CO_2;
	newdevParam[3].next	  	 = &newdevParam[4];
	newdevParam[4].type      = PARAM_TYPE_S8_CH4;
	newdevParam[4].value     = CH_4;
	newdevParam[4].next	  	 = NULL;

	// debug_printf("\n[DBG] newdevParam[2]->type = %d\n",newdevParam[2].type);	
	// debug_printf("\n[DBG] newdevParam[2]->value = %d\n",newdevParam[2].value);
	
	_pdu_content.param        =  &newdevParam[0];		

	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
	
}

/*************************************************************************************************
 * @fn      UploadS12Data()
 *
 * @brief   上传回路控制器状态
 *
 * @param   串口传来的数据帧
 *
 * @return  无
 *************************************************************************************************/
void UploadS12Data(uint8_t *data) {
	char devID[17];
    pdu_content_t _pdu_content;  
	for(uint8_t i=0; i<8; i++) {
		sprintf(devID+2*i,"%02x",data[SHORT_ADDR_START+2+i]);
	}
	devID[16] = '\0';
	
	uint8_t STATUS_DATA = data[PAYLOAD_START];
	
	_pdu_content.deviceName  = "s12";
	_pdu_content.deviceID    = devID;
	_pdu_content.paramNum    = 5;
			
    debug_printf("\n[DBG] the %s device ID is %s\n",_pdu_content.deviceName,devID);
	// devparam_t (*newdevParam)[_pdu_content.paramNum] = (devparam_t *) malloc(_pdu_content.paramNum*sizeof(devparam_t));
	devparam_t newdevParam[_pdu_content.paramNum];
	newdevParam[0].type = PARAM_TYPE_S12_SWITCH;
	if((STATUS_DATA&0x0F) == 0x0F ) {  
		newdevParam[0].value = 0;
	} else {
		newdevParam[0].value = 1;
	}
	newdevParam[0].next	 	 = &newdevParam[1];
	newdevParam[1].type      = PARAM_TYPE_S12_REALY_1;
	newdevParam[1].value     = (~STATUS_DATA)&0x01;
	// newdevParam[1].value     = ~(STATUS_DATA&0xFE);
	newdevParam[1].next	 	 = &newdevParam[2];
	newdevParam[2].type      = PARAM_TYPE_S12_REALY_2;
	newdevParam[2].value     = (~(STATUS_DATA>>1))&0x01;
	newdevParam[2].next	 	 = &newdevParam[3];	
	newdevParam[3].type      = PARAM_TYPE_S12_REALY_3;
	newdevParam[3].value     = (~(STATUS_DATA>>2))&0x01;
	newdevParam[3].next	 	 = &newdevParam[4];
	newdevParam[4].type      = PARAM_TYPE_S12_REALY_4;
	newdevParam[4].value     = (~(STATUS_DATA>>3))&0x01;
	newdevParam[4].next	 	 = NULL;	
	_pdu_content.param        =  &newdevParam[0];		
	sendDevDatatoServer(&_pdu_content);
	debug_printf("\n[DBG] After sendDevDatatoServer\n");
	
	sDevlist_info_t  _Devlist_info;
	_Devlist_info.status = 1;
	// _Devlist_info.note   = "update online";
	sprintf(_Devlist_info.note,"%s","update online");
	sprintf(_Devlist_info.devID,"%s",devID);
	UpdateDevStatustoDatabase(&_Devlist_info);
	
}


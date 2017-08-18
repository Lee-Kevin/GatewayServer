#ifndef _UTILS_H_
#define _UTILS_H_

#include     <stdio.h>  
#include     <stdlib.h>   
#include     <unistd.h>    
#include     <sys/types.h>  
#include     <sys/stat.h>  
#include     <fcntl.h>   
#include     <termios.h>  
#include     <errno.h>  

#include <time.h>


#include     <limits.h>  //PATH_MAX
//sqlite3
#include "sqlite3.h"


#define GW_DEBUG 1

#define TRUE 1
#define FALSE 0
#define OK 1
#define ERROR 0

#define MAX_CFG_BUF                              512 
#define SuccessRet 1;
#define FailedRet  0;
#define MAX_CFG_BUF                              512
#define CFG_OK                                   0
#define CFG_SECTION_NOT_FOUND                    -1
#define CFG_KEY_NOT_FOUND                        -2
#define CFG_ERR                                  -10
#define CFG_ERR_FILE                             -10
#define CFG_ERR_OPEN_FILE                        -10
#define CFG_ERR_CREATE_FILE                      -11
#define CFG_ERR_READ_FILE                        -12
#define CFG_ERR_WRITE_FILE                       -13
#define CFG_ERR_FILE_FORMAT                      -14
#define CFG_ERR_SYSTEM                           -20
#define CFG_ERR_SYSTEM_CALL                      -20
#define CFG_ERR_INTERNAL                         -21
#define CFG_ERR_EXCEED_BUF_SIZE                  -22 

#define  ZHA_PROFILE_ID         0x0104
#define  ZLL_PROFILE_ID         0xc05e
#define  ZHA_IAS_ZONE_ID        0x0402
#define  ZHA_DEVICE_NUM         42
#define  ZLL_DEVICE_NUM         13
#define  ZHA_IAS_ZONE_NUM       13
#define  MAX_DEVICES_NUM        32

typedef unsigned char      uint8;
typedef unsigned short     uint16;

 //long MAXLEN = 10*1024;//10KB
 char sexepath[PATH_MAX];
 char slogpath[PATH_MAX]; //log 文件路径
 char sdbpath[PATH_MAX]; //db 文件路径
 char sinipath[PATH_MAX]; //db 文件路径 
 

extern sqlite3 *gdb;

int fdserwrite, fdread; //串口 写,读






//16 进制字符转 数字
int htoi(char s[]); 

void prt_debug(char *declare, unsigned char *data, unsigned int dlen);

void hex2dec(const char* hex_str, char* dec_str);
/*将str1字符串中第一次出现的str2字符串替换成str3*/  
void replaceFirst(char *str1,char *str2,char *str3);

/*将str1出现的所有的str2都替换为str3*/  
void replace(char *str1,char *str2,char *str3);


void substring(char *dest,char *src,int start,int end);


char* substr( const char *src, unsigned int start, unsigned int end);


// get the file size.  
long getfilesize(FILE *pFile);  
 
//void WriteSendDB(sqlite3 *gdb, unsigned char *str, int nlen, int cmdkind, int cmdsub);
void WriteSendDB(sqlite3 *gdb, unsigned char *str,char uuid[37], int nlen, int cmdkind, int cmdsub);

void WriteRecvDB(sqlite3 *gdb, unsigned char *str, int nlen, int cmdkind, int cmdsub);
 
void WriteZigbeeLog(char *filname, unsigned char *str, int nlen);


void WriteLog(char *filname, char *str);


int ExecSql(sqlite3 *gdb, char strsql[500]);

int getSqlResult(sqlite3 *db, char strsql[500], char **azResult, int nrow, int ncolumn);

//字节流转换为十六进制字符串的另一种实现方式  
void Hex2Str( const char *sSrc,  char *sDest, int nSrcLen );

//十六进制字符串转换为字节流  
void HexStrToByte(const char* source, unsigned char* dest, int sourceLen);


 //从INI文件读取字符串类型数据
char *GetIniKeyString(char *title,char *key,char *filename);
 //从INI文件读取INT类型数据
int GetIniKeyInt(char *title,char *key,char *filename);

char *CharArrayToHexString(char* pOut, const int nMaxLen, const char* pInput, const int nInLen);

//CRC8 校验
 typedef unsigned char uint8;
 //typedef unsigned long uint32;
 typedef unsigned int   uint32;
//uint8 xCal_crc(uint8 *ptr,uint32 len);
uint8 crc_generate(uint8 *buff, uint8 len);
unsigned char Crc8Gen_smbus(unsigned char *charP,unsigned char len);

uint8 xCal_crc(uint8 *ptr,uint32 len,uint8 genPoly);

uint8 Calcxor( uint8 *msg_ptr, uint8 len );

//char *random_uuid( char buf[37] );

size_t GetCurrentExcutableFilePathName( char* processdir,char* processname, size_t len);

int  ConfigGetKey(void *CFG_file, void *section, void *key, void *buf);
//新设备入网
 int push_new_device(char *saddr, char *spoint);

//static 
	void gw_debug(char *declare, unsigned char *data, unsigned int dlen);
#endif //_UTILS_H_

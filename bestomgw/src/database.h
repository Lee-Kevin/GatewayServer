#ifndef _DATABASE_H
#define _DATABASE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "sqlite3.h"

#define DATABASE_OPTION_INSERT    1
#define DATABASE_OPTION_UPDATE    2
#define DATABASE_OPTION_SELECT    3
#define DATABASE_OPTION_DELETE    4 
#define DATABASE_OPTION_NULL      5   


struct sDevlist_info {
	char     devName[10];
	char     prdID[3];
	char     shortAddr[5];
	char     devID[17];
    uint8_t  status;
	uint8_t  power;
	char  	 note[25];
	char     TimeText[20];
    void     *next;
};
typedef struct sDevlist_info sDevlist_info_t;

void APDatabaseInit(char * database);

uint8_t InsertDatatoDatabase(sDevlist_info_t * devInfo);
uint8_t UpdateDatatoDatabase(sDevlist_info_t * devInfo);
uint8_t UpdateDevStatustoDatabase(sDevlist_info_t * devInfo);
uint8_t GetDevInfofromDatabase(char *DevId, sDevlist_info_t *devinfo);
void    Start_Transaction(sqlite3* db);
void  	End_Transaction(sqlite3* db);



#ifdef __cplusplus
}
#endif

#endif /* _DATABASE_H */
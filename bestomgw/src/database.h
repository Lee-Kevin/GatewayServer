#ifndef _DATABASE_H
#define _DATABASE_H

#ifdef __cplusplus
extern "C"
{
#endif

/* The Frame struct */
// struct sDevlist_info {
	// char     *devName;
	// char     *prdID;
	// char     *shortAddr;
	// char     *devID;
    // uint8_t  status;
	// uint8_t  power;
	// char  	 *note;
    // void     *next;
// };
// typedef struct sDevlist_info sDevlist_info_t;

struct sDevlist_info {
	char     devName[10];
	char     prdID[3];
	char     shortAddr[5];
	char     devID[17];
    uint8_t  status;
	uint8_t  power;
	char  	 note[25];
    void     *next;
};
typedef struct sDevlist_info sDevlist_info_t;

void APDatabaseInit(char * database);

uint8_t InsertDatatoDatabase(sDevlist_info_t * devInfo);
uint8_t UpdateDatatoDatabase(sDevlist_info_t * devInfo);
uint8_t UpdateDevStatustoDatabase(sDevlist_info_t * devInfo);
uint8_t GetDevInfofromDatabase(char *DevId, sDevlist_info_t *devinfo);



#ifdef __cplusplus
}
#endif

#endif /* _DATABASE_H */
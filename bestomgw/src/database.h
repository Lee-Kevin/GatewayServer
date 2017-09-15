#ifndef _DATABASE_H
#define _DATABASE_H

#ifdef __cplusplus
extern "C"
{
#endif

/* The Frame struct */
struct sDevlist_info {
    char     *devID;
	char     *devName;
    uint8_t  status;
	uint8_t  power;
	char  	 *note;
    void     *next;
};
typedef struct sDevlist_info sDevlist_info_t;

void APDatabaseInit(char * database);

uint8_t InsertDatatoDatabase(sDevlist_info_t * devInfo);
uint8_t UpdateDatatoDatabase(sDevlist_info_t * devInfo);
uint8_t UpdateDevStatustoDatabase(sDevlist_info_t * devInfo);

#ifdef __cplusplus
}
#endif

#endif /* _DATABASE_H */
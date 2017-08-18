#############################################################
# nfc daemon Makefile 
#############################################################
EXEC = bestomgwd
CFLAGS += -O2 -Wall -Ibestomgw/inc

##OBJS = bestomgw/src/utils.o bestomgw/src/sqlite3.o bestomgw/src/timertask.o bestomgw/src/gateway.o 

OBJS = bestomgw/src/utils.o bestomgw/src/sqlite3.o bestomgw/src/zbController.o \
	bestomgw/src/zbSocCmd.o bestomgw/src/interface_devicelist.o bestomgw/src/interface_grouplist.o \
	bestomgw/src/interface_scenelist.o bestomgw/src/interface_srpcserver.o \
	bestomgw/src/socket_server.o bestomgw/src/SimpleDB.o bestomgw/src/SimpleDBTxt.o  bestomgw/src/cJSON.o
all: $(EXEC)

$(EXEC): $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) -lpthread -lm -lrt  -lglib-2.0    -ldl


clean:
	rm -rf $(OBJS) $(EXEC)

romfs:
	$(ROMFSINST) $(EXEC) /bin/$(EXEC)


#############################################################
# nfc daemon Makefile 
#############################################################
EXEC = apclient
#CFLAGS += -O2 -Wall -Ibestomgw/inc
CFLAGS += -O2 -Wall -std=gnu99 

##OBJS = bestomgw/src/utils.o bestomgw/src/sqlite3.o bestomgw/src/timertask.o bestomgw/src/gateway.o 

OBJS =bestomgw/src/sqlite3.o bestomgw/src/interface_protocol.o bestomgw/src/database.o\
	bestomgw/src/zbSocCmd.o bestomgw/src/socket_server.o bestomgw/src/msgqueue.o\
	bestomgw/src/socket_client.o bestomgw/src/cJSON.o\
	bestomgw/src/ap_protocol.o bestomgw/src/devicestatus.o bestomgw/src/log.o bestomgw/src/apclient.o 
all: $(EXEC)

$(EXEC): $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) -lpthread -lm -lrt -ldl -DTRANSACTION_ON


clean:
	rm -rf $(OBJS) $(EXEC)

romfs:
	$(ROMFSINST) $(EXEC) /bin/$(EXEC)


	
	
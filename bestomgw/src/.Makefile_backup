#############################################################
# smart home gateway Makefile 
#############################################################
EXEC = shomegateway
CFLAGS += -O2 -Wall -I./


OBJS = utils.o sqlite3.o zbController.o \
	zbSocCmd.o interface_devicelist.o interface_grouplist.o \
	interface_scenelist.o interface_srpcserver.o \
	socket_server.o SimpleDB.o SimpleDBTxt.o  cJSON.o
all: $(EXEC)

$(EXEC): $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) -lpthread -lm -lrt  -lglib-2.0    -ldl 


clean:
	rm -rf $(OBJS) $(EXEC)

romfs:
	$(ROMFSINST) $(EXEC) /bin/$(EXEC)
install:
        $(INSTALL) -D -m 0755 shomegateway $(TARGET_DIR)/bin

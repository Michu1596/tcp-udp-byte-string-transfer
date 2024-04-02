CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17
LFLAGS =

.PHONY: all clean

TARGET1 = client
TARGET2 = server

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(TARGET1).o common.o err.o 
$(TARGET2): $(TARGET2).o common.o err.o 

err.o: err.c err.h
common.o: common.c err.h common.h

client.o: client.c err.h common.h
server.o: server.c err.h common.h

clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~

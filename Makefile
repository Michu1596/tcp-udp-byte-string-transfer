CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17
LFLAGS =

.PHONY: all clean

TARGET1 = ppcbc
TARGET2 = ppcbs

all: $(TARGET1) $(TARGET2)

# $(TARGET1): $(TARGET1).o common.o err.o 
# $(TARGET2): $(TARGET2).o common.o err.o 

$(TARGET1): ppcbc.o common.o err.o 
$(TARGET2): ppcbs.o common.o err.o 

err.o: err.c err.h
common.o: common.c err.h common.h protconst.h

ppcbc.o: ppcbc.c err.h common.h protconst.h
ppcbs.o: ppcbs.c err.h common.h protconst.h

clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~

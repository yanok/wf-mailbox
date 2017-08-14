
CC = gcc
CFLAGS = -m64 -O2
LDLIBS = -lpthread

all : client multiclient

client : client.o hwfq.o

multiclient : multiclient.o hwfq.o

clean :
	rm -rf *.o client multiclient

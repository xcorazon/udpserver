CFLAGS := -std=c99 -g -Wall
LIBS := -lev
CC := gcc

.PHONY: all

udpserver : udpserver.o client.o
	$(CC) udpserver.o\
		  client.o\
		  $(LIBS)\
		  -o $@

udpserver.o: udpserver.c udpserver.h client.h
	$(CC) -c $(CFLAGS) -fdata-sections -ffunction-sections -o $@ udpserver.c

client.o: client.c client.h
	$(CC) -c $(CFLAGS) -fdata-sections -ffunction-sections -o $@ client.c
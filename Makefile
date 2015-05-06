#MAKEFILE for flacplay

CC=gcc
CFLAGS= -O3
LINKFLAGS= -lpthread -lao -lFLAC

flacplay.o: flacplay.c
	$(CC) -c flacplay.c
	
flacplay: flacplay.o
	$(CC) $(CFLAGS) flacplay.o -o flacplay $(LINKFLAGS)

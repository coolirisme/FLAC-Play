#MAKEFILE for flacplay

CC=clang
CFLAGS= -O3
DEBUGFLAGS= -g
LINKFLAGS= -lpthread -lao -lFLAC

flacplay.o: flacplay.c
	$(CC) -c flacplay.c
	
packetqueue.o: packetqueue.c
	$(CC) -c packetqueue.c
	
flacplay: packetqueue.o flacplay.o
	$(CC) $(CFLAGS) flacplay.o packetqueue.o -o flacplay $(LINKFLAGS)
	
debug: flacplay.c packetqueue.c
	$(CC) $(DEBUGFLAGS) flacplay.c packetqueue.c $(LINKFLAGS)
	
clean: flacplay.o packetqueue.o
	rm flacplay.o packetqueue.o


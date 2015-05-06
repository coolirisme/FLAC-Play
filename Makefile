all: 
	gcc -O3 flacplay.c -o flacplay -lpthread -lao -lFLAC

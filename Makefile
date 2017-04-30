CC = g++
DEBUG = -g -O0 -pedantic-errors
CFLAGS = -Wall -std=c++14 -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

all: 1730sh

1730sh: lab13.o
	$(CC) $(LFLAGS) -o 1730sh lab13.o

lab13.o: lab13.cpp Lab13.h
	$(CC) $(CFLAGS) lab13.cpp

.PHONY: clean

clean:
	rm -rf *.o
	rm -rf 1730sh


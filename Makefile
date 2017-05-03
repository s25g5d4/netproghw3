CC=gcc
CXX=g++
CFLAGS=-Wall -g
CXXFLAGS=-Wall -g -std=c++11
LDFLAGS=-g -pthread
LDLIBS=-lstdc++
SERVEROBJS=server.o my_send_recv.o commons.o
CLIENTOBJS=client.o my_send_recv.o commons.o

all: server client

server: $(SERVEROBJS)

client: $(CLIENTOBJS)

clean:
	rm -f *.o server client
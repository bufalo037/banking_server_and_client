CC = g++
FLAGS = -Wall -std=c++11

build: server client

server: server.o server.hpp
	$(CC) $(FLAGS) $< -o $@

server.o: server.cpp
	$(CC) $(FLAGS) -c $< -o $@

client: client.o
	gcc -Wall $< -lm -o $@

client.o: client.c
	gcc -Wall -c $< -o $@ 

.PHONY: clean

clean:
	rm -f *.o server client client-*
all: client server

client: client.o
	gcc -o client client.o -lm

client.o: client.c
	gcc -c -o client.o client.c
server: server.o
	gcc -o server server.o -lm
server.o: server.c
	gcc -c -o server.o server.c
clean:
	-rm *.o client server
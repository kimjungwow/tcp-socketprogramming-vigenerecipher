all: client server server_select

client: client.o
	gcc -o client client.o -lm
client.o: client.c
	gcc -c -o client.o client.c

server: server.o
	gcc -o server server.o -lm
server.o: server.c
	gcc -c -o server.o server.c

server_select: server_select.o
	gcc -o server_select server_select.o -lm
server_select.o: server_select.c
	gcc -c -o server_select.o server_select.c

clean:
	-rm *.o client server server_select
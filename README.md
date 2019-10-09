# TCP Socket Programming (Vigenère cipher) :computer:  
- Implement a connection-oriented, client-server protocol based on a given specification (TCP sockets)  
- Send/receive data via socket  
- Implement a string encryption/decryption service : [Vigenère cipher](https://en.wikipedia.org/wiki/Vigen%C3%A8re_cipher)  
## Components :memo:
- client.c : Communicate with the server based on the specifcation.  
- server.c : Communicate with multiple clients using fork().  
- server_select.c : Communicate with multiple clients using select().  

## How to compile? :walking:
  `make all : compile all of three source codes`  
  `make client : compile client.c`  
  `make server : compile server.c`  
  `make server_select : compile server_select.c`  
  `make clean : remove all object files and binary files.`

## How to run? :runner:
#### Make 50MB text file
  `tr -dc A-Za-z0-9 </dev/urandom | head -c 50000000 > test.txt`
#### Run server (server_select)
  `server -p [PORT]` or `server_select -p [PORT]`
#### Run client
  `./client -h 127.0.0.1 -p 7878 -o 0 -k abcd < test.txt > a.txt`  
  `./client -h 127.0.0.1 -p 7878 -o 1 -k abcd < a.txt > b.txt`  
  `diff -i test.txt b.txt`
#### Run multiple clients
##### Make shell scripts like below and execute 
  ```  
  for i in {1..20} ;  
do  
    ./client -h 127.0.0.1 -p 7878 -o 0 -k abcd < test.txt > a$i.txt &  
done  
```  
## Others :+1:
- I studied socket programming through this link.  
So I wrote code related to socket programming such as send() or getaddrinfo() referring to this link.  
https://beej.us/guide/bgnet/html/single/bgnet.html  
- Testcases in `testvectors` directory are encrypted with keyword `abcd`

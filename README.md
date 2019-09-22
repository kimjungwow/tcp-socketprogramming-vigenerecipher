Socket Programming  
Implement a connection-oriented, client-server protocol based on a given specification (TCP sockets)  
Send/receive data via socket  
Implement a string encryption/decryption service (Vigenere Cipher)  
Protocol Specification  
  ### Components

  ### How to compile?
  `make all : compile all of three source codes`  
  `make client : compile client.c`  
  `make server : compile server.c`  
  `make server_select : compile server_select.c`  
  
  `make clean removes all object files and binary files.`

  ### How to run?
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
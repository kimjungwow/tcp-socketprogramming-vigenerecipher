#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


#define PORT "3490"
#define BACKLOG 5
#define KEYWORD "cake"

#define MAXDATASIZE 100 // max number of bytes we can get at once 

struct header {
    short op;
    unsigned short checksum;
    char keyword[4];
    unsigned long long length;
};

int main(int argc, char *argv[])
{
    struct header *myheader = (struct header *)malloc(sizeof(struct header));
    
    
    char *host, *port;

    for (int a=1 ; a < argc ; a++) {
        if (!strcmp(argv[a],"-h")) {
            host = (char *) malloc (sizeof(char)*strlen(argv[a+1]));
            strcpy(host,argv[a+1]);
        } else if (!strcmp(argv[a],"-p")) {
            port = (char *) malloc (sizeof(char)*strlen(argv[a+1]));
            strcpy(port,argv[a+1]);
        } else if (!strcmp(argv[a],"-o")) {
            myheader->op=(unsigned short)(atoi(argv[a+1]));
        } else if (!strcmp(argv[a],"-k")) {
            strcpy(myheader->keyword,argv[a+1]);  
        }

    }
    myheader->length = 16; // Length of header is 16bytes.

    int socket_fd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    printf("HOST : %s    PORT : %s\n",host,port);

    if (getaddrinfo(host, port, &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        return 2;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((socket_fd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_fd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }


    freeaddrinfo(servinfo); // all done with this structure
    unsigned char buffertemp[MAXDATASIZE];
    unsigned char buffer[MAXDATASIZE];
    
    printf("op : %hd | checksum : %hd\n",myheader->op,myheader->checksum);

    fgets(buffertemp,MAXDATASIZE,stdin);

    
    printf("keyword : %s | length : %llu \n",myheader->keyword,myheader->length);
    buffer[0]='\0';
    memcpy(buffer+1,&myheader->op,sizeof(unsigned short));
    memcpy(buffer+2,&myheader->checksum,sizeof(unsigned short));
    memcpy(buffer+4,&myheader->keyword,sizeof(char)*4);
    memcpy(buffer+8,&myheader->length,sizeof(unsigned long long));
    // memcpy(buffer+16,buffertemp,strlen(buffertemp));
    strcpy(buffer+16,buffertemp);
    printf("buffertemp : %s\n",buffertemp);

    printf("myheader %x\n",buffer[0]);
    printf("myheader %x\n",buffer[1]);
    printf("myheader %x\n",buffer[2]);
    printf("myheader %c\n",buffer[16]);
    printf("myheader %c\n",buffer[17]);
    printf("myheader %c\n",buffer[18]);
    printf("%lu = length | %s\n",strlen(buffer),buffer);
    send(socket_fd,buffer,strlen(buffer),0);
    // send(socket_fd,argv[1],strlen(argv[1]),0);

    if ((numbytes = recv(socket_fd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);

    close(socket_fd);
    
    free(host);
    free(port);
    free(myheader);

    return 0;
}
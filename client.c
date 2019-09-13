#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <endian.h>

#define PORT "3490"
#define BACKLOG 5
#define KEYWORD "cake"

#define MAXDATASIZE 1024*1024 // max number of bytes we can get at once

struct header {
  unsigned short op;
  unsigned short checksum;
  char keyword[4];
  unsigned long long length;
  uint32_t nworder_length;
};

void calculate_checksum(unsigned short* a, unsigned short b) {
    unsigned short current = *a;
    // printf("%02x and %02x,",current,b);
    if (current + b > 65536)
        *a = current + b - 65535;
    else
        *a = current + b;
    
    // printf("Current checksum : %02x\n",*a);
}

int main(int argc, char *argv[]) {
  struct header *myheader = (struct header *)malloc(sizeof(struct header));

  char *host, *port;

  for (int a = 1; a < argc; a++) {
    if (!strcmp(argv[a], "-h")) {
        host = (char *)malloc(sizeof(char) * strlen(argv[a + 1]));
        strcpy(host, argv[a + 1]);
    } else if (!strcmp(argv[a], "-p")) {
        port = (char *)malloc(sizeof(char) * strlen(argv[a + 1]));
        strcpy(port, argv[a + 1]);
    } else if (!strcmp(argv[a], "-o")) {
        myheader->op = (unsigned short)(atoi(argv[a + 1]));
    } else if (!strcmp(argv[a], "-k")) {
        strcpy(myheader->keyword, argv[a + 1]);
    }
  }
  myheader->length = 16; // Length of header is 16bytes.
  myheader->checksum = 0;
  calculate_checksum(&myheader->checksum,myheader->op);
  calculate_checksum(&myheader->checksum,
      (unsigned short)(myheader->keyword[0])*(unsigned short)(256) + (unsigned short)(myheader->keyword[1]));
  calculate_checksum(&myheader->checksum,
      (unsigned short)(myheader->keyword[2])*(unsigned short)(256) + (unsigned short)(myheader->keyword[3]));




  int socket_fd, numbytes;
  char* buf = (char *)malloc(MAXDATASIZE * sizeof(char));
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, port, &hints, &servinfo) != 0) {
      perror("getaddrinfo");
      return 2;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
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
  unsigned char buffer_stdin[MAXDATASIZE];
  unsigned char buffer_stdintw[MAXDATASIZE];

  unsigned char *buffertemp = (unsigned char *)(MAXDATASIZE * sizeof(unsigned char));
  unsigned char *buffertempano = (unsigned char *)(MAXDATASIZE * sizeof(unsigned char));
  buffertemp=buffer_stdintw;
  buffertempano=buffer_stdin;

  int bytesread = MAXDATASIZE;
  while(fgets(buffertempano, sizeof(buffer_stdin), stdin)!=NULL) {
      strcat(buffertemp,buffertempano);
  }

//   fgets(buffertemp, sizeof(buffer_stdin), stdin);

  for (int b = 0; b < strlen(buffertemp); b++) {
      buffertemp[b] = tolower(buffertemp[b]);
  }
  unsigned char *buffer = (unsigned char *)malloc(sizeof(char) * (strlen(buffertemp) + 15));

  myheader->length += strlen(buffertemp) - 1;
  myheader->nworder_length = htonl((uint32_t)(myheader->length));
  

  for (int c = 0 ; c < (strlen(buffertemp)-1)/2 ; c++ ) {
      calculate_checksum(&myheader->checksum,
      (unsigned short)(buffertemp[2*c])*(unsigned short)(256) + (unsigned short)(buffertemp[2*c+1]));
  }
  
  if (strlen(buffertemp)%2==0)
      calculate_checksum(&myheader->checksum, (unsigned short)(256)*(unsigned short)(buffertemp[strlen(buffertemp)-2]));
      
  calculate_checksum(&myheader->checksum,(unsigned short)(myheader->length%65536));
  myheader->checksum = ~(myheader->checksum);
  myheader->checksum = htons(myheader->checksum);
  myheader->length = htobe64(myheader->length);

  memset(buffer, 0, 1);
  memcpy(buffer + 1, &myheader->op, sizeof(char));
  memcpy(buffer + 2, &myheader->checksum, sizeof(myheader->checksum));
  memcpy(buffer + 4, &myheader->keyword, sizeof(char) * 4); 
  memcpy(buffer + 8, &myheader->length, sizeof(unsigned long long));
  memcpy(buffer + 16, buffertemp, strlen(buffertemp));

  
  unsigned char *cursor = buffer;
/*
  printf("buffer : %s\n", buffertemp);
  for(int z=0;z<strlen(buffertemp)+15;z++) {
      printf("%02x ",*cursor);
      if(z%4==3)
          printf("| ");
      ++cursor;
  }
  printf("\n");

  */
//   send(socket_fd, buffer, strlen(buffertemp) + 15, 0);
  send(socket_fd, buffer, be64toh(myheader->length), 0);

  if ((numbytes = recv(socket_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
      perror("recv");
      exit(1);
  }

  buf[numbytes] = '\0';

//   printf("client: received '%s' with %d bytes\n", buf,numbytes);
  for (int y=16; y<numbytes;y++) {
      printf("%c",buf[y]);
  }
  printf("\n");
  close(socket_fd);

  free(host);
  free(port);
  free(myheader);
  free(buffer);
  return 0;
}
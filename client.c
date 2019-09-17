#include "client.h"

#define ONEMEGABYTE 1024 * 1024 /// max number of bytes we can get at once
#define HEADERSIZE 16           /// Length of header is always 16bytes.
// #define MAXPACKETSIZE ONEMEGABYTE * 10 - HEADERSIZE*3
#define MAXPACKETSIZE 10000000-15
#define MAXONEBYTE 256
#define MAXTWOBYTES 65536

int main(int argc, char *argv[]) {

  char *host, *port;
  int socket_fd, numbytes, a, sendbytes = 0;
  struct addrinfo hints, *servinfo, *p;
  char s[INET6_ADDRSTRLEN];

  /// Struct "header" for easily saving arguments
  struct header *myheader = (struct header *)calloc(sizeof(struct header),1);

  /// Read arguments
  for (a = 1; a < argc; a++) {
    if (!strcmp(argv[a], "-h")) {
      host = (char *)malloc(sizeof(char) * strlen(argv[a + 1]));
      strcpy(host, argv[a + 1]);
    } else if (!strcmp(argv[a], "-p")) {
      port = (char *)malloc(sizeof(char) * strlen(argv[a + 1]));
      strcpy(port, argv[a + 1]);
    } else if (!strcmp(argv[a], "-o")) {
      myheader->op = (unsigned short)(atoi(argv[a + 1]));
    } else if (!strcmp(argv[a], "-k")) {
      strcpy(myheader->arrKeyword_temp, argv[a + 1]);
    }
  } 

  /// Get address information of client, machine running this code.
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &hints, &servinfo) != 0) {
    perror("getaddrinfo");
    return 2;
  }

  /// Get socket and connect it to server
  if ((socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype,
                          servinfo->ai_protocol)) == -1) {
    perror("socket");
  }
  if (connect(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    close(socket_fd);
    perror("client: connect");
  }
  if (servinfo == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }
  freeaddrinfo(servinfo); // After connecting, server information won't be used

  /// Time to send and receive packets!
  /// 1. Read stdin using fgets(), put stdin and header into packet, send it to
  /// server and receive packet from server
  /// Since maximum packet size is 10MB, we will read (10M-16)bytes per one loop

  // unsigned char *buffer_stdin =
  //     (unsigned char *)calloc(ONEMEGABYTE * sizeof(unsigned char), 1);
unsigned char *concatenated_stdin =
        (unsigned char *)calloc(MAXPACKETSIZE * sizeof(unsigned char), 1);
  /// Concatenate stdin into one char*
  while (fgets(concatenated_stdin, MAXPACKETSIZE, stdin) != NULL) {
    // while (fgets(buffer_stdin, ONEMEGABYTE, stdin) != NULL) {
    // unsigned char *concatenated_stdin =
    //     (unsigned char *)calloc(MAXPACKETSIZE * sizeof(unsigned char), 1);

    shiftKeyword(myheader->arrKeyword, myheader->arrKeyword_temp, sendbytes);

    // sendbytes += readStdin(buffer_stdin, concatenated_stdin, stdin) % 4;
    sendbytes += strlen(concatenated_stdin)%4;

    /// 2. Put stdin and header into packet
    unsigned char *packet_to_send = (unsigned char *)calloc(
        sizeof(unsigned char) * (strlen(concatenated_stdin) + HEADERSIZE), 1);
    myheader->length = strlen(concatenated_stdin) + HEADERSIZE;
    myheader->nworder_length = htonl((uint32_t)(myheader->length));

    
      /// Checksum is one's complement of sum of op, keyword, length and data.
  /// First, let's add op.

  /// Even though we send multiple packets, their ops are same.
  /// It is wise to store it in advance.
    /// Checksum is one's complement of sum of op, keyword, length and data.

    getChecksum(myheader, concatenated_stdin);

    fillPacket(myheader, packet_to_send, concatenated_stdin);

    int readbytes = be64toh(myheader->length), tempreadbytes = readbytes;
    // printf("\nSEND %d readbytes\n",readbytes);
    send(socket_fd, packet_to_send, readbytes, 0);

    while (readbytes > 0) {
      char *buf = (char *)malloc(MAXPACKETSIZE * sizeof(char));

      if ((numbytes = recv(socket_fd, buf, MAXPACKETSIZE - 1, 0)) == -1) {
        perror("recv");
        exit(1);
      }

      buf[numbytes] = '\0';
      int startindex = 0;
      if (readbytes == tempreadbytes)
        startindex = HEADERSIZE;
      int y;
      for (y = startindex; y < numbytes; y++)
        printf("%c", buf[y]);

      readbytes -= (numbytes);
      free(buf);
    }

    // free(concatenated_stdin);
    free(packet_to_send);
    // memset(buffer_stdin, 0, ONEMEGABYTE * sizeof(unsigned char));
    memset(concatenated_stdin,0,MAXPACKETSIZE*sizeof(unsigned char));
  }

  close(socket_fd);

  free(host);
  free(port);
  free(myheader);

  return 0;
}

unsigned short addShorts(unsigned short a, unsigned short b) {
  if (a + b >= MAXTWOBYTES)
    return a + b - 65535;
  else
    return a + b;
}

void getChecksum(struct header *myheader, unsigned char *data) {
  unsigned short checksum = 0;
  myheader->checksum = 0;
  checksum = addShorts(checksum, myheader->op);
  checksum = addShorts(checksum, (unsigned short)(myheader->arrKeyword[0]) *
                                         (unsigned short)(MAXONEBYTE) +
                                     (unsigned short)(myheader->arrKeyword[1]));

  checksum = addShorts(checksum, (unsigned short)(myheader->arrKeyword[2]) *
                                         (unsigned short)(MAXONEBYTE) +
                                     (unsigned short)(myheader->arrKeyword[3]));
  int c, datalength = strlen(data) - 1;
  unsigned char *dataptr, *endpoint = &data[datalength];
  for (dataptr = data; dataptr < endpoint; dataptr += 2) {
    checksum = addShorts(checksum, (unsigned short)(*dataptr) *
                                           (unsigned short)(MAXONEBYTE) +
                                       (unsigned short)(*(dataptr + 1)));
  }
  if (strlen(data) % 2 == 1)
    checksum =
        addShorts(checksum, (unsigned short)(MAXONEBYTE) *
                                (unsigned short)(data[strlen(data) - 1]));
  unsigned long long lengthtemp = myheader->length;
  while (lengthtemp > 0) {
    checksum = addShorts(checksum, (unsigned short)(lengthtemp % MAXTWOBYTES));
    lengthtemp >>= 16;
  }
  checksum = ~checksum;
  myheader->checksum = checksum;
}

void fillPacket(struct header *myheader, unsigned char *packet_to_send,
                unsigned char *data) {
  myheader->checksum = htons(myheader->checksum);
  myheader->length = htobe64(myheader->length);
  memset(packet_to_send, 0, 1);
  memcpy(packet_to_send + 1, &myheader->op, sizeof(char));
  memcpy(packet_to_send + 2, &myheader->checksum, sizeof(myheader->checksum));
  memcpy(packet_to_send + 4, &myheader->arrKeyword, sizeof(char) * 4);
  memcpy(packet_to_send + 8, &myheader->length, sizeof(unsigned long long));
  strncpy(packet_to_send + 16, data, strlen(data));
}

void shiftKeyword(char *keyword, char *keyword_temp, int readbytes) {
  int shift = readbytes % 4, i;
  for (i = 0; i < 4; i++) {
    keyword[(i + shift) % 4] = keyword_temp[i];
  }
}

size_t readStdin(unsigned char *str, unsigned char *dst, FILE *stream) {
  strcat(dst, str);
  while (strlen(dst) + ONEMEGABYTE < MAXPACKETSIZE &&
         fgets(str, ONEMEGABYTE, stream) != NULL) {
    strcat(dst, str);
  }
  return strlen(dst);
}
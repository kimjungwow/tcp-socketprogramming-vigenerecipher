#include "client.h" 

int main(int argc, char *argv[]) {
  char *host, *port;
  int socket_fd, a, sendbytes = 0;
  struct addrinfo hints, *servinfo, *p;

  /// Struct "header" for easily saving arguments.
  struct header *myheader = (struct header *)calloc(sizeof(struct header), 1);

  /// Read arguments.
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

  /// Get address information of the client, machine running this code.
  memset(&hints, 0, sizeof hints); /// Clear before using hints
  hints.ai_family = AF_INET; /// Force version IPv4
  hints.ai_socktype = SOCK_STREAM; /// Use TCP Socket
  if (getaddrinfo(host, port, &hints, &servinfo) != 0) {
    perror("getaddrinfo");
    return -1;
  }

  /// Get socket and connect it to server.
  if ((socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype,
                          servinfo->ai_protocol)) == -1) {
    perror("socket");
    return -1;
  }
  if (connect(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    close(socket_fd);
    perror("client: connect");
  }
  if (servinfo == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return -1;
  }
  freeaddrinfo(servinfo); // After connecting, server information won't be used.

  /// Since maximum packet size is 10MB, read (10000000-16) bytes per one loop.
  unsigned char *readStdin =
      (unsigned char *)calloc(MAXDATASIZE * sizeof(unsigned char), 1);

  /// 1. Read stdin using fgets()
  while (fgets(readStdin, MAXDATASIZE, stdin) != NULL) {

    /// Depending on number of characters read, the keyword can be changed.
    shiftKeyword(myheader->arrKeyword, myheader->arrKeyword_temp, sendbytes);
    // sendbytes += strlen(readStdin) % KEYWORDSIZE;
    int temp = countAlphabet(readStdin);
    sendbytes = temp % KEYWORDSIZE;
    // sendbytes += temp % KEYWORDSIZE;

    /// 2. Calculate checksum, and put stdin and header into packet.
    unsigned char *packetToSend = (unsigned char *)calloc(
        sizeof(unsigned char) * (strlen(readStdin) + HEADERSIZE), 1);
    myheader->length = strlen(readStdin) + HEADERSIZE;
    // myheader->nworder_length = htonl((uint32_t)(myheader->length));
    /// Checksum is one's complement of sum of op, keyword, length and data.
    getChecksum(myheader, readStdin);
    fillPacket(myheader, packetToSend, readStdin);

    /// 3. Send packet to server
    int readbytes = be64toh(myheader->length), tempreadbytes = readbytes,
        startindex = 16;
    send(socket_fd, packetToSend, readbytes, 0);
    free(packetToSend); /// Free useless pointer.

    /// 4. Receive packet from server and print it.
    while (readbytes > 0) {
      readbytes -= receiveAndPrint(socket_fd, startindex);
      startindex = 0;
    }

    /// Clear buffer for using in the next iteration.
    memset(readStdin, 0, MAXDATASIZE * sizeof(unsigned char));
  }

  /// Close socket.
  close(socket_fd);

  /// Free useless pointers.
  free(readStdin);
  free(host);
  free(port);
  free(myheader);

  return 0;
}

unsigned short addShorts(unsigned short a, unsigned short b) {
  if (a + b >= MAXTWOBYTES) {
    return a + b - MAXTWOBYTES + 1; /// To cope with overflow.
  } else {
    return a + b;
  }
}

void getChecksum(struct header *myheader, unsigned char *data) {
  unsigned short checksum = 0;
  int c, datalength = strlen(data) - 1;
  unsigned char *dataptr, *endpoint = &data[datalength];
  unsigned long long lengthtemp = myheader->length;

  /// Checksum is one's complement of sum of op, keyword, length and data.
  checksum = addShorts(checksum, myheader->op);
  checksum = addShorts(checksum, (unsigned short)(myheader->arrKeyword[0]) *
                                         (unsigned short)(MAXONEBYTE) +
                                     (unsigned short)(myheader->arrKeyword[1]));
  checksum = addShorts(checksum, (unsigned short)(myheader->arrKeyword[2]) *
                                         (unsigned short)(MAXONEBYTE) +
                                     (unsigned short)(myheader->arrKeyword[3]));
  /// To reduce the time to run, use the pointer instead of directly accessing
  /// the index.
  for (dataptr = data; dataptr < endpoint; dataptr += 2) {
    checksum = addShorts(checksum, (unsigned short)(*dataptr) *
                                           (unsigned short)(MAXONEBYTE) +
                                       (unsigned short)(*(dataptr + 1)));
  }
  if (strlen(data) % 2 == 1)
    checksum =
        addShorts(checksum, (unsigned short)(MAXONEBYTE) *
                                (unsigned short)(data[strlen(data) - 1]));
  /// Length is composed of 64 bits.
  while (lengthtemp > 0) {
    checksum = addShorts(checksum, (unsigned short)(lengthtemp % MAXTWOBYTES));
    lengthtemp >>= 16;
  }
  checksum = ~checksum;
  myheader->checksum = checksum;
}

void fillPacket(struct header *myheader, unsigned char *packetToSend,
                unsigned char *data) {

  myheader->checksum = htons(myheader->checksum);
  myheader->length = htobe64(myheader->length);

  /**********************************************************************
    0             16                  32                                63
    | op(16 bits) | checksum(16 bits) |          keyword(32 bits)       |
    |                           length(64 bits)                         |
    |                               data                                |
    |                               data                                |
    |                               data                                |

  ************MAXIMUM LENGTH OF EACH MESSAGE IS LIMITED TO 10MB!*********

  **********************************************************************/

  memset(packetToSend, 0, 1);
  memcpy(packetToSend + 1, &myheader->op, sizeof(char));
  memcpy(packetToSend + 2, &myheader->checksum, sizeof(unsigned short));
  memcpy(packetToSend + 4, &myheader->arrKeyword, sizeof(char) * KEYWORDSIZE);
  memcpy(packetToSend + 8, &myheader->length, sizeof(unsigned long long));
  strncpy(packetToSend + HEADERSIZE, data, strlen(data));
}

void shiftKeyword(char *keyword, char *keyword_temp, int readbytes) {
  /// Depending on number of characters read, the keyword can be changed.
  /// If I already sent 9999981 characters when the keyword is 'cake',
  /// next keyword should be 'akec', not 'cake'.
  int shift = readbytes % KEYWORDSIZE, i;
  for (i = 0; i < KEYWORDSIZE; i++) {
    keyword[i] = keyword_temp[(i + shift) % KEYWORDSIZE];
  }
  for (i = 0; i < KEYWORDSIZE; i++) {
    keyword_temp[i] = keyword[i];
  }
}

int receiveAndPrint(int socket_fd, int startindex) {
  /// Receive message from server.
  char *buf = (char *)malloc(MAXDATASIZE * sizeof(char));
  int y, numbytes;
  if ((numbytes = recv(socket_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
    perror("recv");
    exit(1);
  }
  buf[numbytes] = '\0';
  /// Print message from server.
  for (y = startindex; y < numbytes; y++)
    printf("%c", buf[y]);
  free(buf);
  return numbytes;
}

int countAlphabet(unsigned char *data) {
  int count = 0, datalength = strlen(data) - 1;
  unsigned char *dataptr, *endpoint = &data[datalength];
  /// To reduce the time to run, use the pointer instead of directly accessing
  /// the index.
  for (dataptr = data; dataptr < endpoint; dataptr++) {
    if ((*dataptr >= 'A' && *dataptr <= 'Z') ||
        (*dataptr >= 'a' && *dataptr <= 'z'))
      count++;
  }
  return count;
}
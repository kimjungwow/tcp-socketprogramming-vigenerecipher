#include "client.h"

#define ONEMEGABYTE 1024 * 1024 /// max number of bytes we can get at once
#define HEADERSIZE 16           /// Length of header is always 16bytes.
#define MAXPACKETSIZE ONEMEGABYTE * 10 - HEADERSIZE

struct header {
  unsigned short op;
  unsigned short checksum;
  unsigned short checksum_temp;
  char keyword[4];
  char keyword_temp[4];
  unsigned long long length;
  uint32_t nworder_length;
};

void calculate_checksum(unsigned short *a, unsigned short b) {
  unsigned short current = *a;
  // printf("%02x and %02x,", current, b);
  if (current + b > 65536)
    *a = current + b - 65535;
  else
    *a = current + b;

  // printf("Current checksum : %02x\n", *a);
}

void shift_keyword(char *keyword, char *keyword_temp, int readbytes) {
  int shift = readbytes % 4;
  for (int i = 0; i < 4; i++) {
    keyword[(i + shift) % 4] = keyword_temp[i];
  }
}

void print_packet(unsigned char *buffer, int size) {
  unsigned char *cursor = buffer;

  for (int z = 0; z <size+HEADERSIZE; z++) {
    // for (int z = size+HEADERSIZE; z >0; z--) {
    printf("%02x ", *cursor);
    // printf("%02x : %d\n", *cursor,z);
    if (z % 4 == 3)
      printf("| ");
    ++cursor;
  }
  printf("\n");
}

int main(int argc, char *argv[]) {

  char *host, *port;
  int socket_fd, numbytes;
  struct addrinfo hints, *servinfo, *p;
  char s[INET6_ADDRSTRLEN];

  /// Struct header for easily saving arguments
  struct header *myheader = (struct header *)malloc(sizeof(struct header));
  myheader->length = HEADERSIZE;
  myheader->checksum = 0;

  /// Read arguments
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
      strcpy(myheader->keyword_temp, argv[a + 1]);
    }
  }

  /// Checksum is one's complement of sum of op, keyword, length and data.
  /// First, let's add op and keyword. Keyword is always 4 characters in this
  /// project.
  calculate_checksum(&myheader->checksum, myheader->op);

  /// Even though we send multiple packets, their op and keyword are same.
  /// It is wise to store it in advance.
  myheader->checksum_temp = myheader->checksum;

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
  // for (int yy = 0; yy < 100; yy++)
  //       printf("%d\n",yy);
  /// Time to send and receive packets!
  /// 1. Read stdin using fgets(), put stdin and header into packet, send it to
  /// server and receive packet from server
  /// Since maximum packet size is 10MB, we will read (10M-16)bytes per one loop
  unsigned char *buffer_stdin =
      (unsigned char *)malloc(ONEMEGABYTE * sizeof(unsigned char));
  int sendbytes = 0;
  /// Concatenate stdin into one char*
  while (fgets(buffer_stdin, sizeof(buffer_stdin), stdin) != NULL) {
    shift_keyword(myheader->keyword, myheader->keyword_temp, sendbytes);

    unsigned char *concatenated_stdin =
        (unsigned char *)malloc(MAXPACKETSIZE * sizeof(unsigned char));
    strcat(concatenated_stdin, buffer_stdin);
    // printf("%zu %zu \n",strlen(concatenated_stdin),strlen(buffer_stdin));
    // for (int yy = 0; yy < strlen(buffer_stdin); yy++)
    //     printf("%c", buffer_stdin[yy]);
    while (strlen(concatenated_stdin) + ONEMEGABYTE < MAXPACKETSIZE &&
           fgets(buffer_stdin, sizeof(buffer_stdin), stdin) != NULL) {
      strcat(concatenated_stdin, buffer_stdin);
      // printf("%zu %zu \n",strlen(concatenated_stdin),strlen(buffer_stdin));
      // for (int yy = 0; yy < strlen(buffer_stdin); yy++)
      //   printf("%c", buffer_stdin[yy]);
      // printf("\n");
    }
    // printf("%zu %zu
    // %zu\n",strlen(concatenated_stdin),ONEMEGABYTE,MAXPACKETSIZE);
    for (int b = 0; b < strlen(concatenated_stdin); b++) {
      concatenated_stdin[b] = tolower(concatenated_stdin[b]);
    }
    // printf("HERE?2 == %zu\n",strlen(concatenated_stdin));
    // print_packet(concatenated_stdin,strlen(concatenated_stdin));
    // for (int yy = 0; yy < (int)(strlen(concatenated_stdin)); yy++)
    // for (int yy = 0; yy < 100; yy++)
    //     printf("%d\n",yy);
      // printf("%c %d %d\n", concatenated_stdin[yy], yy,
    // (int)(strlen(concatenated_stdin)));
    // printf("%zu LENGTH\n", strlen(concatenated_stdin));
    /// 2. Put stdin and header into packet
    unsigned char *packet_to_send = (unsigned char *)calloc(
        sizeof(unsigned char) * (strlen(concatenated_stdin) + HEADERSIZE),1);
    myheader->length = strlen(concatenated_stdin) + HEADERSIZE;
    myheader->nworder_length = htonl((uint32_t)(myheader->length));
    myheader->checksum = myheader->checksum_temp;
    calculate_checksum(&myheader->checksum,
                       (unsigned short)(myheader->keyword[0]) *
                               (unsigned short)(256) +
                           (unsigned short)(myheader->keyword[1]));
    calculate_checksum(&myheader->checksum,
                       (unsigned short)(myheader->keyword[2]) *
                               (unsigned short)(256) +
                           (unsigned short)(myheader->keyword[3]));
    for (int c = 0; c < (strlen(concatenated_stdin)) / 2; c++) {
      printf("Below is index %d and %d\n",2*c,2*c+1);
      calculate_checksum(&myheader->checksum,
                         (unsigned short)(concatenated_stdin[2 * c]) *
                                 (unsigned short)(256) +
                             (unsigned short)(concatenated_stdin[2 * c + 1]));
    }
    if (strlen(concatenated_stdin) % 2 == 1)
      calculate_checksum(
          &myheader->checksum,
          (unsigned short)(256) *
              (unsigned short)(concatenated_stdin[strlen(concatenated_stdin) -
                                                  1]));
    unsigned long long lengthtemp = myheader->length;
    calculate_checksum(&myheader->checksum,
                         (unsigned short)(myheader->length % 65536));
    lengthtemp >>= 16;
    calculate_checksum(&myheader->checksum,
                         (unsigned short)(lengthtemp % 65536));
// calculate_checksum(&myheader->checksum,
//                          (unsigned short)((myheader->length>>16) % 65536));
    // while(lengthtemp>0) {
    //   printf("%llu = %02x | %hd \n", lengthtemp,lengthtemp, (unsigned short)(lengthtemp%65536));
    //   calculate_checksum(&myheader->checksum,
    //                      (unsigned short)(lengthtemp % 65536));
    //   lengthtemp >>= 16;
    // }

    myheader->checksum = ~(myheader->checksum);
    myheader->checksum = htons(myheader->checksum);
    myheader->length = htobe64(myheader->length);
    memset(packet_to_send, 0, 1);
    memcpy(packet_to_send + 1, &myheader->op, sizeof(char));
    memcpy(packet_to_send + 2, &myheader->checksum, sizeof(myheader->checksum));
    memcpy(packet_to_send + 4, &myheader->keyword, sizeof(char) * 4);
    memcpy(packet_to_send + 8, &myheader->length, sizeof(unsigned long long));
    // memcpy(packet_to_send + 16, concatenated_stdin, strlen(concatenated_stdin));
    strcpy(packet_to_send+16, concatenated_stdin);

    unsigned char *cursor = packet_to_send;

    int readbytes = be64toh(myheader->length), tempreadbytes = readbytes;
    printf("%c\n",packet_to_send[79999+16]);
    printf("%c\n",packet_to_send[79998+16]);
    print_packet(packet_to_send, strlen(concatenated_stdin));
    printf("\nDONE\n");
    send(socket_fd, packet_to_send, readbytes, 0);
    // printf("Here3? %zu | %02x\n", strlen(packet_to_send), readbytes);

    

    while (readbytes > 0) {
      char *buf = (char *)malloc(MAXPACKETSIZE * sizeof(char));

      if ((numbytes = recv(socket_fd, buf, MAXPACKETSIZE - 1, 0)) == -1) {
        perror("recv");
        exit(1);
      }
      // printf("%d\n", numbytes);

      buf[numbytes] = '\0';
      int startindex = 0;
      if (readbytes == tempreadbytes)
        startindex = HEADERSIZE;
      for (int y = startindex; y < numbytes; y++)
        printf("%c", buf[y]);

      free(buf);
      readbytes -= (numbytes);
      //   printf("%d %d\n",readbytes,numbytes);
    }
    // sleep(1000);
    free(concatenated_stdin);
    free(packet_to_send);
  }

  close(socket_fd);

  free(host);
  free(port);
  free(myheader);

  return 0;
}
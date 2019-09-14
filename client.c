#include "client.h"

#define ONEMEGABYTE 1024 * 1024 /// max number of bytes we can get at once
#define HEADERSIZE 16 /// Length of header is always 16bytes.
#define MAXPACKETSIZE ONEMEGABYTE * 10 - HEADERSIZE


struct header {
  unsigned short op;
  unsigned short checksum;
  unsigned short checksum_temp;
  char keyword[4];
  unsigned long long length;
  uint32_t nworder_length;
};

void calculate_checksum(unsigned short *a, unsigned short b) {
  unsigned short current = *a;
  if (current + b > 65536)
    *a = current + b - 65535;
  else
    *a = current + b;
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
      strcpy(myheader->keyword, argv[a + 1]);
    }
  }

  /// Checksum is one's complement of sum of op, keyword, length and data.
  /// First, let's add op and keyword. Keyword is always 4 characters in this
  /// project.
  calculate_checksum(&myheader->checksum, myheader->op);
  calculate_checksum(&myheader->checksum,
                     (unsigned short)(myheader->keyword[0]) *
                             (unsigned short)(256) +
                         (unsigned short)(myheader->keyword[1]));
  calculate_checksum(&myheader->checksum,
                     (unsigned short)(myheader->keyword[2]) *
                             (unsigned short)(256) +
                         (unsigned short)(myheader->keyword[3]));

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

  /// Time to send and receive packets!
  /// 1. Read stdin using fgets(), put stdin and header into packet, send it to server and receive packet from server
  /// Since maximum packet size is 10MB, we will read (10M-16)bytes per one loop
  unsigned char *buffer_stdin =
      (unsigned char *)malloc(ONEMEGABYTE * sizeof(unsigned char));
  /// Concatenate stdin into one char*
  while (fgets(buffer_stdin, sizeof(buffer_stdin), stdin) != NULL) {
    unsigned char *concatenated_stdin =
        (unsigned char *)malloc(MAXPACKETSIZE * sizeof(unsigned char));
    strcat(concatenated_stdin, buffer_stdin);
    while (strlen(concatenated_stdin) + ONEMEGABYTE < MAXPACKETSIZE &&
           fgets(buffer_stdin, sizeof(buffer_stdin), stdin) != NULL) {
      strcat(concatenated_stdin, buffer_stdin);
    }
    for (int b = 0; b < strlen(concatenated_stdin); b++) {
      concatenated_stdin[b] = tolower(concatenated_stdin[b]);
    }

    /// 2. Put stdin and header into packet
    unsigned char *packet_to_send =
        (unsigned char *)malloc(sizeof(char) * (strlen(concatenated_stdin) + HEADERSIZE));
    myheader->length = strlen(concatenated_stdin) + HEADERSIZE;
    myheader->nworder_length = htonl((uint32_t)(myheader->length));
    myheader->checksum = myheader->checksum_temp;
    for (int c = 0; c < (strlen(concatenated_stdin)) / 2; c++) {
      calculate_checksum(&myheader->checksum,
                         (unsigned short)(concatenated_stdin[2 * c]) *
                                 (unsigned short)(256) +
                             (unsigned short)(concatenated_stdin[2 * c + 1]));
    }
    if (strlen(concatenated_stdin) % 2 == 1)
      calculate_checksum(
          &myheader->checksum,
          (unsigned short)(256) *
              (unsigned short)(concatenated_stdin[strlen(concatenated_stdin) - 1]));
    calculate_checksum(&myheader->checksum,
                       (unsigned short)(myheader->length % 65536));
    myheader->checksum = ~(myheader->checksum);
    myheader->checksum = htons(myheader->checksum);
    myheader->length = htobe64(myheader->length);
    memset(packet_to_send, 0, 1);
    memcpy(packet_to_send + 1, &myheader->op, sizeof(char));
    memcpy(packet_to_send + 2, &myheader->checksum, sizeof(myheader->checksum));
    memcpy(packet_to_send + 4, &myheader->keyword, sizeof(char) * 4);
    memcpy(packet_to_send + 8, &myheader->length, sizeof(unsigned long long));
    memcpy(packet_to_send + 16, concatenated_stdin, strlen(concatenated_stdin));

    unsigned char *cursor = packet_to_send;

    int readbytes = be64toh(myheader->length), tempreadbytes = readbytes;
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
      for (int y = startindex; y < numbytes; y++)
        printf("%c", buf[y]);

      free(buf);
      readbytes -= (numbytes);
    }
    free(concatenated_stdin);
    free(packet_to_send);
  }

  close(socket_fd);

  free(host);
  free(port);
  free(myheader);

  return 0;
}
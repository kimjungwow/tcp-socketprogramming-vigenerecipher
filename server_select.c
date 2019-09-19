#include "server_select.h"

int main(int argc, char *argv[]) {
  struct addrinfo hints, *res, *p;
  int status, socket_fd, new_fd, numbytes, fdmax, listener;
  char ipstr[INET6_ADDRSTRLEN], *port;
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  struct sigaction sa;
  fd_set readfds, master;
  FD_ZERO(&master);
  FD_ZERO(&readfds);

  /// Read arguments
  int a;
  for (a = 1; a < argc; a++) {
    if (!strcmp(argv[a], "-p")) {
      port = (char *)malloc(sizeof(char) * strlen(argv[a + 1]));
      strcpy(port, argv[a + 1]);
    }
  }

  memset(&hints, 0, sizeof hints); // Clear before using hints
  hints.ai_family = AF_INET;       // Force version IPv4
  hints.ai_socktype = SOCK_STREAM; // Use TCP Socket
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

  if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }

  socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  int yes = 1;
  // lose the pesky "Address already in use" error message
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
    perror("setsockopt");
    exit(1);
  }

  bind(socket_fd, res->ai_addr, res->ai_addrlen);
  listen(socket_fd, BACKLOG);
  FD_SET(socket_fd, &master);
  fdmax = socket_fd;


  for (;;) {
    readfds = master;
    if (select(fdmax + 1, &readfds, NULL, NULL, NULL) == -1) {
      perror("select");
      exit(4);
    }
    int i;
    for (i = 0; i < fdmax + 1; i++) {
      if
        FD_ISSET(i, &readfds) {
          if (i == socket_fd) {
            new_fd =
                accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size);
            if (new_fd == -1) {
              perror("accept");
            } else {
              FD_SET(new_fd, &master);
              if (new_fd > fdmax)
                fdmax = new_fd;
            }

          } else {

            unsigned char *buf =
                (unsigned char *)calloc(sizeof(unsigned char) * MAXDATASIZE, 1);
            struct header *myheader =
                (struct header *)calloc(sizeof(struct header), 1);
            int firstrecv = 0;
            unsigned short op, checksum;
            unsigned char keyword[5];
            unsigned char keyword_temp[5];
            unsigned long long length = 0, comparelength = 0;
            int keyworditer;
            while ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) > 0) {
              int writebytes = 0;
              if (firstrecv == 0) {
                length = 0;
                keyworditer = 0;
                op = (unsigned short)buf[1];
                checksum = (unsigned short)buf[2] * (unsigned short)(256) +
                           (unsigned short)buf[3];
                strncpy(keyword, buf + 4, 4);
                keyword[4] = '\0';
                strncpy(keyword_temp, buf + 4, 4);
                keyword_temp[4] = '\0';
                int i;
                for (i = 15; i >= 8; i--) {
                  length += (unsigned long long)buf[i] *
                            (unsigned long long)(pow(256.0, (double)(15 - i)));
                  // printf("%llu NOW %d BECAUSE %llu * %llu\n", length, i,
                  //        (unsigned long long)buf[i],
                  //        (unsigned long long)(pow(16.0, (double)(15 - i))));
                }
                while (numbytes < length) {
                  numbytes += recv(new_fd, buf + numbytes, MAXDATASIZE, 0);
                }
                comparelength = length;
                myheader->op = op;
                strncpy(myheader->arrKeyword, buf + 4, 4);
                myheader->length = length;
                // printf(
                //     "%d op | %02x checksum | %s keyword | %d numbytes | %llu "
                //     "length | %p addr\n",
                //     op, checksum, keyword, numbytes, length, buf);
                if (op > 1 || op < 0 || comparelength > MAXPACKETSIZE ||
                    checkInvalidKeyword(keyword) ||
                    checkInvalidChecksum(myheader, buf, checksum, length)) {
                  close(new_fd);
                  free(myheader);
                  exit(0);
                }
              }

              unsigned char *data = (unsigned char *)calloc(
                  sizeof(unsigned char) * (numbytes), 1);
              int tempchar;
              // printf("%d op | %02x checksum | %s keyword | %d numbytes | %llu "
              //        "length "
              //        "| %p addr\n",
              //        op, checksum, keyword, numbytes, length, data);
              int k;
              for (k = 16 - 16 * firstrecv; k < numbytes; k++) {
                unsigned char tempchar = tolower(buf[k]);
                // printf("%c in %d | %c\n", tempchar, k, buf[k]);
                if (tempchar >= 'a' && tempchar <= 'z') {
                  if (op == 0) {
                    if (tempchar + keyword[keyworditer % 4] - 'a' > 'z')
                      data[k] = tempchar + keyword[keyworditer % 4] - 26 - 'a';
                    else
                      data[k] = tempchar + keyword[keyworditer % 4] - 'a';
                  } else {
                    if (tempchar - (keyword[keyworditer % 4] - 'a') < 'a')
                      data[k] =
                          tempchar - (keyword[keyworditer % 4] - 'a') + 26;
                    else
                      data[k] = tempchar - (keyword[keyworditer % 4] - 'a');
                  }
                  keyworditer++;

                } else {
                  data[k] = tempchar;
                }
              }

              int t;
              for (t = 16 * firstrecv; t < 16; t++) {
                data[t] = buf[t];
              }

              data[numbytes] = '\0';

              // printf("%d =new_fd | data = %p | %d = numbytes\n", new_fd, data,
              //        numbytes);
              int sentbytes;
              if ((sentbytes = send(new_fd, data, numbytes, 0)) == -1)
                perror("send");
              // printf("SEND! %d \n", sentbytes);
              comparelength -= numbytes;
              if (comparelength == 0)
                firstrecv = 0;
              else
                firstrecv = 1;
              // free(data);
              memset(buf, 0, sizeof(unsigned char) * MAXDATASIZE);
            }
            close(new_fd);
            FD_CLR(new_fd,&master);
            free(myheader);
          }
        }
    }
    
  }
  free(port);

  return 0;
}

void shiftKeyword(char *keyword, char *keyword_temp, int readbytes) {
  /// Depending on number of characters read, the keyword can be changed.
  /// If I already sent 9999981 characters when the keyword is 'cake',
  /// next keyword should be 'akec', not 'cake'.
  int shift = readbytes % KEYWORDSIZE, i;
  for (i = 0; i < KEYWORDSIZE; i++) {
    keyword[(i + shift) % KEYWORDSIZE] = keyword_temp[i];
  }
}

bool checkInvalidKeyword(unsigned char *keyword) {
  int i;
  for (i = 0; i < KEYWORDSIZE; i++) {
    if (keyword[i] < 'a' || keyword[i] > 'z')
      return true;
  }
  return false;
}

void getChecksum(struct header *myheader, unsigned char *data,
                 unsigned long long length) {
  unsigned short checksum = 0;
  int c, datalength = length - 1;
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
  dataptr = data + 16;
  for (dataptr = &data[16]; dataptr < endpoint; dataptr += 2) {
    checksum = addShorts(checksum, (unsigned short)(*dataptr) *
                                           (unsigned short)(MAXONEBYTE) +
                                       (unsigned short)(*(dataptr + 1)));
  }
  if (length % 2 == 1)
    checksum = addShorts(checksum, (unsigned short)(MAXONEBYTE) *
                                       (unsigned short)(data[length - 1]));
  /// Length is composed of 64 bits.
  while (lengthtemp > 0) {
    checksum = addShorts(checksum, (unsigned short)(lengthtemp % MAXTWOBYTES));
    lengthtemp >>= 16;
  }
  checksum = ~checksum;
  myheader->checksum = checksum;
}

bool checkInvalidChecksum(struct header *myheader, unsigned char *data,
                          unsigned short givenChecksum,
                          unsigned long long length) {
  getChecksum(myheader, data, length);
  // printf("GIVEN %02x | CORRECT %02x\n", givenChecksum, myheader->checksum);

  return myheader->checksum != givenChecksum;
}

unsigned short addShorts(unsigned short a, unsigned short b) {
  if (a + b >= MAXTWOBYTES) {
    // printf("%02x + %02x -> %02x\n", a, b, a + b - MAXTWOBYTES + 1);
    return a + b - MAXTWOBYTES + 1; /// To cope with overflow.
  } else {
    // printf("%02x + %02x -> %02x\n", a, b, a + b);
    return a + b;
  }
}

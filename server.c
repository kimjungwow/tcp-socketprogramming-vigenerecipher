#include "server.h"

int main(int argc, char *argv[]) {
  struct addrinfo hints, *res;
  int status, listener, new_fd, numOfBytesRecv, a;
  char *port;
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  struct sigaction sa;

  /// Read arguments
  for (a = 1; a < argc; a++) {
    if (!strcmp(argv[a], "-p")) {
      port = (char *)malloc(sizeof(char) * strlen(argv[a + 1]));
      strcpy(port, argv[a + 1]);
    }
  }

  /// Get address information of the server, machine running this code.
  memset(&hints, 0, sizeof hints); /// Clear before using hints
  hints.ai_family = AF_INET;       /// Force version IPv4
  hints.ai_socktype = SOCK_STREAM; /// Use TCP Socket
  hints.ai_flags = AI_PASSIVE;     /// fill in my IP for me
  if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    perror("getaddrinfo");
    return -1;
  }

  /// Get socket, bind it to port and listen for upcoming clients.
  listener = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  int yes = 1;
  /// If a socket that was connected is still hanging around the kernel,
  /// allowing it to reuse the port.
  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
    perror("setsockopt");
    exit(1);
  }
  bind(listener, res->ai_addr, res->ai_addrlen);
  listen(listener, BACKLOG);

  /// "server.c" uses multiple thread with fork(), so we must reap dead
  /// processes.
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  /// In infinite loop, parent process is continuously listening for upcoming
  /// clients.
  /// When a client comes, parent process make child process with fork(),
  /// to deal with that client.
  for (;;) {
    addr_size = sizeof client_addr;
    new_fd = accept(listener, (struct sockaddr *)&client_addr,
                    &addr_size); /// The client is arriving.

    if (!fork()) {
      close(listener); // Child process doesn't need the listener
      unsigned char *buf =
          (unsigned char *)calloc(sizeof(unsigned char) * MAXDATASIZE, 1);

      /// Struct "header" for easily saving arguments.
      struct header *myheader =
          (struct header *)calloc(sizeof(struct header), 1);

      int firstrecv = 0;
      unsigned short checksum;
      unsigned long long length = 0, givenLength = 0;
      int numOfAlphabetRead;
      /// 1. Read message from client
      while ((numOfBytesRecv = recv(new_fd, buf, MAXDATASIZE, 0)) > 0) {

        /// Read op, checksum, keyword, length from packet.
        if (firstrecv == 0) {
          myheader->length = 0;
          numOfAlphabetRead = 0;
          myheader->op = (unsigned short)buf[1];
          checksum = (unsigned short)buf[2] * (unsigned short)(MAXONEBYTE) +
                     (unsigned short)buf[3];
          strncpy(myheader->arrKeyword, buf + KEYWORDSIZE, KEYWORDSIZE);
          int i;
          for (i = 15; i >= 8; i--) {
            myheader->length +=
                (unsigned long long)buf[i] *
                (unsigned long long)(pow(256.0, (double)(15 - i)));
          }
          /// Read as much as the client sent.
          givenLength = myheader->length;
          while (numOfBytesRecv < givenLength) {
            numOfBytesRecv += recv(new_fd, buf + numOfBytesRecv, MAXDATASIZE, 0);
          }
          
          /// If protocol is invalid, close the socket.
          if (checkInvalidProtocol(myheader, buf, checksum)) {
            close(new_fd);
            free(myheader);
            exit(0);
          }
        }

        /// 2. Put header and lowercased and encrypted(decrypted) alphabets into packet.
        unsigned char *data =
            (unsigned char *)calloc(sizeof(unsigned char) * (numOfBytesRecv), 1);
        int k, t;
        for (k = HEADERSIZE - HEADERSIZE * firstrecv; k < numOfBytesRecv; k++) {
          unsigned char tempchar = tolower(buf[k]);
          if (tempchar >= 'a' && tempchar <= 'z') {
            if (myheader->op == 0) {
              if (tempchar + myheader->arrKeyword[numOfAlphabetRead % KEYWORDSIZE] - 'a' > 'z')
                data[k] =
                    tempchar + myheader->arrKeyword[numOfAlphabetRead % KEYWORDSIZE] - NUMOFALPHABETS - 'a';
              else
                data[k] =
                    tempchar + myheader->arrKeyword[numOfAlphabetRead % KEYWORDSIZE] - 'a';
            } else {
              if (tempchar - (myheader->arrKeyword[numOfAlphabetRead % KEYWORDSIZE] - 'a') <
                  'a')
                data[k] = tempchar -
                          (myheader->arrKeyword[numOfAlphabetRead % KEYWORDSIZE] - 'a') + NUMOFALPHABETS;
              else
                data[k] =
                    tempchar - (myheader->arrKeyword[numOfAlphabetRead % KEYWORDSIZE] - 'a');
            }
            numOfAlphabetRead++;
          } else {
            data[k] = tempchar;
          }
        }
        for (t = HEADERSIZE * firstrecv; t < HEADERSIZE; t++) {
          data[t] = buf[t];
        }
        data[numOfBytesRecv] = '\0';

        /// 3. Send packet to client
        int sentbytes;
        if ((sentbytes = send(new_fd, data, numOfBytesRecv, 0)) == -1)
          perror("send");
        givenLength -= numOfBytesRecv;
        if (givenLength == 0)
          firstrecv = 0;
        else
          firstrecv = 1;

        memset(buf, 0, sizeof(unsigned char) * MAXDATASIZE);
      }
      close(new_fd);
      free(myheader);
      exit(0);
    }
    close(new_fd); /// Parent process doesn't need this socket.
  }
  free(port); /// Not reached.
  return 0;
}

/// Check whether op is valid.
bool checkInvalidOp(struct header *myheader) {
  return myheader->op > 1 || myheader->op < 0;
}

/// Check whether length is valid.
bool checkInvalidLength(struct header *myheader) {
  return myheader->length > MAXPACKETSIZE;
}

/// Check whether keyword is valid.
bool checkInvalidKeyword(unsigned char *keyword) {
  int i;
  for (i = 0; i < KEYWORDSIZE; i++) {
    if (!((keyword[i] >= 'a' && keyword[i] <= 'z') || (keyword[i] >= 'A' && keyword[i] <= 'Z')))
      return true;
  }
  return false;
}

void getChecksum(struct header *myheader, unsigned char *data,
                 unsigned long long length) {
  unsigned short checksum = 0;
  int datalength = length - 1;
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
  dataptr = data + HEADERSIZE;
  for (dataptr = &data[HEADERSIZE]; dataptr < endpoint; dataptr += 2) {
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
    lengthtemp >>= HEADERSIZE;
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

bool checkInvalidProtocol(struct header *myheader, unsigned char *data,
                          unsigned short givenChecksum) {
  return (
      checkInvalidOp(myheader) || checkInvalidLength(myheader) ||
      checkInvalidKeyword(myheader->arrKeyword) ||
      checkInvalidChecksum(myheader, data, givenChecksum, myheader->length));
}

void sigchld_handler(int sig) {
  int olderrno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    errno = olderrno;
}
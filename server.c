#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
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

#define MAXDATASIZE 10 * 1024 * 1024 // max number of bytes we can get at once
#define BACKLOG 5

void sigchld_handler(int sig) {
  int olderrno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    errno = olderrno;
}

int main(int argc, char *argv[]) {
  struct addrinfo hints, *res, *p;
  int status, socket_fd, new_fd, numbytes;
  char ipstr[INET6_ADDRSTRLEN], *port;
  struct sockaddr_storage client_addr;
  socklen_t addr_size;
  struct sigaction sa;

  /// Read arguments
  for (int a = 1; a < argc; a++) {
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
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  for (;;) {
    addr_size = sizeof client_addr;
    new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size);

    if (!fork()) {      // this is the child process
      close(socket_fd); // child doesn't need the listener
      unsigned char *buf =
          (unsigned char *)malloc(sizeof(unsigned char) * MAXDATASIZE);

      // if ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
      //   perror("recv");
      //   exit(1);
      // }
      int firstrecv = 0;
      unsigned short op, checksum;
      unsigned char keyword[5];
      unsigned long long length = 0;
      int keyworditer = 0;
      while ((numbytes = recv(new_fd, buf, MAXDATASIZE - 1, 0)) > 0) {
        if (firstrecv == 0) {
          op = (unsigned short)buf[1];
          checksum = (unsigned short)buf[2] * (unsigned short)(256) +
                     (unsigned short)buf[3];
          strncpy(keyword, buf + 4, 4);
          keyword[4] = '\0';

          for (int i = 15; i >= 8; i--) {
            length += (unsigned long long)buf[i] *
                      (unsigned long long)(pow(256.0, (double)(15 - i)));
            printf("%llu NOW %d BECAUSE %llu * %llu\n", length, i,
                   (unsigned long long)buf[i],
                   (unsigned long long)(pow(16.0, (double)(15 - i))));
          }
        }

        // for (int j=0; j<4; j++) {
        //     keyword[j] += keyword[j]-'a';
        // }

        printf(
            "%d op | %02x checksum | %s keyword | %d numbytes | %llu length\n",
            op, checksum, keyword, numbytes, length);

        unsigned char *data =
            (unsigned char *)calloc(sizeof(unsigned char) * (numbytes), 1);
        for (int m = 16 - 16 * firstrecv; m < numbytes; m++) {
          buf[m] = tolower(buf[m]);
        }

        for (int k = 16 - 16 * firstrecv; k < numbytes; k++) {
          if (buf[k] >= 'a' && buf[k] <= 'z') {
            if (op == 0) {
              if (buf[k] + keyword[keyworditer % 4] - 'a' > 'z')
                data[k] = buf[k] + keyword[keyworditer % 4] - 26 - 'a';
              else
                data[k] = buf[k] + keyword[keyworditer % 4] - 'a';
            } else {
              if (buf[k] - (keyword[keyworditer % 4] - 'a') < 'a')
                data[k] = buf[k] - (keyword[keyworditer % 4] - 'a') + 26;
              else
                data[k] = buf[k] - (keyword[keyworditer % 4] - 'a');
            }
            keyworditer++;

          } else {
            data[k] = buf[k];
          }
        }
        for (int t = 16 * firstrecv; t < 16; t++) {
          data[t] = buf[t];
        }
        data[numbytes] = '\0';
        // for (int z = 0; z < numbytes; z++) {
        //   printf("%02x ", data[z]);
        //   if (z % 4 == 3)
        //     printf("| ");
        // }
        // printf("\n\n\n\n\n");
        if (send(new_fd, data, numbytes, 0) == -1)
          perror("send");
        firstrecv++;
      }
      close(new_fd);
      exit(0);
    }
    close(new_fd); // parent doesn't need this
  }

  return 0;
}
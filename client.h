#include <arpa/inet.h>
#include <ctype.h>
#include <endian.h>
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
#include <stdio_ext.h>

#define HEADERSIZE 16           /// Length of header is always 16bytes.
#define MAXPACKETSIZE 10000000 
#define MAXDATASIZE MAXPACKETSIZE-HEADERSIZE+1
#define MAXONEBYTE 256
#define MAXTWOBYTES 65536
#define KEYWORDSIZE 4

struct header {
  unsigned short op;
  unsigned short checksum;
  unsigned short checksum_temp;
  char arrKeyword[KEYWORDSIZE];      /// Keyword is always 4 characters.
  char arrKeyword_temp[KEYWORDSIZE]; /// The order of letters can be different per packet.
  unsigned long long length;
  uint32_t nworder_length;
};

unsigned short addShorts(unsigned short a, unsigned short b);
void getChecksum(struct header *myheader, unsigned char *data);
void fillPacket(struct header *myheader, unsigned char *packet_to_send,
                     unsigned char *data);
void shiftKeyword(char *keyword, char *keyword_temp, int readbytes);
int receiveAndPrint(int socket_fd, int startindex);
int countAlphabet(unsigned char *data);
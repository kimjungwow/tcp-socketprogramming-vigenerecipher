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

struct header {
  unsigned short op;
  unsigned short checksum;
  unsigned short checksum_temp;
  char arrKeyword[4];      /// Keyword is always 4 characters.
  char arrKeyword_temp[4]; /// The order of letters can be different per packet.
  unsigned long long length;
  uint32_t nworder_length;
};

unsigned short addShorts(unsigned short a, unsigned short b);
void getChecksum(struct header *myheader, unsigned char *data);
void fillPacket(struct header *myheader, unsigned char *packet_to_send,
                     unsigned char *data);
void shiftKeyword(char *keyword, char *keyword_temp, int readbytes);
size_t readStdin(unsigned char *str,  unsigned char* dst,FILE* stream);
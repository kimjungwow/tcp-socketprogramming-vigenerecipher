#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define PORT "3490"
#define BACKLOG 5

void sigchld_handler(int sig) {
    int olderrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
    errno = olderrno;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *res, *p;
    int status, socket_fd, new_fd, numbytes;
    char ipstr[INET6_ADDRSTRLEN];
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    struct sigaction sa;
    char buf[MAXDATASIZE];
    if (argc != 2) {
        fprintf(stderr,"usage: showip hostname\n");
        return 1;
    }

    memset(&hints, 0, sizeof hints); // Clear before using hints
    hints.ai_family = AF_INET; // Force version IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP Socket
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    printf("IP addresses for %s:\n\n", argv[1]);

    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    int yes=1;
    // lose the pesky "Address already in use" error message
    if (setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    } 
    
    bind(socket_fd, res->ai_addr, res->ai_addrlen);
    listen(socket_fd,BACKLOG);
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    for(;;) {
        addr_size = sizeof client_addr;
        new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size);


        if (!fork()) { // this is the child process
            close(socket_fd); // child doesn't need the listener

            if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            
            for (int i=0 ; i<strlen(buf) ; i++) {
                buf[i]=tolower(buf[i]);
            }
            printf("server: received '%s'\n",buf);
            if (send(new_fd, buf, strlen(buf), 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
        


    }




    return 0;
}
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>

volatile int some_global = 0;

void rofl(int arg)
{
    printf("Signal Handler (arg=%x)!\n", arg);
    some_global = 1;
}

int main(int argc, char **argv)
{
    /*  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
      if(sock == -1)
      {
        printf("Couldn't get a socket: %d [%s]\n", errno, strerror(errno));
        return 1;
      }

      struct timeval t;
      t.tv_sec = 30;

      fd_set readfd;
      FD_SET(sock, &readfd);

      char* tmp = (char*) malloc(2048);
      while(1)
      {
        select(sock + 1, &readfd, 0, 0, &t);
        int n = read(sock, tmp, 2048);
        if(n > 0)
          printf("interface received %d bytes\n", n);
      }
    */

    printf("Installing signal handler for SIGHUP (size of sigaction = %d)\n", sizeof(struct sigaction));

    signal(SIGALRM, rofl);
    alarm(10);

    printf("Waiting 10 seconds!\n");
    while(!some_global);
    printf("Done!\n");

    printf("Raising SIGHUP\n");

    raise(SIGHUP);

    printf("Raise returns\n");

    while (1);

    return 0;
}

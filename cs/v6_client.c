#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/select.h>

#ifndef __GNU_LIBRARY
# include "fixes.h"
#endif

#define BUFLEN 16384

void usage();
void debug();
char *get_ip_str();

unsigned short DEBUG_LEVEL=0;

int main(argc, argv, envp)
int argc;
char **argv;
char **envp;
{
  int rval, sockfd6;
  struct addrinfo addrinfo;
  struct addrinfo *res, *r;
  struct hostent *host_ent;
  int e_save;
  int success;
  char **addrlist;
  fd_set read_fds, write_fds, except_fds;
  char buf[BUFLEN];
  char s[BUFLEN];
  int mlen;

  if (argc == 4)
  {
    if (strncmp(argv[2], "-d\0", 3) !=0) usage(argv[0], "Invalid option.");
    rval=(int) strtol(argv[3], NULL, 0);
    if (errno != 0)
    {
      fprintf(stderr, "%s: Invalid debug level %s\n", argv[0], argv[3]);
      exit(2);
    }
    else
    {
      DEBUG_LEVEL=rval;
    }
  }
  else if (argc != 2)
  {
    usage(argv[0], "Incorrect arguments.");
  }
  /* Get address info for specified host and demo service */
  memset(&addrinfo, 0, sizeof(addrinfo));
  addrinfo.ai_family=PF_UNSPEC;
  addrinfo.ai_socktype=SOCK_STREAM;
  addrinfo.ai_protocol=IPPROTO_TCP;
  if (rval = getaddrinfo(argv[1], "5001", &addrinfo, &res) != 0) {
    fprintf(stderr, "%s: Failed to resolve address information.\n", argv[0]);
    exit(2);
  }

  for (r=res; r; r = r->ai_next) {
    sockfd6 = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
    if (connect(sockfd6, r->ai_addr, r->ai_addrlen) < 0)
    {
      e_save = errno;
      (void) close(sockfd6);
      errno = e_save;
      fprintf(stderr, "%s: Failed attempt to %s.\n", argv[0], 
		get_ip_str((struct sockaddr *)r->ai_addr, buf, BUFLEN));
      perror("Socket error");
    } else {
      snprintf(s, BUFLEN, "%s: Succeeded to %s.", argv[0],
		get_ip_str((struct sockaddr *)r->ai_addr, buf, BUFLEN));
      debug(5, argv[0], s);
      success++;
      break;
    }
  }
  if (success == 0)
  {
    fprintf(stderr, "%s: Failed to connect to %s.\n", argv[0], argv[1]);
    freeaddrinfo(res);
    exit(5);
  }
  printf("%s: Successfully connected to %s at %s on FD %d.\n", argv[0], argv[1],
	get_ip_str((struct sockaddr *)r->ai_addr, buf, BUFLEN),
	sockfd6);
  freeaddrinfo(res);
  while(1)
  {
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    FD_SET(fileno(stdin), &read_fds);
    FD_SET(fileno(stdin), &except_fds);
    FD_SET(sockfd6, &read_fds);
    FD_SET(sockfd6, &except_fds);
    select(fileno(stdin) > sockfd6 ? fileno(stdin)+1 : sockfd6+1,
	&read_fds, &write_fds, &except_fds, NULL);
    if (FD_ISSET(fileno(stdin), &except_fds))
    {
      if (feof(stdin))
      {
        close(sockfd6);
        printf("End of file detected, exiting.\n");
        exit(0);
      }
      else
      {
        perror("Exception on STDIN");
        printf("Exiting.\n");
        exit(6);
      }
    }
    if (FD_ISSET(sockfd6, &except_fds))
    {
      perror("Exception on socket.");
      printf("Exiting.\n");
      exit(7);
    }
    if (FD_ISSET(sockfd6, &read_fds))
    {
      /* Read from socket and display to user */
      mlen = recv(sockfd6, (void *)buf, BUFLEN-1, MSG_DONTWAIT);
      buf[mlen]=0;
      if (mlen == 0)
      {
        fprintf(stderr, "Remote site has apparently hung up on us.\n");
	exit(0);
      }
      else
      {
        printf("Received %d bytes: %s", mlen, buf);
      }
    }
    if (FD_ISSET(fileno(stdin), &read_fds))
    {
      fgets(buf, BUFLEN, stdin);
      snprintf(s, BUFLEN, "Sent %d octets to server.", 
        send(sockfd6, (void *)buf, (size_t) strnlen(buf, BUFLEN), 0));
      debug(5,argv[0],  s);
    }
  } 
  exit(0);
}

void usage(prog, string)
char *prog;
char *string;
{
  fprintf(stderr, "Error: %s\n", string);
  fprintf(stderr, "Usage: %s <server>\n", prog);
  exit(1);
}

void debug(lvl, prog, string)
unsigned short lvl;
char *prog;
char *string;
{
  if (lvl <= DEBUG_LEVEL)
  {
    fprintf(stderr, "%s: %s\n", prog, string);
  }
}

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
  switch(sa->sa_family) {
    case AF_INET:
      inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
      break;

    case AF_INET6:
      inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
      break;

    default:
      strncpy(s, "Unknown AF", maxlen);
      return s;
  }
  return s;
}


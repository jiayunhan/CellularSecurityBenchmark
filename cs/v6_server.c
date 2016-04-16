#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>

#ifndef __GNU_LIBRARY
# include "fixes.h"
#endif

#define BUFLEN 16384

#define MAXCLIENTS 512

unsigned short DEBUG_LEVEL = 0;


void usage();
void debug();

int main(argc, argv, envp)
int argc;
char **argv;
char **envp;
{
  int i, j, rval;
  int sockfd6;
  int nclients, maxfd;
  int clients[MAXCLIENTS];
  struct hostent *host_ent;
  struct sockaddr_in6 dest_sin6;
  socklen_t socklen;
  int so_optval;
  struct servent *srvp;
  int e_save;
  int success;
  char **addrlist;
  fd_set read_fds, write_fds, except_fds;
  char buf[BUFLEN];
  int mlen;
  char s[BUFLEN];
  char u[BUFLEN];
  time_t t;
  char *ts;

  /* Check to see if debug level specified */
  if (argc == 3)
  {
    if (strncmp(argv[1], "-d\0", 3) !=0) usage(argv[0], "Invalid option.");
    rval=(int) strtol(argv[2], NULL, 0);
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
  /* Otherwise, no arguments expected. */
  else if (argc != 1)
  {
    usage(argv[0], "Incorrect arguments.");
  }

  /* Create an empty IPv6 socket interface specification */
  (void) memset(&dest_sin6, 0, sizeof(dest_sin6));
  /* Set up for IPv6 */
  dest_sin6.sin6_family = AF_INET6;

  /* Choose demo/tcp service */
 
  dest_sin6.sin6_port = htons(5001);

  /* Bind to any and all local addresses (in6addr_any) */
  dest_sin6.sin6_addr = in6addr_any;

  /* Create the sockets */
  if ((sockfd6 = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP)) == -1)
  {
    snprintf(s, BUFLEN, "%s: failed to create socket for v6 listener\0", argv[0]);
    perror(s);
    exit(4);
  }
  snprintf(s, BUFLEN, "V6 Socket created: %d\n", sockfd6);
  debug(5, argv[0], s);

  /* Set to non-blocking */
  if (fcntl(sockfd6, F_SETFL, O_NONBLOCK) < 0)
  {
    snprintf(s, BUFLEN, "%s: could not set v6 nonblocking\0", argv[0]);
    perror(s);
    exit(5);
  }
  snprintf(s, BUFLEN, "Socket set to v6 non-blocking: %d\n", sockfd6);
  debug(5, argv[0], s);

  /* Mark as re-usable (accept more than one connection to same socket) */
  so_optval = 1;
  if (setsockopt(sockfd6, SOL_SOCKET, SO_REUSEADDR, (char *) &so_optval,
		sizeof(SO_REUSEADDR)) < 0)
  {
    snprintf(s, BUFLEN, "%s: setsockopt on %d failed\0", argv[0], sockfd6);
    perror(s);
    exit(6);
  }

  /* Actually bind the socket to the port and addresses */

  if (bind(sockfd6, (const void*)&dest_sin6, sizeof(dest_sin6)) == -1)
  {
    snprintf(s, BUFLEN, "%s: bind v6 failed\0", argv[0]);
    perror(s);
    exit(7);
  }

  /* Tell the kernel to listen for new connections, queue up to 10 connections */
  listen(sockfd6, 10);

  /* Track the highest active file descriptor number for select */
  maxfd = (fileno(stdin) > sockfd6 ? fileno(stdin) : sockfd6);

  nclients = 0;
  while(1)
  {
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);
    FD_SET(sockfd6, &read_fds);
    FD_SET(sockfd6, &except_fds);
    for (i = 0; i < nclients; i++)
    {
      snprintf(s, BUFLEN, "FD_SET %d [%d] for read and exceptions.\n", i, clients[i]);
      debug(5, argv[0], s);
      FD_SET(clients[i], &read_fds);
      FD_SET(clients[i], &except_fds);
    }
    snprintf(s, BUFLEN, "Entering select with maxfd: %d\n", maxfd);
    debug(5, argv[0], s);
    /* Wait for someone to do something */
    select(maxfd + 1, &read_fds, &write_fds, &except_fds, NULL);

    /* Process an exception on the socket itself */
    if (FD_ISSET(sockfd6, &except_fds))
    {
      perror("Exception on socket.");
      fprintf(stderr, "Exiting.\n");
      exit(7);
    }

    /* A read event on the socket is a new connection */
    if (FD_ISSET(sockfd6, &read_fds))
    {
      socklen = sizeof(dest_sin6);
      /* Accept the new connection */
      rval = accept(sockfd6, (struct sockaddr *) &dest_sin6, &socklen);
      if (rval == -1)
      {
        (void) inet_ntop(dest_sin6.sin6_family, dest_sin6.sin6_addr.s6_addr,
									buf, BUFLEN);
        (void) snprintf(s, BUFLEN, "V6 Accept failed for %s %d\0",
		buf, dest_sin6.sin6_port);
        perror(s);
      }
      else
      {
        /* Too many clients? */
        if (nclients == MAXCLIENTS)
        {
          (void) send(rval, "Too many clients, please try later.\n",
		strlen("Too many clients, please try later.\n"), MSG_DONTWAIT);
          close(rval);
        }
        else
        {
          /* Add client to list of clients */
          clients[nclients++] = rval;
          if (rval > maxfd) maxfd = rval;
          (void) inet_ntop(dest_sin6.sin6_family, dest_sin6.sin6_addr.s6_addr,
								buf, BUFLEN);
          snprintf(s, BUFLEN, "Accepted V6 connection from %s %d as %d\n",
		buf, dest_sin6.sin6_port, rval);
          debug(1, argv[0], s);
          snprintf(s, BUFLEN, "You are client %d [%d]. You are now connected.\n\0",
			nclients, rval);
          send(rval, s, strnlen(s, BUFLEN), MSG_DONTWAIT);
        }
      }
    }

    /* Check for events from each client */
    for (i = 0; i < nclients; i++)
    {
      snprintf(s, BUFLEN,  "Checking client %d [%d] for read indicator.\n",
		i, clients[i]);
      debug(5, argv[0], s);
      /* Client read events */
      if (FD_ISSET(clients[i], &read_fds))
      {
        snprintf(s, BUFLEN,  "Client %d [%d] marked for read.\n", i, clients[i]);
        debug(1, argv[0], s);
        /* Read from client */
        if ((rval=recv(clients[i], buf, BUFLEN-1, MSG_DONTWAIT)) < 1)
        {
          snprintf(s, BUFLEN, "Short recv %d octets from %d [%d]\0",  rval,
			i, clients[i]);
          perror(s);
          /* Treat a 0 byte receive as an exception */
          FD_SET(clients[i], &except_fds);
        }
        buf[rval]=0;
        snprintf(s, BUFLEN,  "Received: %d (%d) bytes containing %s", rval,
							strnlen(buf, BUFLEN), buf);
        debug(5, argv[0], s);
        t=time(NULL);
        ts=ctime(&t);
        ts[24]=0;
        snprintf(s, BUFLEN, "Client %d [%d] at %s: %s\0", i, clients[i],
		ts,
		buf);
        snprintf(u, BUFLEN,  "Message Length: %d, %s", strnlen(s, BUFLEN), s);
        debug(5, argv[0], u);
        /* Send the message to every other client */
        for(j=0; j < nclients; j++)
        {
          /* Skip the sender */
          if (j == i) continue;
          /* Send the message */
	  printf("Jack:%s",s);
          send(clients[j], s, strnlen(s, BUFLEN), MSG_DONTWAIT);
        }
      }
      /* Client eception events */
      if (FD_ISSET(clients[i], &except_fds))
      {
        /* Close the client connection */
        close(clients[i]);
        /* Flag the client as no longer connected */
        clients[i]=-1;
      }
    } 
    /* Remove disconnected clients from list and recompute maxfd */
    maxfd = fileno(stdin);
    if (sockfd6 > maxfd) maxfd = sockfd6;
    /* Iterate through and condense list of clients */
    for(i=0; i < nclients; i++)
    {
      if (clients[i] == -1)
      {
        snprintf(s, BUFLEN,  "Client: %d Removed.\n", i);
        debug(1, argv[0], s);
        for(j=i; j < nclients-1; j++)
        {
          clients[j]=clients[j+1];
        }
        nclients--;
      }
      if (clients[i] > maxfd) maxfd = clients[i];
      snprintf(s, BUFLEN,  "End of loop %d / %d (%d)\n", i, nclients, maxfd);
      debug(5, argv[0], s);
    }
    snprintf(s, BUFLEN,  "Finished removal loop (maxfd: %d).\n", maxfd);
    debug(3, argv[0], s);
  }
  exit(0);
}

void usage(prog, string)
char *prog;
char *string;
{
  fprintf(stderr, "Error: %s\n", string);
  fprintf(stderr, "Usage: %s\n", prog);
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


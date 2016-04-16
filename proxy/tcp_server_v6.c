#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(char *msg){
	perror(msg);
	exit(1);
}
int main(int argc, char *argv[]){
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in6 serv_addr, cli_addr;
	int n;
	char client_addr_ipv6[100];
	if (argc < 2){
		fprintf(stderr, "Usage: %s \n", argv[0]);
		exit(0);
	}
	printf("\nIPv6 TCP Server Started...\n");
	//Socket Layer Call: socket()
	sockfd = socket(AF_INET6, SOCK_STREAM, 0);
	if(sockfd < 0)
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin6_flowinfo = 0;
	serv_addr.sin6_family = AF_INET6;
	serv_addr.sin6_addr = in6addr_any;
	serv_addr.sin6_port = htons(portno);
	
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))<0)
		error("ERROR on binding");
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	
	newsockfd = accept (sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if(newsockfd < 0)
		error("ERROR on accept");
	inet_ntop(AF_INET6, &(cli_addr.sin6_addr),client_addr_ipv6,100);
	printf("Incoming connection from client having IPv6 address: %s\n", client_addr_ipv6);
	memset(buffer, 0, 256);
	n = recv(newsockfd, buffer, 255, 0);
	if(n<0){
		error("ERROR reading from socket");
	}
	printf("Message from client: %s\n",buffer);
	n = send(newsockfd, "Server got your message", 23+1, 0);
	if(n<0)
		error("ERROR writing to socket");
	close(sockfd);
	close(newsockfd);
	return 0;
}

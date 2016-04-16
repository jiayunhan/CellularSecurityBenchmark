/*  Copyright (C) 2011-2013  P.D. Buchan (pdbuchan@yahoo.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Send an IPv4 TCP packet via raw socket.
// Stack fills out layer 2 (data link) information (MAC addresses) for us.
// Values set for SYN packet, no TCP options data.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()

#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_TCP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#define __FAVOR_BSD           // Use BSD format of tcp header
#include <netinet/tcp.h>      // struct tcphdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq

#include <errno.h>            // errno, perror()

// Define some constants.
#define IP4_HDRLEN 20         // IPv4 header length
#define TCP_HDRLEN 20         // TCP header length, excludes options data

// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t tcp4_checksum (struct ip, struct tcphdr);
char *allocate_strmem (int);
uint8_t *allocate_ustrmem (int);
int *allocate_intmem (int);

	int
main (int argc, char **argv)
{
	if(argc != 7)
	{
		printf("- Invalid parameters!!!\n");
		printf("- Usage: %s <source hostname/IP> <source port> <target hostname/IP> <target port> <sequence number> <ack number>\n", argv[0]);
		exit(-1);
	}
	int i, status, sd, *ip_flags, *tcp_flags;
	const int on = 1;
	char *interface, *target, *src_ip, *dst_ip;
	struct ip iphdr;
	struct tcphdr tcphdr;
	uint8_t *packet;
	struct addrinfo hints, *res;
	struct sockaddr_in *ipv4, sin;
	struct ifreq ifr;
	void *tmp;

	// Allocate memory for various arrays.
	packet = allocate_ustrmem (IP_MAXPACKET);
	interface = allocate_strmem (40);
	target = allocate_strmem (40);
	src_ip = allocate_strmem (INET_ADDRSTRLEN);
	dst_ip = allocate_strmem (INET_ADDRSTRLEN);
	ip_flags = allocate_intmem (4);
	tcp_flags = allocate_intmem (8);

	// Interface to send packet through.
	strcpy (interface, "em4");
	//strcpy (interface, "wlan0");

	// Submit request for a socket descriptor to look up interface.
	if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror ("socket() failed to get socket descriptor for using ioctl() ");
		exit (EXIT_FAILURE);
	}

	// Use ioctl() to look up interface index which we will use to
	// bind socket descriptor sd to specified interface with setsockopt() since
	// none of the other arguments of sendto() specify which interface to use.
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	if (ioctl (sd, SIOCGIFINDEX, &ifr) < 0) {
		perror ("ioctl() failed to find interface ");
		return (EXIT_FAILURE);
	}
	close (sd);
	printf ("Index for interface %s is %i\n", interface, ifr.ifr_ifindex);

	// Source IPv4 address: you need to fill this out
	strcpy (src_ip, argv[1]);

	// Destination URL or IPv4 address: you need to fill this out
	strcpy (target, argv[3]);

	// Fill out hints for getaddrinfo().
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = hints.ai_flags | AI_CANONNAME;

	// Resolve target using getaddrinfo().
	if ((status = getaddrinfo (target, NULL, &hints, &res)) != 0) {
		fprintf (stderr, "getaddrinfo() failed: %s\n", gai_strerror (status));
		exit (EXIT_FAILURE);
	}
	ipv4 = (struct sockaddr_in *) res->ai_addr;
	tmp = &(ipv4->sin_addr);
	if (inet_ntop (AF_INET, tmp, dst_ip, INET_ADDRSTRLEN) == NULL) {
		status = errno;
		fprintf (stderr, "inet_ntop() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}
	freeaddrinfo (res);

	// IPv4 header

	// IPv4 header length (4 bits): Number of 32-bit words in header = 5
	iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);

	// Internet Protocol version (4 bits): IPv4
	iphdr.ip_v = 4;

	// Type of service (8 bits)
	iphdr.ip_tos = 0;

	// Total length of datagram (16 bits): IP header + TCP header
	iphdr.ip_len = htons (IP4_HDRLEN + TCP_HDRLEN);

	// ID sequence number (16 bits): unused, since single datagram
	iphdr.ip_id = htons (0);

	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

	// Zero (1 bit)
	ip_flags[0] = 0;

	// Do not fragment flag (1 bit)
	ip_flags[1] = 1;

	// More fragments following flag (1 bit)
	ip_flags[2] = 0;

	// Fragmentation offset (13 bits)
	ip_flags[3] = 0;

	iphdr.ip_off = htons ((ip_flags[0] << 15)
			+ (ip_flags[1] << 14)
			+ (ip_flags[2] << 13)
			+  ip_flags[3]);

	// Time-to-Live (8 bits): default to maximum value
	iphdr.ip_ttl = 255;

	// Transport layer protocol (8 bits): 6 for TCP
	iphdr.ip_p = IPPROTO_TCP;

	// Source IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}

	// Destination IPv4 address (32 bits)
	if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1) {
		fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
		exit (EXIT_FAILURE);
	}


	// TCP header

	// Source port number (16 bits)
	tcphdr.th_sport = htons (atoi(argv[2]));

	// Sequence number (32 bits)
	tcphdr.th_seq = htonl (0);

	// Acknowledgement number (32 bits): 0 in first packet of SYN/ACK process
	tcphdr.th_ack = htonl (atoi(argv[6]));

	// Reserved (4 bits): should be 0
	tcphdr.th_x2 = 0;

	// Data offset (4 bits): size of TCP header in 32-bit words
	tcphdr.th_off = TCP_HDRLEN / 4;

	// Flags (8 bits)

	// FIN flag (1 bit)
	tcp_flags[0] = 0;

	// SYN flag (1 bit): set to 1
	tcp_flags[1] = 0;

	// RST flag (1 bit)
	tcp_flags[2] = 0;

	// PSH flag (1 bit)
	tcp_flags[3] = 1;

	// ACK flag (1 bit)
	tcp_flags[4] = 1;

	// URG flag (1 bit)
	tcp_flags[5] = 0;

	// ECE flag (1 bit)
	tcp_flags[6] = 0;

	// CWR flag (1 bit)
	tcp_flags[7] = 0;

	tcphdr.th_flags = 0;
	for (i=0; i<8; i++) {
		tcphdr.th_flags += (tcp_flags[i] << i);
	}

	// Window size (16 bits)
	tcphdr.th_win = htons (65535);

	// Urgent pointer (16 bits): 0 (only valid if URG flag is set)
	tcphdr.th_urp = htons (0);

	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
	iphdr.ip_sum = 0;
	iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);
	// The kernel is going to prepare layer 2 information (ethernet frame header) for us.
	// For that, we need to specify a destination for the kernel in order for it
	// to decide where to send the raw datagram. We fill in a struct in_addr with
	// the desired destination IP address, and pass this structure to the sendto() function.
	memset (&sin, 0, sizeof (struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;

	// Submit request for a raw socket descriptor.
	if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		perror ("socket() failed ");
		exit (EXIT_FAILURE);
	}

	// Set flag so socket expects us to provide IPv4 header.
	if (setsockopt (sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof (on)) < 0) {
		perror ("setsockopt() failed to set IP_HDRINCL ");
		exit (EXIT_FAILURE);
	}

	// Bind socket to interface index.
	if (setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof (ifr)) < 0) {
		perror ("setsockopt() failed to bind to interface ");
		exit (EXIT_FAILURE);
	}

	// Prepare packet.

	// First part is an IPv4 header.
	memcpy (packet, &iphdr, IP4_HDRLEN * sizeof (uint8_t));



	unsigned int count=0;
	struct timeval start, end;
	gettimeofday(&start, NULL);
	unsigned int seq = atoi(argv[5]);
	unsigned int port = atoi(argv[4]);
	
	//Send one crafted packet
	tcphdr.th_dport = htons(port);
	tcphdr.th_seq = htonl(seq);
	tcphdr.th_sum = 0;
	tcphdr.th_sum = tcp4_checksum(iphdr,tcphdr);
	memcpy ((packet + IP4_HDRLEN),&tcphdr, TCP_HDRLEN * sizeof (uint8_t));
	if(sendto(sd,packet,IP4_HDRLEN + TCP_HDRLEN, 0, (struct sockaddr *) &sin, sizeof(struct sockaddr))<0){
		perror("sendto() failed\n");
		exit(EXIT_FAILURE);
	}
	/*
	for(; port < 1100; port++) 
	{ 
		// Destination port number (16 bits)
		//tcphdr.th_dport = htons (atoi(argv[4]));
		tcphdr.th_dport = htons (port);
		for (seq = 0; seq < 0xffffffff - 0x8000; seq += 0x8000) 
		{
			tcphdr.th_seq=htonl(seq);
			tcphdr.th_sum=0;
			tcphdr.th_sum = tcp4_checksum (iphdr, tcphdr);
			memcpy ((packet + IP4_HDRLEN), &tcphdr, TCP_HDRLEN * sizeof (uint8_t));
			if (sendto (sd, packet, IP4_HDRLEN + TCP_HDRLEN, 0, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)  {
				perror ("sendto() failed ");
				exit (EXIT_FAILURE);
			}
		}
		//printf("Finished attacking port %d\n", port);
	}*/

	// Close socket descriptor.
	close (sd);

	// Free allocated memory.
	free (packet);
	free (interface);
	free (target);
	free (src_ip);
	free (dst_ip);
	free (ip_flags);
	free (tcp_flags);

	return (EXIT_SUCCESS);
}

// Checksum function
	uint16_t
checksum (uint16_t *addr, int len)
{
	int nleft = len;
	int sum = 0;
	uint16_t *w = addr;
	uint16_t answer = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= sizeof (uint16_t);
	}

	if (nleft == 1) {
		*(uint8_t *) (&answer) = *(uint8_t *) w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
}

// Build IPv4 TCP pseudo-header and call checksum function.
	uint16_t
tcp4_checksum (struct ip iphdr, struct tcphdr tcphdr)
{
	uint16_t svalue;
	char buf[IP_MAXPACKET], cvalue;
	char *ptr;
	int chksumlen = 0;

	// ptr points to beginning of buffer buf
	ptr = &buf[0];

	// Copy source IP address into buf (32 bits)
	memcpy (ptr, &iphdr.ip_src.s_addr, sizeof (iphdr.ip_src.s_addr));
	ptr += sizeof (iphdr.ip_src.s_addr);
	chksumlen += sizeof (iphdr.ip_src.s_addr);

	// Copy destination IP address into buf (32 bits)
	memcpy (ptr, &iphdr.ip_dst.s_addr, sizeof (iphdr.ip_dst.s_addr));
	ptr += sizeof (iphdr.ip_dst.s_addr);
	chksumlen += sizeof (iphdr.ip_dst.s_addr);

	// Copy zero field to buf (8 bits)
	*ptr = 0; ptr++;
	chksumlen += 1;

	// Copy transport layer protocol to buf (8 bits)
	memcpy (ptr, &iphdr.ip_p, sizeof (iphdr.ip_p));
	ptr += sizeof (iphdr.ip_p);
	chksumlen += sizeof (iphdr.ip_p);

	// Copy TCP length to buf (16 bits)
	svalue = htons (sizeof (tcphdr));
	memcpy (ptr, &svalue, sizeof (svalue));
	ptr += sizeof (svalue);
	chksumlen += sizeof (svalue);

	// Copy TCP source port to buf (16 bits)
	memcpy (ptr, &tcphdr.th_sport, sizeof (tcphdr.th_sport));
	ptr += sizeof (tcphdr.th_sport);
	chksumlen += sizeof (tcphdr.th_sport);

	// Copy TCP destination port to buf (16 bits)
	memcpy (ptr, &tcphdr.th_dport, sizeof (tcphdr.th_dport));
	ptr += sizeof (tcphdr.th_dport);
	chksumlen += sizeof (tcphdr.th_dport);

	// Copy sequence number to buf (32 bits)
	memcpy (ptr, &tcphdr.th_seq, sizeof (tcphdr.th_seq));
	ptr += sizeof (tcphdr.th_seq);
	chksumlen += sizeof (tcphdr.th_seq);

	// Copy acknowledgement number to buf (32 bits)
	memcpy (ptr, &tcphdr.th_ack, sizeof (tcphdr.th_ack));
	ptr += sizeof (tcphdr.th_ack);
	chksumlen += sizeof (tcphdr.th_ack);

	// Copy data offset to buf (4 bits) and
	// copy reserved bits to buf (4 bits)
	cvalue = (tcphdr.th_off << 4) + tcphdr.th_x2;
	memcpy (ptr, &cvalue, sizeof (cvalue));
	ptr += sizeof (cvalue);
	chksumlen += sizeof (cvalue);

	// Copy TCP flags to buf (8 bits)
	memcpy (ptr, &tcphdr.th_flags, sizeof (tcphdr.th_flags));
	ptr += sizeof (tcphdr.th_flags);
	chksumlen += sizeof (tcphdr.th_flags);

	// Copy TCP window size to buf (16 bits)
	memcpy (ptr, &tcphdr.th_win, sizeof (tcphdr.th_win));
	ptr += sizeof (tcphdr.th_win);
	chksumlen += sizeof (tcphdr.th_win);

	// Copy TCP checksum to buf (16 bits)
	// Zero, since we don't know it yet
	*ptr = 0; ptr++;
	*ptr = 0; ptr++;
	chksumlen += 2;

	// Copy urgent pointer to buf (16 bits)
	memcpy (ptr, &tcphdr.th_urp, sizeof (tcphdr.th_urp));
	ptr += sizeof (tcphdr.th_urp);
	chksumlen += sizeof (tcphdr.th_urp);

	return checksum ((uint16_t *) buf, chksumlen);
}

// Allocate memory for an array of chars.
	char *
allocate_strmem (int len)
{
	void *tmp;

	if (len <= 0) {
		fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
		exit (EXIT_FAILURE);
	}

	tmp = (char *) malloc (len * sizeof (char));
	if (tmp != NULL) {
		memset (tmp, 0, len * sizeof (char));
		return (tmp);
	} else {
		fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_strmem().\n");
		exit (EXIT_FAILURE);
	}
}

// Allocate memory for an array of unsigned chars.
	uint8_t *
allocate_ustrmem (int len)
{
	void *tmp;

	if (len <= 0) {
		fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_ustrmem().\n", len);
		exit (EXIT_FAILURE);
	}

	tmp = (uint8_t *) malloc (len * sizeof (uint8_t));
	if (tmp != NULL) {
		memset (tmp, 0, len * sizeof (uint8_t));
		return (tmp);
	} else {
		fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_ustrmem().\n");
		exit (EXIT_FAILURE);
	}
}

// Allocate memory for an array of ints.
	int *
allocate_intmem (int len)
{
	void *tmp;

	if (len <= 0) {
		fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_intmem().\n", len);
		exit (EXIT_FAILURE);
	}

	tmp = (int *) malloc (len * sizeof (int));
	if (tmp != NULL) {
		memset (tmp, 0, len * sizeof (int));
		return (tmp);
	} else {
		fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_intmem().\n");
		exit (EXIT_FAILURE);
	}
}

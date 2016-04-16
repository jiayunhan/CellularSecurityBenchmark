#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>


#define ETH_HDRLEN 14
#define IP6_HDRLEN 40
#define UDP_HDRLEN 8

uint16_t checksum(uint16_t *, int);
uint16_t udp6_checksum(struct ip6_hdr, struct udphdr, uint8_t *,int);
char *allocate_strmem (int);
uint8_t *allocate_ustrmem (int);
int main(int argc, char **argv){
	int i, status, datalen, frame_length, sd, bytes,srclen;
	char * interface,*source, *target, *src_ip, *dst_ip;
	struct ip6_hdr iphdr;
	struct udphdr udphdr;
	uint8_t *data, *packet, *outpack, *psdhdr;
	struct addrinfo hints, *res;
	struct sockaddr_in6 *ipv6, src, dst,sin;
	struct sockaddr_ll device;
	struct ifreq ifr;
	struct cmsghdr *cmsghdr;
	void *tmp;
	
	data = allocate_ustrmem(IP_MAXPACKET);
	interface = allocate_strmem(40);
	source = allocate_strmem(40);
	target = allocate_strmem(INET6_ADDRSTRLEN);
	src_ip = allocate_strmem(INET6_ADDRSTRLEN);
	dst_ip = allocate_strmem(INET6_ADDRSTRLEN);
	packet = allocate_ustrmem(IP_MAXPACKET);
	//Interface to send packet through
	strcpy (interface, "em4");
	if((sd = socket(AF_INET6, SOCK_RAW, IPPROTO_IPV6))<0){
		perror("socket() failed to get socket descriptor");
		exit(EXIT_FAILURE);
	}
	memset (&ifr, 0, sizeof(ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	if(ioctl(sd,SIOCGIFINDEX, &ifr)<0){
		perror ("ioctl() failed to find interface");
		return (EXIT_FAILURE);
	}
	close(sd);
	printf ("Index for interface %s is %i\n", interface, ifr.ifr_ifindex);
	strcpy (src_ip, "2607:f018:600:4:5908:b84a:23f5:5cb8");
	strcpy (target, "2600:1007:8026:e27e:0:45:9f02:5901");
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = hints.ai_flags | AI_CANONNAME;

	//Resolve source using getaddrinfo()
	/*
	if(status = getaddrinfo(source, NULL, &hints, &res)!=0){
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
		return (EXIT_FAILURE);
	}	
	memset(&src, 0, sizeof(src));
	memcpy(&src, res->ai_addr, res->ai_addrlen);
	srclen = res->ai_addrlen;
	memcpy(psdhdr, src.sin6_addr.s6_addr, 16*sizeof(uint8_t));
	freeaddrinfo(res);
	*/
	//Resolve target using getaddrinfo().
	if((status = getaddrinfo(target, NULL, &hints, &res))!=0){
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(status));
		exit(EXIT_FAILURE);
	}
	ipv6 = (struct sockaddr_in6 *) res->ai_addr;	
	tmp = &(ipv6->sin6_addr);
	//memset (&dst, 0, sizeof(dst));
	//memcpy (&dst, res->ai_addr, res->ai_addrlen);
	//memcpy (psdhdr+16, dst.sin6_addr.s6_addr, 16*sizeof(uint8_t));
	//freeaddrinfo(res);	

	if(inet_ntop(AF_INET6, tmp, dst_ip, INET6_ADDRSTRLEN) == NULL){
		status = errno;
		fprintf(stderr, "inet_ntop() failed.\nError message: %s",strerror(status));
		exit(EXIT_FAILURE);
	}
	//freeaddrinfo(res);
	//UDP data
	datalen = 1;
	data[0] = 'T';
	//IPv6 header
	iphdr.ip6_flow = htonl((6<<28) | (0<<20) | 0);
	iphdr.ip6_plen = htons(UDP_HDRLEN + datalen);
	iphdr.ip6_nxt = IPPROTO_UDP;
	iphdr.ip6_hops = 255;
	if((status = inet_pton (AF_INET6,src_ip,&(iphdr.ip6_src)))!=1){
		fprintf(stderr, "inet_pton() failed.\nError message: %s",strerror (status));
		exit (EXIT_FAILURE);
	}
	if((status = inet_pton(AF_INET6,dst_ip,&(iphdr.ip6_dst)))!=1){
		fprintf(stderr, "inet_pton() failed.\nError message: %s",strerror (status));
		exit (EXIT_FAILURE);
	}
	//UDP header
	udphdr.source = htons(4950);
	udphdr.dest = htons(1234);
	udphdr.len = htons (UDP_HDRLEN + datalen);
	udphdr.check = udp6_checksum (iphdr, udphdr, data, datalen);
	memcpy(packet, &iphdr, IP6_HDRLEN*sizeof(uint8_t));
	memcpy(packet+IP6_HDRLEN, &udphdr, UDP_HDRLEN*sizeof(uint8_t));
	memcpy(packet+IP6_HDRLEN+UDP_HDRLEN, data, datalen*sizeof(uint8_t));
	//Let kernel prepare layer 2 information for us
	//memset(&sin, 0, sizeof (struct sockaddr_in6));
	//sin.sin6_family = AF_INET6;
	//sin.sin6_addr.s6_addr = iphdr.ip6_dst.s6_addr;
	
	//submit request for a raw socket descriptor
	if((sd = socket(AF_INET6, SOCK_RAW, IPPROTO_IPV6))<0){
		perror("socket() failed");
		exit(EXIT_FAILURE);
	}
	//set flag so socket expects us to provide IPV6 header
	
	//bind socket to interface index
	if(setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))<0){
		perror("setsockopt() failed to bind to interface ");
		exit(EXIT_FAILURE);
	}
	while(1){
	if(sendto (sd, packet, IP6_HDRLEN+UDP_HDRLEN+datalen, 0, res->ai_addr, res->ai_addrlen)<0){
		perror ("sendto() failed ");
		exit(EXIT_FAILURE);
		}
	usleep(100000);
	}
	close(sd);
	
	free (data);
	free (packet);
	free (interface);
	free (target);
	free (src_ip);
	free (dst_ip);
	return (EXIT_SUCCESS);
}

// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
uint16_t
checksum (uint16_t *addr, int len)
{
  int count = len;
  register uint32_t sum = 0;
  uint16_t answer = 0;

  // Sum up 2-byte values until none or only one byte left.
  while (count > 1) {
    sum += *(addr++);
    count -= 2;
  }

  // Add left-over byte, if any.
  if (count > 0) {
    sum += *(uint8_t *) addr;
  }

  // Fold 32-bit sum into 16 bits; we lose information by doing this,
  // increasing the chances of a collision.
  // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }

  // Checksum is one's compliment of sum.
  answer = ~sum;

  return (answer);
}

// Build IPv6 UDP pseudo-header and call checksum function (Section 8.1 of RFC 2460).
uint16_t
udp6_checksum (struct ip6_hdr iphdr, struct udphdr udphdr, uint8_t *payload, int payloadlen)
{
  char buf[IP_MAXPACKET];
  char *ptr;
  int chksumlen = 0;
  int i;

  ptr = &buf[0];  // ptr points to beginning of buffer buf

  // Copy source IP address into buf (128 bits)
  memcpy (ptr, &iphdr.ip6_src.s6_addr, sizeof (iphdr.ip6_src.s6_addr));
  ptr += sizeof (iphdr.ip6_src.s6_addr);
  chksumlen += sizeof (iphdr.ip6_src.s6_addr);

  // Copy destination IP address into buf (128 bits)
  memcpy (ptr, &iphdr.ip6_dst.s6_addr, sizeof (iphdr.ip6_dst.s6_addr));
  ptr += sizeof (iphdr.ip6_dst.s6_addr);
  chksumlen += sizeof (iphdr.ip6_dst.s6_addr);

  // Copy UDP length into buf (32 bits)
  memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
  ptr += sizeof (udphdr.len);
  chksumlen += sizeof (udphdr.len);

  // Copy zero field to buf (24 bits)
  *ptr = 0; ptr++;
  *ptr = 0; ptr++;
  *ptr = 0; ptr++;
  chksumlen += 3;

  // Copy next header field to buf (8 bits)
  memcpy (ptr, &iphdr.ip6_nxt, sizeof (iphdr.ip6_nxt));
  ptr += sizeof (iphdr.ip6_nxt);
  chksumlen += sizeof (iphdr.ip6_nxt);

  // Copy UDP source port to buf (16 bits)
  memcpy (ptr, &udphdr.source, sizeof (udphdr.source));
  ptr += sizeof (udphdr.source);
  chksumlen += sizeof (udphdr.source);

  // Copy UDP destination port to buf (16 bits)
  memcpy (ptr, &udphdr.dest, sizeof (udphdr.dest));
  ptr += sizeof (udphdr.dest);
  chksumlen += sizeof (udphdr.dest);

  // Copy UDP length again to buf (16 bits)
  memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
  ptr += sizeof (udphdr.len);
  chksumlen += sizeof (udphdr.len);

  // Copy UDP checksum to buf (16 bits)
  // Zero, since we don't know it yet
  *ptr = 0; ptr++;
  *ptr = 0; ptr++;
  chksumlen += 2;

  // Copy payload to buf
  memcpy (ptr, payload, payloadlen * sizeof (uint8_t));
  ptr += payloadlen;
  chksumlen += payloadlen;

  // Pad to the next 16-bit boundary
  for (i=0; i<payloadlen%2; i++, ptr++) {
    *ptr = 0;
    ptr++;
    chksumlen++;
  }

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



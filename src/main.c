#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>
#include <ifaddrs.h>
#include <netdb.h>

struct dns_header
{
	unsigned short id;
	unsigned short flags;
	unsigned short qdcount;
	unsigned short ancount;
	unsigned short nscount;
	unsigned short arcount;
};

static void print_hex_data(const unsigned char* data, int size)
{
	for (int i = 0; i < size; ++i) 
	{
		printf("%02x ", data[i]);
		if ((i + 1) % 16 == 0) 
			printf("\n");
	}
	printf("\n");
}

static char* extract_dns_name(const unsigned char* dns_data, int len, char* domain_buffer, int buffer_size)
{
	int domain_len = 0;
	int i = 0;
	
	domain_buffer[0] = '\0';
	
	while (i < len) 
	{
		unsigned char length = dns_data[i++];

		if (length == 0) 
			break;
		
		if (i + length > len) 
			break;
		
		if (domain_len > 0 && domain_len < buffer_size - 1) 
		{
			domain_buffer[domain_len] = '.';
			++domain_len;
		}
		
		if (domain_len + length >= buffer_size - 1)
			break;
		
		memcpy(domain_buffer + domain_len, &dns_data[i], length);
		domain_len += length;
		domain_buffer[domain_len] = '\0';
		
		i += length;
	}
	
	return domain_buffer;
}

static void list_interfaces(void)
{
	printf("Available network interfaces:\n");
	
	struct ifaddrs *ifaddr, *ifa;
	
	if (getifaddrs(&ifaddr) == -1) 
	{
		fprintf(stderr, "getifaddrs failed: %s\n", strerror(errno));
		return;
	}
	
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
	{
		if (ifa->ifa_addr == NULL) 
			continue;
		
		if (ifa->ifa_addr->sa_family == AF_INET) 
		{
			char host[NI_MAXHOST];
			getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
				host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			printf(" - %s: %s\n", ifa->ifa_name, host);
		}
	}
	
	freeifaddrs(ifaddr);
	printf("\n");
}

int main(void)
{
	list_interfaces();
	
	if (geteuid() != 0) 
	{
		fprintf(stderr, "This program requires root privileges. Try running with sudo.\n");
		return 1;
	}
	
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) 
	{
		fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
		return 1;
	}
	
	printf("Socket created successfully\n");
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(53);
	
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
	{
		fprintf(stderr, "Bind failed: %s\n", strerror(errno));
		close(sock);
		return 1;
	}
	
	printf("Socket bound successfully\n");
	printf("Starting DNS traffic monitoring...\n\n");
	printf("Time\t\t\tDomain Name\n");
	printf("------------------------------------------------\n");
	
	unsigned char buffer[4096];
	char domain_buffer[256];
	
	while (1) 
	{
		struct sockaddr_in src_addr;
		socklen_t addr_len = sizeof(src_addr);
		
		ssize_t data_size = recvfrom(sock, buffer, sizeof(buffer), 0, 
			(struct sockaddr*)&src_addr, &addr_len);
		
		if (data_size < 0) 
		{
			fprintf(stderr, "Failed to receive packet: %s\n", strerror(errno));
			continue;
		}
		
		printf("Received packet of size %zd bytes from %s:%d\n", 
			data_size, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
		
		if ((size_t)data_size >= sizeof(struct dns_header)) 
		{
			struct dns_header* dns = (struct dns_header*)buffer;
			unsigned short qdcount = ntohs(dns->qdcount);
			printf("DNS query count: %d\n", qdcount);
			
			if (qdcount > 0) 
			{
				unsigned char* query_section = buffer + sizeof(struct dns_header);
				
				extract_dns_name(query_section, data_size - sizeof(struct dns_header), 
				                	domain_buffer, sizeof(domain_buffer));
				
				time_t now = time(0);
				char time_str[64];

				strftime(time_str, sizeof(time_str),
					"%Y-%m-%d %H:%M:%S", localtime(&now));
				
				printf("%s\t%s\n", time_str, domain_buffer);
				
				printf("Packet data (first 32 bytes):\n");
				print_hex_data(buffer, (data_size < 32) ? data_size : 32);
				printf("\n");
			}
		}
	}
	
	close(sock);
	return 0;
}

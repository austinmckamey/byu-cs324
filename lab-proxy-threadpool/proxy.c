#include "sbuf.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS  8
#define SBUFSIZE  5

sbuf_t sbuf; 

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int open_sfd(char *);
void *handle_client(void *vargp);
int parse_server_request(char *, char *, char *, char *, char *);
int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);


int main(int argc, char *argv[])
{
	int sfd = open_sfd(argv[1]);
	int fd;
	pthread_t tid; 
	sbuf_init(&sbuf, SBUFSIZE);
	for (int i = 0; i < NTHREADS; i++) {
		pthread_create(&tid, NULL, handle_client, NULL);  
	}
	while(1) {
		struct sockaddr_storage peer_addr;
		socklen_t peer_addr_len;
		peer_addr_len = sizeof(struct sockaddr_storage);
		memset(&peer_addr, 0, sizeof(struct sockaddr_storage));
		fd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
		sbuf_insert(&sbuf, fd);
	}
	printf("%s\n", user_agent_hdr);
	return 0;
}

int open_sfd(char *port) {
	int sfd = socket(AF_INET, SOCK_STREAM, 0);

	int optval = 1;
	setsockopt(sfd, SOL_SOCKET, 15, &optval, sizeof(optval));

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(atoi(port));
	address.sin_addr.s_addr = INADDR_ANY;

	bind(sfd,(struct sockaddr *)&address,sizeof(address));
	listen(sfd, 100);

	return sfd;
}

void *handle_client(void *vargp) {
	pthread_detach(pthread_self());
	while(1) {
		int fd = sbuf_remove(&sbuf);

		char buf[MAX_OBJECT_SIZE];
		memset(buf, 0, MAX_OBJECT_SIZE);
		char method[16], hostname[64], port[8], path[64], headers[1024];
		memset(method, 0, 16);
		memset(hostname, 0, 64);
		memset(port, 0, 8);
		memset(path, 0, 64);
		memset(headers, 0, 1024);

		int bytes_read = 0;
		while(all_headers_received(buf) != 1) {
			bytes_read += recv(fd, buf + bytes_read, 128, 0);
		}

		parse_request(buf, method, hostname, port, path, headers);

		char new_request[MAX_OBJECT_SIZE];

		if (strstr(port, "80\0") == NULL) {
			sprintf(new_request, "%s %s HTTP/1.0\r\nHost: %s:%s\r\n%s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n", method, path, hostname, port, user_agent_hdr);
		} else {
			sprintf(new_request, "%s %s HTTP/1.0\r\nHost: %s\r\n%s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n", method, path, hostname, user_agent_hdr);
		}

		printf("METHOD: %s\n", method);
		printf("HOSTNAME: %s\n", hostname);
		printf("PORT: %s\n", port);
		printf("PATH: %s\n", path);
		printf("HEADERS: %s\n\n", headers);

		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int sfd, s;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0;

		s = getaddrinfo(hostname, port, &hints, &result);
		if (s != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
			exit(EXIT_FAILURE);
		}

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			sfd = socket(rp->ai_family, rp->ai_socktype,
					rp->ai_protocol);
			if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
				break; 
			}
			close(sfd);
		}

		if (rp == NULL) {
			fprintf(stderr, "Could not connect\n");
			exit(EXIT_FAILURE);
		}
		freeaddrinfo(result);

		printf("%s\n", new_request);
		send(sfd, new_request, sizeof(new_request), 0);

		unsigned char response[MAX_OBJECT_SIZE];
		bzero(response, MAX_OBJECT_SIZE);
		bytes_read = 0;
		while(1) {
			int temp = recv(sfd, response + bytes_read, 128, 0);
			if (temp <= 0) {
				break;
			}
			bytes_read += temp;
		}
		close(sfd);
		printf("%s\n", response);

		printf("%d\n", bytes_read);
		send(fd, response, bytes_read, 0);
		close(fd);
	}
	return NULL;
}

int all_headers_received(char *request) {
	if (strstr(request, "\r\n\r\n") != NULL) {
    	return 1;
	}
	return 0;
}

int parse_request(char *request, char *method,
		char *hostname, char *port, char *path, char *headers) {
	printf("%s\n", request);

	char* e = strchr(request, ' ');
	int index = (int)(e - request);
	int endIndex;
	strncpy(method, request, index);

	if((e = strstr(request, "Host: ")) != NULL) {
		memset(hostname, '\0', strlen(hostname));
		int indexOfHostStart = (int) (e - request) + 6;
		memset(port, '\0', strlen(port));
		char * colonCase = strstr(request + indexOfHostStart, ":");
		int idxOfColon = (int) ((colonCase + 1) - request);
		e = strstr(request + indexOfHostStart, "\r\n");
		int endIndex = (int) (e - request);
		if ( (colonCase == NULL) || (idxOfColon > endIndex)) {
			strncpy(hostname, request + indexOfHostStart, endIndex - indexOfHostStart);
			hostname[endIndex - indexOfHostStart] = '\0';
			memcpy(port, "80", 3);
		}
		else {
			strncpy(hostname, request + indexOfHostStart, idxOfColon - indexOfHostStart - 1);
			hostname[idxOfColon - indexOfHostStart] = '\0';
			strncpy(port, request + idxOfColon, endIndex - idxOfColon);
			port[endIndex - idxOfColon] = '\0';
		}
	}

	e = strchr(request + 15, '/');
	index = (int)(e - request);
	e = strchr(request + index, ' ');
	endIndex = (int)(e - request);
	strncpy(path, request + index, endIndex - index);
	memset(path + endIndex, '\0', 1);

	if ((e = strchr(request, '\n')) != NULL) {
		index = (int)(e - request);
		if ((e = strstr(request + index, "\r\n\r\n")) != NULL) {
			endIndex = (int)(e - request);
			strncpy(headers, request + index + 1, endIndex - index - 1);
			memset(headers + endIndex - index - 1, '\0', 1);
		} else {
			return 0;
		}
	} else {
		return 0;
	}

	return 1;
}

void test_parser() {
	int i;
	char method[16], hostname[64], port[8], path[64], headers[1024];

       	char *reqs[] = {
		"GET http://www.example.com/index.html HTTP/1.0\r\n"
		"Host: www.example.com\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html?foo=1&bar=2 HTTP/1.0\r\n"
		"Host: www.example.com:8080\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://localhost:1234/home.html HTTP/1.0\r\n"
		"Host: localhost:1234\r\n"
		"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:68.0) Gecko/20100101 Firefox/68.0\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n\r\n",

		"GET http://www.example.com:8080/index.html HTTP/1.0\r\n",

		NULL
	};
	
	for (i = 0; reqs[i] != NULL; i++) {
		printf("Testing %s\n", reqs[i]);
		if (parse_request(reqs[i], method, hostname, port, path, headers)) {
			printf("METHOD: %s\n", method);
			printf("HOSTNAME: %s\n", hostname);
			printf("PORT: %s\n", port);
			printf("HEADERS: %s\n", headers);
		} else {
			printf("REQUEST INCOMPLETE\n");
		}
	}
}

void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}

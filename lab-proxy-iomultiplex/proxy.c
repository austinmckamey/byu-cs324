#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <signal.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct request_info {
	int clientfd;
	int serverfd;
	int state;
	char buffer[MAX_OBJECT_SIZE];
	int client_read;
	int server_to_write;
	int server_written;
	int server_read;
	int client_written;
} request_info;

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:97.0) Gecko/20100101 Firefox/97.0";

int open_sfd(char *);
void handle_new_client(int, int, int, struct epoll_event *);
void *handle_client(int, struct request_info *);
void *read_request(int, struct request_info *);
void *send_request(int, struct request_info *);
void *read_response(int, struct request_info *);
void *send_response(int, struct request_info *);
int all_headers_received(char *);
int parse_request(char *, char *, char *, char *, char *, char *);
void test_parser();
void print_bytes(unsigned char *, int);

int interrupted = 0;

void sigint_handler(int sig) {
	interrupted = 1;
}

int main(int argc, char *argv[])
{
	dup2(1, 2);
	int efd = epoll_create1(0);
	int sfd = open_sfd(argv[1]);

	struct request_info *listener;
	listener = malloc(sizeof(struct request_info));
	listener->clientfd = sfd;
	listener->state = 0;
	listener->client_read = 0;
	listener->server_to_write = 0;
	listener->server_written = 0;
	listener->server_read = 0;
	listener->client_written = 0;

	struct epoll_event event;
	event.events = EPOLLIN | EPOLLET;
	event.data.ptr = listener;

	epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);

	struct epoll_event *events = calloc(64, sizeof(struct epoll_event));

	while(1) {
		int results = epoll_wait(efd, events, 64, 1000);
		if (results == 0) {
			if (interrupted) {
				break;
			}
			continue;
		} else if (results < 0) {
			printf("Error in results\n");
			break;
		} else {
			handle_new_client(efd, sfd, results, events);
		}
	}
	free(listener);
	free(events);

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

	fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);

	bind(sfd,(struct sockaddr *)&address,sizeof(address));
	listen(sfd, 100);

	return sfd;
}

void handle_new_client(int efd, int listener, int results, struct epoll_event *events) {
	int acc;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	peer_addr_len = sizeof(struct sockaddr_storage);

	for (int i = 0; i < results; i++) {
		struct request_info *client_req = (struct request_info *)events[i].data.ptr;
		if (client_req->clientfd == listener) {
			while(1) {
				acc = accept(listener, (struct sockaddr *)&peer_addr, &peer_addr_len);
				if (acc < 0) {
					if (errno != EAGAIN || errno != EWOULDBLOCK) {
						printf("Error in accepting\n");
						perror("ERROR: ");
						exit(0);
					}
					return;
				}

				fcntl(acc, F_SETFL, fcntl(acc, F_GETFL, 0) | O_NONBLOCK);

				struct request_info *client_req = malloc(sizeof(struct request_info));
				client_req->clientfd = acc;
				client_req->state = 1;
				client_req->client_read = 0;
				client_req->server_to_write = 0;
				client_req->server_written = 0;
				client_req->server_read = 0;
				client_req->client_written = 0;

				struct epoll_event event;
				event.events = EPOLLIN | EPOLLET;
				event.data.ptr = client_req;
				epoll_ctl(efd, EPOLL_CTL_ADD, acc, &event);
				//printf("%d\n", acc);
			}
		} else {
			handle_client(efd, client_req);
		}
	}
}

void *handle_client(int efd, struct request_info *client_request) {
	if (client_request->state == 1) {
		read_request(efd, client_request);
	} else if (client_request->state == 2) {
		send_request(efd, client_request);
	} else if (client_request->state == 3) {
		read_response(efd, client_request);
	} else if (client_request->state == 4) {
		send_response(efd, client_request);
	}
	return NULL;
}

void *read_request(int efd, struct request_info *client_req) {
	int cfd = client_req->clientfd;
	char buf[MAX_OBJECT_SIZE];
	bzero(buf, MAX_OBJECT_SIZE);
	strcpy(buf, client_req->buffer);
	int total_read = client_req->client_read;
	while(1) {
		int bytes_read;
		bytes_read = recv(cfd, buf + total_read, 128, 0);
		strcpy(client_req->buffer, buf);
		if (all_headers_received(buf) == 1) {
			total_read += bytes_read;

			char method[16], hostname[64], port[8], path[64], headers[1024];
			memset(method, 0, 16);
			memset(hostname, 0, 64);
			memset(port, 0, 8);
			memset(path, 0, 64);
			memset(headers, 0, 1024);

			printf("CLIENT REQUEST: %s\n", buf);

			parse_request(buf, method, hostname, port, path, headers);

			char new_request[MAX_OBJECT_SIZE];

			if (strstr(port, "80\0") == NULL) {
				sprintf(new_request, "%s %s HTTP/1.0\r\nHost: %s:%s\r\n%s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n", method, path, hostname, port, user_agent_hdr);
			} else {
				sprintf(new_request, "%s %s HTTP/1.0\r\nHost: %s\r\n%s\r\nConnection: close\r\nProxy-Connection: close\r\n\r\n", method, path, hostname, user_agent_hdr);
			}

			printf("SERVER REQUEST: %s\n", new_request);

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

			fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);

			memset(client_req->buffer, 0, MAX_OBJECT_SIZE);

			client_req->serverfd = sfd;
			client_req->state = 2;
			strcpy(client_req->buffer, new_request);
			client_req->client_read = total_read;
			client_req->server_to_write = strlen(new_request);
			client_req->server_written = 0;
			client_req->server_read = 0;
			client_req->client_written = 0;

			struct epoll_event event;
			event.events = EPOLLOUT | EPOLLET;
			event.data.ptr = client_req;

			epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);

			break;
		} else if (bytes_read < 0) {
			if (errno != EAGAIN || errno != EWOULDBLOCK) {
				printf("Error in reading request\n");
				perror("ERROR: ");
				exit(0);
			}
			break;
		}
		total_read += bytes_read;
		client_req->client_read = total_read;
	}
	return NULL;
}

void *send_request(int efd, struct request_info *client_req) {
	char request[MAX_OBJECT_SIZE];
	bzero(request, MAX_OBJECT_SIZE);
	strcpy(request, client_req->buffer);
	int sfd = client_req->serverfd;
	int bytes_sent = send(sfd, request, client_req->server_to_write, 0);
	if (bytes_sent >= 0) {
		client_req->server_written = bytes_sent;
		client_req->state = 3;

		struct epoll_event event;
		event.events = EPOLLIN | EPOLLET;
		event.data.ptr = client_req;

		epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
	} else {
		if (errno != EAGAIN || errno != EWOULDBLOCK) {
			printf("Error in sending request\n");
			exit(0);
		}
	}
	return NULL;
}

void *read_response(int efd, struct request_info *client_req) {
	int sfd = client_req->serverfd;
	char buf[MAX_OBJECT_SIZE];
	int total_read = client_req->server_read;
	while(1) {
		int bytes_read;
		bytes_read = recv(sfd, buf + total_read, 128, 0);
		if (bytes_read == 0) {
			int cfd = client_req->clientfd;

			client_req->state = 4;
			memcpy(client_req->buffer, buf, total_read);
			client_req->server_read = total_read;

			struct epoll_event event;
			event.events = EPOLLOUT | EPOLLET;
			event.data.ptr = client_req;

			epoll_ctl(efd, EPOLL_CTL_MOD, cfd, &event);

			printf("Total: %d\n", total_read);
			printf("RESPONSE: %s\n", buf);

			break;
		} else if (bytes_read < 0) {
			if (errno != EAGAIN || errno != EWOULDBLOCK) {
				printf("Error in reading response\n");
				perror("ERROR: ");
				exit(0);
			}
			break;
		}
		total_read += bytes_read;
		client_req->server_read = total_read;
	}
	return NULL;
}

void *send_response(int efd, struct request_info *client_req) {
	char response[MAX_OBJECT_SIZE];
	bzero(response, MAX_OBJECT_SIZE);
	memcpy(response, client_req->buffer, client_req->server_read);
	int cfd = client_req->clientfd;
	int bytes_sent = send(cfd, response, client_req->server_read, 0);
	if (bytes_sent >= 0) {
		
		close(cfd);
		free(client_req);

	} else {
		if (errno != EAGAIN || errno != EWOULDBLOCK) {
			printf("Error in sending response\n");
			perror("ERROR: ");
			exit(0);
		}
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

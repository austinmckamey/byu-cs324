// Replace PUT_USERID_HERE with your actual BYU CS user id, which you can find
// by running `id -u` on a CS lab machine.
#define USERID 1823700432

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int verbose = 0;

void print_bytes(unsigned char *bytes, int byteslen);

int main(int argc, char *argv[]) {
	char * server = argv[1];
	char * port_string = argv[2];
	int port_local;
	int port = htons(atoi(argv[2]));
	int level = htons(atoi(argv[3]));
	unsigned short seed = htons(atoi(argv[4]));

	unsigned int user = htonl(USERID);

	char buf[8];
	bzero(buf, 8);
	memcpy(&buf[0], &level, 2);
	memcpy(&buf[2], &user, 4);
	memcpy(&buf[6], &seed, 2);

	//print_bytes(&buf, 8);

	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s, j;
	size_t len;
	ssize_t nread;
	int hostindex;
	int af;

	if (argc < 3 ||
		((strcmp(argv[1], "-4") == 0 || strcmp(argv[1], "-6") == 0) &&
			argc < 4)) {
		fprintf(stderr, "Usage: %s [ -4 | -6 ] host port msg...\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Use only IPv4 (AF_INET) if -4 is specified;
	 * Use only IPv6 (AF_INET6) if -6 is specified;
	 * Try both if neither is specified. */
	af = AF_INET;
	if (strcmp(argv[1], "-4") == 0 ||
			strcmp(argv[1], "-6") == 0) {
		if (strcmp(argv[1], "-6") == 0) {
			af = AF_INET6;
		} else { // (strcmp(argv[1], "-4") == 0) {
			af = AF_INET;
		}
		hostindex = 2;
	} else {
		hostindex = 1;
	}

	/* Obtain address(es) matching host/port */

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = af;    /* Allow IPv4, IPv6, or both, depending on
				    what was specified on the command line. */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;  /* Any protocol */

	/* SECTION A - pre-socket setup; getaddrinfo() */

	s = getaddrinfo(argv[hostindex], argv[hostindex + 1], &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */

	/* SECTION B - pre-socket setup; getaddrinfo() */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd != -1) {
			break;
		}

		close(sfd);
	}

	if (rp == NULL) {   /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in ipv4addr_remote;
	struct sockaddr_in6 ipv6addr_remote;
	struct sockaddr_in ipv4addr_local;
	struct sockaddr_in6 ipv6addr_local;
	int bytes_received;
	char buf_read[256];

	af = rp->ai_family;
	if (af == AF_INET) {
		ipv4addr_remote = *(struct sockaddr_in *)rp->ai_addr;
		ipv4addr_remote.sin_port = port;
		socklen_t remote_addr_len = sizeof(struct sockaddr_in);
		socklen_t addrlen = sizeof(struct sockaddr_in);
		port_local = getsockname(sfd, (struct sockaddr *)&ipv4addr_local, &addrlen);
		sendto(sfd, &buf, 8, 0,(struct sockaddr *) &ipv4addr_remote, remote_addr_len);
		bytes_received = recvfrom(sfd, buf_read, 256, 0, NULL, NULL);
	} else {
		ipv6addr_remote = *(struct sockaddr_in6 *)rp->ai_addr;
		ipv6addr_remote.sin6_port = port;
		socklen_t remote_addr_len = sizeof(struct sockaddr_in6);
		socklen_t addrlen = sizeof(struct sockaddr_in6);
		port_local = getsockname(sfd, (struct sockaddr *)&ipv6addr_local, &addrlen);
		sendto(sfd, &buf, 8, 0, (struct sockaddr *) &ipv6addr_remote, remote_addr_len);
		bytes_received = recvfrom(sfd, buf_read, 256, 0, NULL, NULL);
	}

	//print_bytes(buf_read, bytes_received);

	char final[1024];
	int total_received = 0;
	bzero(final, 1);

	int count = 1;

	while(1) {
		int chunk_length;

		memcpy(&chunk_length, &buf_read[0], 1);

		if(chunk_length == 0) {
			break;
		}

		char chunk[chunk_length];
		int op_code;
		unsigned short op_param;
		unsigned int nonce;

		memcpy(&chunk, &buf_read[1], chunk_length);
		memcpy(&op_code, &buf_read[chunk_length + 1], 1);
		memcpy(&op_param, &buf_read[chunk_length + 2], 2);
		memcpy(&nonce, &buf_read[chunk_length + 4], 4);

		strncat(final, chunk, chunk_length);
		total_received += chunk_length;

		if (op_code == 1) {
			port = op_param;
		} else if (op_code == 2) {
			port_local = op_param;
			close(sfd);
			for (rp = result; rp != NULL; rp = rp->ai_next) {
				sfd = socket(rp->ai_family, rp->ai_socktype,
						rp->ai_protocol);
				if (sfd != -1) {
					break;
				}

				close(sfd);
			}
			if (af == AF_INET) {
				ipv4addr_local.sin_family = AF_INET;
				ipv4addr_local.sin_port = port_local;
				ipv4addr_local.sin_addr.s_addr = 0;
				if (bind(sfd, (struct sockaddr *)&ipv4addr_local, sizeof(struct sockaddr_in)) < 0) {
					perror("bind()");
				}
			} else {
				ipv6addr_local.sin6_family = AF_INET6;
				ipv6addr_local.sin6_port = port_local;
				bzero(ipv6addr_local.sin6_addr.s6_addr, 16);
				if (bind(sfd, (struct sockaddr *)&ipv6addr_local, sizeof(struct sockaddr_in6)) < 0) {
					perror("bind()");
				}
			}
		} else if (op_code == 3) {
			unsigned int nonce_total = 0;
			if (af == AF_INET) {
				for(int i = 0; i < ntohs(op_param); i++) {
					struct sockaddr_in ipv4addr_new;
					int remote_addr_len = sizeof(struct sockaddr_in);
					recvfrom(sfd, buf, 8, 0, (struct sockaddr *)&ipv4addr_new, (socklen_t *)&remote_addr_len);
					nonce_total += ntohs(ipv4addr_new.sin_port);
				}
			} else {
				for(int i = 0; i < ntohs(op_param); i++) {
					struct sockaddr_in6 ipv6addr_new;
					int remote_addr_len = sizeof(struct sockaddr_in6);
					recvfrom(sfd, buf, 8, 0, (struct sockaddr *)&ipv6addr_remote, (socklen_t *)&remote_addr_len);
					nonce_total += ntohs(ipv6addr_remote.sin6_port);
				}
			}
			nonce = htonl(nonce_total);
		} else if (op_code == 4) {
			port = op_param;
			if(af == AF_INET) {
				hints.ai_family = AF_INET6;
				af = AF_INET6;
			} else {
				hints.ai_family = AF_INET;
				af = AF_INET;
			}
			s = getaddrinfo(argv[hostindex], argv[hostindex + 1], &hints, &result);
			close(sfd);
			for (rp = result; rp != NULL; rp = rp->ai_next) {
				sfd = socket(rp->ai_family, rp->ai_socktype,
						rp->ai_protocol);
				if (sfd != -1) {
					break;
				}

				close(sfd);
			}
		}

		nonce = ntohl(nonce);
		nonce++;
		nonce = htonl(nonce);

		char new_buf[4];
		bzero(new_buf, 4);
		memcpy(&new_buf[0], &nonce, 4);

		//print_bytes(new_buf, 4);

		af = rp->ai_family;
		if (af == AF_INET) {
			ipv4addr_remote = *(struct sockaddr_in *)rp->ai_addr;
			ipv4addr_remote.sin_port = port;
			socklen_t remote_addr_len = sizeof(struct sockaddr_in);
			socklen_t addrlen = sizeof(struct sockaddr_in);
			getsockname(sfd, (struct sockaddr *)&ipv4addr_local, &addrlen);
			sendto(sfd, &new_buf, 4, 0,(struct sockaddr *) &ipv4addr_remote, remote_addr_len);
			bytes_received = recvfrom(sfd, buf_read, 256, 0, NULL, NULL);
		} else {
			ipv6addr_remote = *(struct sockaddr_in6 *)rp->ai_addr;
			ipv6addr_remote.sin6_port = port;
			socklen_t remote_addr_len = sizeof(struct sockaddr_in6);
			socklen_t addrlen = sizeof(struct sockaddr_in6);
			getsockname(sfd, (struct sockaddr *)&ipv6addr_local, &addrlen);
			sendto(sfd, &new_buf, 4, 0, (struct sockaddr *) &ipv6addr_remote, remote_addr_len);
			bytes_received = recvfrom(sfd, buf_read, 256, 0, NULL, NULL);
		}	

		//print_bytes(buf_read, bytes_received);

		//print_bytes(final, total_received);

		if (count == 4) {
			break;
		}
		//count++;
	}

	printf("%s\n", final);
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

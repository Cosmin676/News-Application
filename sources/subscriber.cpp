#include <iostream>
#include <bits/stdc++.h>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	int flag = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
	DIE(ret < 0, "nagle_not_solved");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// send lungimea id-ului
	int len = strlen(argv[1]);
	n = send(sockfd, &len, sizeof(int), 0);
	DIE(n < 0, "send lungime");

	// send id-ul
	n = send(sockfd, argv[1], len, 0);
	DIE(n < 0, "send");

	fd_set reads_client;
	FD_ZERO(&reads_client);
	FD_SET(sockfd,&reads_client);
	FD_SET(STDIN_FILENO,&reads_client);
	fd_set tmp_fds;
	int fdmax = sockfd;
	int i;
	int trueConst = 1;
	while (trueConst) {
		tmp_fds = reads_client;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");	
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// primesc dimensiune mesaj de la server
					int len;
					n = recv(i, &len, sizeof(int), 0);
					DIE(n < 0, "recv data size");

					// primesc un mesaj de la server
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, len, 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						printf("Serverul a inchis conexiunea\n");
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &reads_client);
					} else {
						if(strcmp(buffer, "exit") == 0) {
							trueConst = 0;
							close(i);
							break;
						} else {
							// se afiseaza payload-ul primit
							printf ("%s\n", buffer);
						}
					}
				} else {
					// s-au primit date de la stdin
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);
					buffer[strcspn(buffer, "\n")] = 0;

					if (strncmp(buffer, "exit", 4) == 0) {
						trueConst = 0;
						break;
					}

					// se trimite dimensiunea mesajul citit
					// de la tastatura catre server

					int x = strlen(buffer);
					n = send(sockfd, &x, sizeof(int), 0);
					DIE(n < 0, "send lungime fgets()");

					// se trimite mesaj la server
					n = send(sockfd, buffer, x, 0);
					if(n < 0)
						DIE(n < 0, "send");
					else {
						if(strncmp(buffer, "subscribe", 9) == 0)
							printf("Subscribed to topic.\n");
						if(strncmp(buffer, "unsubscribe", 11) == 0)
							printf("Unsubscribed from topic.\n");
					}
					memset(buffer, 0, BUFLEN);

				}
			}
		}
	}

	close(sockfd);

	return 0;
}

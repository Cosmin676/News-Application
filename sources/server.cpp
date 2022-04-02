#include <iostream>
#include <bits/stdc++.h>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "helpers.h"

struct client {
	int socketFD;

	client(int sockID) {
		socketFD = sockID;
	}
};

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

// am folosit functia aceasta pentru debug
void print_map(map<string, vector<client>> &map) {
	if(map.size() > 0) {
		for(auto it = map.begin(); it != map.end(); it++) {
			printf("<%s>", (*it).first.c_str());
			if((*it).second.size() > 0) {
				for(auto ct = (*it).second.begin(); ct != (*it).second.end(); ct++) {
					printf("{%d}; ", (*ct).socketFD);
				}
			} else {
				printf("{ NULL }");
			}
			printf("\n");
		} 
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	int tcp_sockfd, new_tcp, portno, udp_socket, new_udp;
	char buffer[BUFLEN];
	int size;
	struct sockaddr_in serv_addr, cli_addr;
	int n, i;
	int tcp_ret, udp_ret;
	socklen_t clilen;
	vector<pair<int, string>> ids_connected;
	map<string, vector<client>> topics;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_sockfd < 0, "socket");

	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_socket < 0, "UDP SOCKET");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	int flag = 1;
	tcp_ret = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)& flag, sizeof(int));
	DIE(tcp_ret < 0, "nagle_not_resolved");

	tcp_ret = bind(tcp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(tcp_ret < 0, "bind tcp");

	udp_ret = bind(udp_socket, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(udp_ret < 0, "bind udp");

	tcp_ret = listen(tcp_sockfd, MAX_CLIENTS);
	DIE(tcp_ret < 0, "tcp listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(tcp_sockfd, &read_fds);
	FD_SET(udp_socket, &read_fds);
	if(tcp_sockfd > udp_socket)
		fdmax = tcp_sockfd;
	else
		fdmax = udp_socket;

	while (1) {
		tmp_fds = read_fds; 
		tcp_ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(tcp_ret < 0, "select");
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				
				if (i == tcp_sockfd) {
					// a venit o cerere de conexiune pe socketul TCP inactiv
					// sau a venit un mesaj de la client privind ID-ul cu care
					// acesta va fii logat

					clilen = sizeof(cli_addr);
					new_tcp = accept(tcp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(new_tcp < 0, "accept");					

					// receive lungimea id-ului
					int len;
					n = recv(new_tcp, &len, sizeof(int), 0);
					DIE(n < 0, "lungime");

					// receive id-ul de dimensiunea primita mai sus
					memset(buffer, 0, BUFLEN);
					n = recv(new_tcp, buffer, len, 0);
					DIE(n < 0, "recv");

					if(find_if(ids_connected.begin(), ids_connected.end(),
						[&buffer](const pair<int, string> &p)
						{return p.second == buffer;}) == ids_connected.end()) {

						ids_connected.push_back(make_pair(new_tcp, buffer));
						FD_SET(new_tcp, &read_fds);
						if (new_tcp > fdmax) { 
							fdmax = new_tcp;
						}
						printf("New client %s connected from %s:%d.\n", buffer,
						inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));


					} else {
						int x = strlen("exit");
						n = send(new_tcp, &x, sizeof(int), 0);
						DIE(n < 0, "send exit size");

						n = send(new_tcp, "exit", x, 0);
						DIE(n < 0, "send exit");
						
						printf("Client %s already connected.\n", buffer);
					}
				} else if(i == udp_socket) {

					char payload[1610] = "";
					clilen = sizeof(cli_addr);
					new_udp = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen);
					DIE(new_udp < 0, "RECV_UDP");

					if(new_udp == 0) {
						// udp inchide conexiunea.
						// daca se primeste 0
						// in checker nu cred ca se primeste 0
						// asa ca l-am inchis la final.
					} else {
						char topic[60] = "";
						char type_string[15];
						char auxiliar[1501];
						char aux[1501];

						// am parsat mesajul de UDP
						// in functie de type-ul mesajului
						// fac urmatorele parsari

						uint8_t type = (uint8_t)buffer[50];

						if(type == 0) {
							sprintf(type_string, "INT");
							uint8_t sign = (uint8_t)buffer[51];
							uint32_t *aux = (uint32_t*)(&buffer[52]);
							int data;
							if(sign == 0)
								data = ntohl(*aux);
							else {
								data = ntohl(*aux);
								data = 0 - data;
							}
							sprintf(auxiliar, "%d", data);
						}
						if(type == 1) {
							sprintf(type_string, "SHORT_REAL");
							uint16_t *aux = (uint16_t*)(&buffer[51]);
							uint16_t aux1 = ntohs(*aux);
							double data = (double)aux1 / 100;
							sprintf(auxiliar, "%.2lf", data);
						} 
						if(type == 2) {	
							sprintf(type_string, "FLOAT");
							uint8_t sign = (uint8_t)buffer[51];
							uint32_t *aux = (uint32_t*)(&buffer[52]);
							uint8_t *p2 = (uint8_t*)(&buffer[52] + sizeof(uint32_t));
							int putere = 0 - *p2;
							double data = ntohl(*aux) * pow(10, putere);
							if(sign == 1)
								data = 0 - data;
							sprintf(auxiliar, "%.*lf", *p2, data);
						}
						if(type == 3) {
							sprintf(type_string, "STRING");
							sprintf(auxiliar, "%s", &buffer[51]);
						}

						// alcatuiesc in payload mesajul ce trebuie trimis
						strcat(topic, buffer);
						strcat(payload, inet_ntoa(cli_addr.sin_addr));
						strcat(payload, ":");
						sprintf(aux, "%d",ntohs(cli_addr.sin_port));
						strcat(payload, aux);
						strcat(payload, " - ");
						strcat(payload, topic);
						strcat(payload, " - ");
						strcat(payload, type_string);
						strcat(payload, " - ");
						sprintf(aux, "%s", auxiliar);
						strcat(payload, aux);

						if(topics.find(string(topic)) != topics.end()) {
							// exita deja topic-ul
							// trebuie parcurs map si trimis fiecarui user
							// mesajul primit de la UDP
							int er;
							auto it = topics.find(string(topic));
							for(auto ptr = (*it).second.begin(); ptr != (*it).second.end(); ptr++) {
								(*ptr).socketFD;
								int x = strlen(payload);

								er = send((*ptr).socketFD, &x, sizeof(int), 0);
								DIE(er < 0, "send data size to client");

								er = send((*ptr).socketFD, payload, x, 0);
								DIE(er < 0, "send data to client");
							}
							
						} else {
							// daca topicul nu exista deja il creez
							topics.insert(pair<string, vector<client>>(string(topic), vector<client>()));
						}						
					}
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze

					// se primeste intai dimensiunea stringului
					int x;
					n = recv(i, &x, sizeof(int), 0);
					DIE(n < 0, "lungime");

					// acum se primeste mesajul de la client
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, x, 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						auto it = find_if(ids_connected.begin(), ids_connected.end(),
						[&i](const pair<int, string> &p)
						{return p.first == i;});
						cout << "Client " << (*it).second << " disconnected." << endl;
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);

						// scot din ids_connected, clientul ce tocmai a iesit
						ids_connected.erase(it);

						// se scoate din map clientul ce tocmai a iesit
						for(auto x = topics.begin(); x != topics.end(); x++) {
							if((*x).second.size() > 0) {
								auto ptr = find_if((*x).second.begin(), (*x).second.end(),
									[&i](const client &v){return v.socketFD == i;});
								if(ptr != (*x).second.end())
									(*x).second.erase(ptr);
							}
						}

					} else {
						// au venit date de la clienti/subscriberi
						if(strncmp(buffer, "subscribe", 9) == 0) {
							char *token = strtok(buffer, " ");
							char *type, *topic;
							int sf;
							if(token != NULL) {
								type = token; // subscribe;
								topic = strtok(NULL, " ");
								char *aux = strtok(NULL, " ");
								sf = atoi(aux);
							}
							auto it = topics.find(string(topic));

							if(it != topics.end()) {
								// dc topicul exista deja
								// adaug noul user, doar dc el nu este abonat deja
								
								if (find_if((*it).second.begin(), (*it).second.end(),
									[&i](const client &v){return v.socketFD == i;}) == (*it).second.end())
									topics[string(topic)].push_back(i);
							} else {
								// se adauga topic nou
								topics.insert(pair<string, vector<client>>(string(topic), vector<client>()));
								topics[string(topic)].push_back(i);
							}
						} else if(strncmp(buffer, "unsubscribe", 11) == 0) {
							// se primest unsubscribe

							char *tok = strtok(buffer, " ");
							if(tok != NULL) {
								tok = strtok(NULL, " ");
							}
							tok[strcspn(tok, "\n")] = 0;

							auto it = topics.find(string(tok));
							if(it != topics.end()) {
								auto ptr = find_if((*it).second.begin(), (*it).second.end(),
									[&i](const client &v){return v.socketFD == i;});
								(*it).second.erase(ptr);
							}
						}
					}
				}
			}
		}
	}

	close(tcp_sockfd);
	close(udp_socket);

	return 0;
}

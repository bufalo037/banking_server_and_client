#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#define MaxBuff 100	
#define FREE1 free(server);free(udp_sock);free(mesaj);free(nume_fisier);free(buff);
#define FREE2 free(mesaj2);free(read_set);free(read_set_aux);free(from);free(last_attempt);
#define FREE FREE1 FREE2

// unsigned int to ascii
char *uitoa(unsigned int x)
{
	//presupun ca x nu va fi nicidodata 0 ca nu are sens ca un pid sa fie 0
	unsigned int copie = x;
	unsigned int nr = 0;
	char *string;
	while(copie != 0)
	{
		copie = copie/10;
		nr++;
	}
	string = (char *)malloc(sizeof(char) * (nr + 1));

	for(unsigned int i = 0; i < nr; i++)
	{
		string[i] = ((x/((int)pow(10.0, (double)(nr - i - 1)))) % 10) + '0';
	}
	string[nr] = '\0';
	return string;
}



void init_structaddr(unsigned short port, struct sockaddr_in* server)
{
	server->sin_family = AF_INET;
	server->sin_port = htons(port);
	server->sin_addr.s_addr = htonl(INADDR_ANY);
}


void init_server(unsigned short port , char *addr, struct sockaddr_in* server)
{
	server->sin_family = AF_INET;
	server->sin_port = htons(port);
	if(inet_aton(addr, &(server->sin_addr)) == 0)
	{
		perror("Adresa IP invaida");
		exit(EXIT_FAILURE);
	}
}


int main(int argc, char** argv)
{

	if(argc != 3)
	{
		perror("Clientul primeste 2 argumente <IP_server> <port_server>\n");
		return -1;
	}

	struct sockaddr_in *server;
	struct sockaddr_in *udp_sock;
	struct sockaddr_in *from;
	fd_set *read_set;
	fd_set *read_set_aux;
	unsigned short port;
	unsigned int len;
	int connected = 0;
	int udp_fd;
	int tcp_fd;
	int pid;
	char *last_attempt;
	char *mesaj;
	char *mesaj2;
	char *nume_fisier;
	char *buff;
	int select_ret_val;
	int ret_val;
	FILE *log;

	//=========================================================================

	srand(time(NULL));
	server = malloc(sizeof(struct sockaddr_in));
	udp_sock = malloc(sizeof(struct sockaddr_in));
	mesaj = malloc(sizeof(char) * MaxBuff);
	nume_fisier = malloc(sizeof(char) * 22);
	buff = malloc(sizeof(char) * MaxBuff);
	last_attempt = malloc(sizeof(char) * 7);
	mesaj2 = malloc(sizeof(char) * MaxBuff);
	read_set = malloc(sizeof(fd_set));
	read_set_aux = malloc(sizeof(fd_set));
	from = malloc(sizeof(struct sockaddr_in));

	udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	tcp_fd = socket(AF_INET, SOCK_STREAM, 0);

	if(udp_fd < 0 || tcp_fd < 0)
	{
		perror("Eroare deschidere socketi");
		return -1;
	}

	// Din cauza ca nu am un port specificat pentru clint aleg efectiv unul random
	do
	{
		port = ((rand() % (653535 - 1025)) + 1025);
		init_structaddr(port, udp_sock);
	}
	while(bind(udp_fd, (struct sockaddr *)udp_sock, sizeof(struct sockaddr_in)) == -1);
	
	init_server((unsigned short)atoi(argv[2]), argv[1], server);
	if(connect(tcp_fd, (struct sockaddr *)server, sizeof(struct sockaddr_in)) == -1)
	{
		perror("Eroare connect");
		return -1;
	}

	FD_ZERO(read_set);
	FD_SET(0, read_set);
	FD_SET(tcp_fd, read_set);

	pid = getpid();
	snprintf(nume_fisier, 22, "client-%s.log", uitoa(pid));
	log = fopen(nume_fisier, "wt");

	//=========================================================================
	
	while(1)
	{
		memcpy(read_set_aux, read_set, sizeof(fd_set));
		select_ret_val = select(tcp_fd + 1, read_set_aux, NULL, NULL, NULL);

		if(select_ret_val == -1)
		{
			perror("Eroare primul select");
		}

		if(FD_ISSET(tcp_fd, read_set_aux))
		{
			ret_val = recv(tcp_fd, buff, MaxBuff, 0);
			if(ret_val == -1)
			{
				perror("Eroare primire mesaj server");
			}
			else
			{
				if(strcmp(buff,"quit\n") == 0)
				{
					fprintf(log, "Ne cerem scuze dar serverul intra in mentenanta\n");
					printf("Ne cerem scuze dar serverul intra in mentenanta\n");
					FREE
					fclose(log);
					close(udp_fd);
					close(tcp_fd);
					return 0;
				}
			}
		}

		if(FD_ISSET(0, read_set_aux))
		{
			fgets(mesaj, MaxBuff, stdin);
			if(strcmp(mesaj, "unlock\n") == 0)
			{	
				strcpy(mesaj2, mesaj);
				mesaj2[6] = ' ';
				memcpy(mesaj2 + 7, last_attempt, 6);
				mesaj2[13] = '\0';

				if(sendto(udp_fd, mesaj2, 14, 0, (struct sockaddr *)server,
				 sizeof(struct sockaddr_in)) == -1)
				{
					perror("Eroare send prin UDP");
				}

				len = sizeof(struct sockaddr_in);
				if(recvfrom(udp_fd, buff, MaxBuff, 0, (struct sockaddr *)from, &len) == -1)
				{
					perror("Eroare primire mesaj server prin UDP");
				}
				else
				{
					if(strcmp(buff, "UNLOCK> Trimite parola secreta") == 0)
					{	
						printf("%s\n\n",buff);
						fprintf(log, "%s%s\n", mesaj, buff);
						fflush(log);

						fgets(mesaj, 10, stdin);
						memcpy(mesaj2 + 7, mesaj, strlen(mesaj) - 1);
						memcpy(mesaj2, last_attempt, 6);
						mesaj2[6] = ' ';
						mesaj2[6 + strlen(mesaj)] = '\0';
						sendto(udp_fd, mesaj2, strlen(mesaj2) + 1, 0,
						 (struct sockaddr *)server, sizeof(struct sockaddr_in));

						if(recvfrom(udp_fd, buff, MaxBuff, 0, (struct sockaddr *)from, &len) == -1)
						{
							perror("Eroare primire mesaj server prin UDP");
						}
					}
				}
			}
			else
			{
				if(strncmp(mesaj, "login", 5) == 0 && (mesaj[5] == ' ' || mesaj[5] == '\n'))
				{
					//pentru a nu primi segmentasion fault
					if(strspn(mesaj + 6,"12345678890") == 6 && (mesaj[12] == ' '
					 || mesaj[12] == '\n'))
					{
						memcpy(last_attempt, mesaj + 6, 6);
						last_attempt[6] = '\0';
					}
					else
					{
						// un nr de card este valid doar daca este format din 6 cifre
						// prin urmare urmatorul lucru e incorect mereu
						memcpy(last_attempt, "aaaaaa",6);
						last_attempt[6] = '\0';
					}

					if(connected == 0)
					{
						if((ret_val = send(tcp_fd, mesaj, strlen(mesaj) + 1, 0)) == -1)
						{
							perror("Eroare trasmitere mesaj");
						}
						ret_val = recv(tcp_fd, buff, MaxBuff, 0);
						if(ret_val == -1)
						{
							perror("Eroare primire mesaj server");
						}
						else
						{
							if(strstr(buff,"Welcome"))
							{
								connected = 1;
							}
						}
					}
					else
					{
						sprintf(buff,"IBANK> -2 : Sesiune deja deschisa");
					}
				}
				else
				{
					if(strcmp(mesaj, "quit\n") == 0)
					{
						if((ret_val = send(tcp_fd, mesaj, strlen(mesaj) + 1, 0)) == -1)
						{
							perror("Eroare trasmitere mesaj");
						}
						FREE
						fprintf(log, "quit\n");
						fflush(log);
						fclose(log);
						close(udp_fd);
						close(tcp_fd);
						return 0;	
					}

					if(connected == 0)
					{
						sprintf(buff,"-1 : Clientul nu este autentificat");
					}
					else
					{
						if((ret_val = send(tcp_fd, mesaj, strlen(mesaj) + 1, 0)) == -1)
						{
							perror("Eroare trasmitere mesaj");
						}
						ret_val = recv(tcp_fd, buff, MaxBuff, 0);
						if(ret_val == -1)
						{
							perror("Eroare primire mesaj server");
						}

						if(strcmp(buff, "IBANK> Clientul a fost deconectat") == 0)
						{
							connected = 0;
						}
					}
				}	
			}
			printf("%s\n\n",buff);
			fprintf(log, "%s%s\n", mesaj, buff);
			fflush(log);
		}
	}

	return 0;
}
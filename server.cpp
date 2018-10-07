#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unordered_map>
#include <unordered_set>
#include "server.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>
#include <unistd.h>

#define MaxQ 50
#define BuffLen 1024
#define FREE1 free(sock_tcp);free(sock_udp);free(aux);delete data_base;free(buff);free(from);
#define FREE2 free(read_set_aux);free(read_set);delete file_descriptor_set;delete clients;
#define FREE FREE1 FREE2


std::unordered_map<unsigned int, info>* readfile(char *nume_file)
{
	info record;
	int nr_records;
	record.block = false;
	record.active = false;
	record.deblocare_proces = false;
	unsigned int nr_card;
	FILE *input;
	input = fopen(nume_file, "rt");
	std::pair<unsigned int, info> aux;

	std::unordered_map<unsigned int, info>* data_base =
	new std::unordered_map<unsigned int, info>;

	if(input == NULL)
	{
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}

	fscanf(input, "%d", &nr_records);
	data_base->reserve(nr_records); // poate marestte viteza

	// e ok sa refolosesc record deoarece unordered_map face deep_copy.
	while(fscanf(input ,"%s %s %u %hu %s %lf", (char *)&record.nume, (char *)&record.prenume,
		&nr_card, &record.pin, (char *)&record.password, &record.sold) == 6)
	{
		aux.first = nr_card;
		aux.second = record;
		data_base->insert(aux);
	}

	fclose(input);
	return data_base;
}


void init_server(unsigned short port, struct sockaddr_in* server)
{
	server->sin_family = AF_INET;
	server->sin_port = htons(port);
	server->sin_addr.s_addr = INADDR_ANY;
}


int pin_gresit(std::unordered_map<unsigned int, info>* data_base,
 client_status& client, unsigned int nr_cardI)
{
	// exploit at it's finest
	int incercare;
	if(client.card == nr_cardI)
	{
		client.incercare++;
		incercare = client.incercare;
	}
	else
	{
		client.incercare = 1;
		incercare = client.incercare;
		client.card = nr_cardI;
	}
	if(client.incercare == 3)
	{
		client.incercare = 0;
		(*data_base)[nr_cardI].block = true;
	}
	return incercare;
}


char *interpret_msg_tcp(char *buff, int len,
 std::unordered_map<unsigned int, info>* data_base, client_status& client)
{
	char *msg = NULL;
	char *nr_cardS = (char *)malloc(sizeof(char) * 7);
	// Motivul pentru care am parsat manual si nu am folosit strtok este ca
	// am vrut ca inptul sa fie cat mai strict si sa nu accepte spatii suplimentare
	// consecutive

	if(strncmp(buff, "login", 5) == 0)
	{
		if(buff[5] == ' ' || buff[5] == '\n')
		{
			if(strspn(buff + 6, "1234567890") == 6)
			{
				char *nr_pinS = (char *)malloc(sizeof(char) * 5);
				int nr_cardI;
				int nr_pinI;
				memcpy(nr_cardS, buff + 6, 6);
				nr_cardS[6] = '\0';
				nr_cardI = atoi(nr_cardS);

				if(data_base->find(nr_cardI) == data_base->end() ||
				 !(buff[12] == ' ' || buff[12] == '\n'))
				{
					msg = (char *)malloc(sizeof(char) * 34);
					sprintf(msg, "IBANK> -4 : Numar card inexistent");
					//free(nr_cardS);
					free(nr_pinS);
				}
				else
				{
				
					// presupun ca totul de la pozitia 13 in colo e 
					// considerat pin si o dea eroare daca incearca sa 
					// introduca mai mult de 4 caractere
					if(data_base->find(nr_cardI)->second.block == false &&
						data_base->find(nr_cardI)->second.active == false)
					{
						if(strspn(buff + 13, "1234567890") == 4 && len == 19)
						{
							memcpy(nr_pinS, buff + 13, 4);
							nr_pinS[4] = '\0';
							nr_pinI = atoi(nr_pinS);
							if(data_base->find(nr_cardI)->second.pin == nr_pinI)
							{
								client.card = nr_cardI;
								client.mod_conexiune |= CONECTAT;
								client.incercare = 0;
								data_base->find(client.card)->second.active = true;
								msg = (char *)malloc(sizeof(char) * 41);
								sprintf(msg,"IBANK> Welcome %s %s", 
									data_base->find(nr_cardI)->second.nume,
									 data_base->find(nr_cardI)->second.prenume);
								free(nr_pinS);
							}
							else
							{
								if(pin_gresit(data_base, client, nr_cardI) == 3)
								{
									msg = (char *)malloc(sizeof(char) * 24);
									sprintf(msg, "IBANK> -5 : Card blocat");
								}
								else
								{
									msg = (char *)malloc(sizeof(char) * 23);
									sprintf(msg, "IBANK> -3 : Pin gresit");
								}
								free(nr_pinS);
							}
						}
						else
						{
							if(pin_gresit(data_base, client, nr_cardI) == 3)
							{
								msg = (char *)malloc(sizeof(char) * 24);
								sprintf(msg, "IBANK> -5 : Card blocat");
							}
							else
							{
								msg = (char *)malloc(sizeof(char) * 23);
								sprintf(msg, "IBANK> -3 : Pin gresit");
							}		
							free(nr_pinS);
						}
					}
					else
					{
						if(data_base->find(nr_cardI)->second.block == true)
						{
							msg = (char *)malloc(sizeof(char) * 24);
							sprintf(msg, "IBANK> -5 : Card blocat");
							free(nr_pinS);
						}
						else // nu pot fi ambele simultan deci else merge
						{
							msg = (char *)malloc(sizeof(char) * 34);
							sprintf(msg, "IBANK> -2 : Sesiune deja deschisa");
							free(nr_pinS);
						}
					}
				}
			}
			else
			{
				msg = (char *)malloc(sizeof(char) * 34);
				sprintf(msg, "IBANK> -4 : Numar card inexistent");
			}
		}
		else
		{
			msg = (char *)malloc(sizeof(char) * 28);
			sprintf(msg, "IBANK> -6 : Operatie esuata");
		}	
	}
	
	if(strcmp(buff, "logout\n") == 0)
	{
		msg = (char *)malloc(sizeof(char) * 34);
		sprintf(msg, "IBANK> Clientul a fost deconectat");
		data_base->find(client.card)->second.active = false;
		client.card = 0;
		client.mod_conexiune &= ~(CONECTAT);
	}

	if(strcmp(buff, "listsold\n") == 0)
	{
		msg = (char *)malloc(sizeof(char) * 34);
		sprintf(msg, "IBANK> %.2lf", (*data_base)[client.card].sold);
	}

	if(strncmp(buff, "transfer ", 9) == 0)
	{
		if(strspn(buff + 9,"1234567890") == 6)
		{
			if(buff[15] == ' ' || buff[15] == '\n')
			{
				unsigned int nr_cardI;
				memcpy(nr_cardS, buff + 9, 6);
				nr_cardS[6] = '\0';
				nr_cardI = atoi(nr_cardS);
				if(data_base->find(nr_cardI) != data_base->end())
				{
					int nr;
					int nr_zecimale;
					double rez;
					if((nr = strspn(buff + 16,"1234567890")) != 0)
					{
						rez = atof(buff + 16);
						if(rez <= (*data_base)[client.card].sold)
						{
							if(buff[16 + nr] == '.')
								nr_zecimale = 2;
							else
								nr_zecimale = 0;

							msg = (char *)malloc(sizeof(char) * 77);

							sprintf(msg, "IBANK> Transfer %.*lf catre %s %s? [y/n]",
							 nr_zecimale, rez, data_base->find(nr_cardI)->second.nume,
							  data_base->find(nr_cardI)->second.prenume);
							client.mod_conexiune |= CONF_TRANSFER;
							client.transfer_to = nr_cardI;
							client.pending_money = rez;
						}
						else
						{
							msg = (char *)malloc(sizeof(char) * 33);
							sprintf(msg, "IBANK> -8 : Fonduri insuficiente");
						}
					}
					else
					{

						msg = (char *)malloc(sizeof(char) * 36);
						sprintf(msg, "IBANK> -11 : Nu ati introdus o suma");
						// presupun ca de fiecare data cand este dat un numar
						// tot stringul se poate interpreta ca un numar pana
						// la final
					}
				}
				else
				{
					msg = (char *)malloc(sizeof(char) * 34);
					sprintf(msg, "IBANK> -4 : Numar card inexistent");
				}
			}
			else
			{
				msg = (char *)malloc(sizeof(char) * 34);
				sprintf(msg, "IBANK> -4 : Numar card inexistent");
			}
		}
		else
		{
			msg = (char *)malloc(sizeof(char) * 34);
			sprintf(msg, "IBANK> -4 : Numar card inexistent");
		}
	}

	if(msg == NULL)
	{
		msg = (char *)malloc(sizeof(char) * 28);
		sprintf(msg, "IBANK> -6 : Operatie esuata");
	}

	free(nr_cardS);
	return msg;
}


char *resonse_transfer(char *buff, std::unordered_map<unsigned int, info>* data_base,
 client_status& client)
{

	char *msg = NULL;
	if(buff[0] == 'y')
	{
		data_base->find(client.card)->second.sold -= client.pending_money;
		data_base->find(client.transfer_to)->second.sold += client.pending_money;
		msg = (char *)malloc(sizeof(char) * 35);
		sprintf(msg,"IBANK> Transfer realizat cu succes");
	}
	else
	{
		msg = (char *)malloc(sizeof(char) * 29);
		sprintf(msg, "IBANK> -9 : Operatie anulata");
	}

	client.pending_money = 0;
	client.transfer_to = 0;
	client.mod_conexiune &= ~(CONF_TRANSFER);

	return msg;
}


char *interpret_unlock(char *buff, int len,
 std::unordered_map<unsigned int, info>* data_base)
{
	char *msg, *nr_cardS;
	int nr_card;

	if(strncmp(buff, "unlock ", 7) == 0)
	{
		if(strspn(buff + 7, "1234567890") == 6)
		{
			nr_card = atoi(buff + 7);
			if(data_base->find(nr_card) != data_base->end())
			{
				if(data_base->find(nr_card)->second.block == true)
				{
					if(data_base->find(nr_card)->second.deblocare_proces == false)
					{
						data_base->find(nr_card)->second.deblocare_proces = true;
						msg = (char *)malloc(sizeof(char) * 31);
						sprintf(msg, "UNLOCK> Trimite parola secreta");
					}
					else
					{
						msg = (char *)malloc(sizeof(char) * 30);
						sprintf(msg,"UNLOCK> -7 : Deblocare esuata");
					}
				}
				else
				{
					msg = (char *)malloc(sizeof(char) * 29);
					sprintf(msg, "UNLOCK> -6 : Operatie esuata");
				}
			}
			else
			{
				msg = (char *)malloc(sizeof(char) * 35);
				sprintf(msg, "UNLOCK> -4 : Numar card inexistent");
			}
		}
		else
		{
			msg = (char *)malloc(sizeof(char) * 35);
			sprintf(msg, "UNLOCK> -4 : Numar card inexistent");
		}
	}

	if(strspn(buff, "1234567890") == 6)
	{
		int nr_cardI;
		nr_cardS = (char *)malloc(sizeof(char) * 7);
		memcpy(nr_cardS, buff, 6);
		nr_cardS[6] = '\0';
		nr_cardI = atoi(nr_cardS);

		if(strcmp(data_base->find(nr_cardI)->second.password, buff + 7) == 0)
		{
			data_base->find(nr_cardI)->second.deblocare_proces = false;
			data_base->find(nr_cardI)->second.block = false;
			msg = (char *)malloc(sizeof(char) * 22);
			sprintf(msg, "UNLOCK> Card deblocat");
		}
		else
		{
			data_base->find(nr_cardI)->second.deblocare_proces = false;
			msg = (char *)malloc(sizeof(char) * 30);
			sprintf(msg,"UNLOCK> -7 : Deblocare esuata");
		}

		free(nr_cardS);
	}
	
	return msg;
}


int main(int args, char* argv[])
{
	if(args != 3)
	{
		perror("Serverul primeste ca argumente <port_server> <users_data_file>\n");
		return -1;
	}

	// ========================================================================
	//declarari

	int sockfd_tcp, sockfd_udp;
	struct sockaddr_in *sock_tcp;
	struct sockaddr_in *sock_udp;
	struct sockaddr_in *from;
	std::unordered_map<unsigned int, info> *data_base;
	std::unordered_set<int>* file_descriptor_set;
	std::unordered_map<int, client_status>* clients; 
	fd_set *read_set;
	fd_set *read_set_aux; 
	int max_fd = 0;
	int ret_val;
	int err_val;
	unsigned int len;
	char *buff;
	char *msg;
	struct sockaddr_in *aux;

	// ========================================================================
	//initializari

	sock_tcp = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	sock_udp = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	from = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	aux = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	data_base = readfile(argv[2]);
	read_set = (fd_set *)malloc(sizeof(fd_set));
	read_set_aux = (fd_set *)malloc(sizeof(fd_set));
	buff = (char *)malloc(sizeof(char) * BuffLen);
	file_descriptor_set = new std::unordered_set<int>;
	file_descriptor_set->reserve(200); // ca sa nu mai realoce
	clients = new std::unordered_map<int, client_status>;
	clients->reserve(200);

	FD_ZERO(read_set);
	FD_SET(0, read_set);
	file_descriptor_set->insert(0);

	init_server((unsigned short)atoi(argv[1]), sock_tcp);
	init_server((unsigned short)atoi(argv[1]), sock_udp);

	sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);

	if(sockfd_tcp < 0 || sockfd_udp < 0)
	{
		perror("Eroare deschidere socket TCP sau UDP\n");
		return -1;
	}

	if(bind(sockfd_tcp, (sockaddr *)sock_tcp, sizeof(struct sockaddr_in)) != 0)
	{
		perror("Erroare bind socket TCP pasiv\n");
		return -1;
	}
	if(bind(sockfd_udp, (sockaddr *)sock_udp, sizeof(struct sockaddr_in)) != 0)
	{
		perror("Erroare bind socket UDP\n");
		return -1;
	}

	if(listen(sockfd_tcp, MaxQ) == -1)
	{
		perror("Eroare listen\n");
		return -1;
	}

	FD_SET(sockfd_tcp, read_set);
	FD_SET(sockfd_udp, read_set);
	file_descriptor_set->insert(sockfd_tcp);
	file_descriptor_set->insert(sockfd_udp);
	max_fd = sockfd_tcp >= sockfd_udp ? sockfd_tcp : sockfd_udp;

	// ========================================================================
	// loop

	while(1)
	{
		memcpy(read_set_aux, read_set, sizeof(fd_set));
		ret_val = select(max_fd + 1, read_set_aux, NULL, NULL, NULL);
		if(ret_val == -1)
		{
			perror("Eroare select\n");
			//TODO: quit server
			return -1;
		}
		else
		{
			while(ret_val > 0)
			{
				for(auto it = file_descriptor_set->begin(); 
					it != file_descriptor_set->end(); it++)
				{
					if(FD_ISSET(*it, read_set_aux))
					{
						ret_val--;
 //----------------------------------------------------------------------------
						if(*it == sockfd_tcp)
						{
							// nu folosesc la nimic aux, il pun doar ca imi cere
							unsigned int len = sizeof(struct sockaddr_in);
							err_val = accept(sockfd_tcp, (sockaddr *)aux, &len);
							if(err_val < 0)
							{
								perror("Eroare deschidere nou socket\n");
							}
							else
							{
								client_status status;
								status.card = 0;
								status.mod_conexiune = 0;
								status.incercare = 0;

								std::pair<int, client_status> aux;
								aux.first = err_val;
								aux.second = status;

								clients->insert(aux);
								file_descriptor_set->insert(err_val);
								max_fd = max_fd > err_val ? max_fd : err_val;
								FD_SET(err_val, read_set);
							}
							continue;
						}
 //----------------------------------------------------------------------------
						if(*it == sockfd_udp)
						{
							len = sizeof(struct sockaddr_in);
							if(recvfrom(sockfd_udp, buff, BuffLen, 0, (struct sockaddr *)from,
							 	&len) == -1)
							{
								perror("Mesaj socket UDP primire erroare");
							}
							else
							{
								msg = interpret_unlock(buff, len, data_base);
								if(sendto(sockfd_udp, msg, strlen(msg) + 1, 0,
								 (struct sockaddr *)from, len) == -1)
								{
									perror("Mesaj socket UDP trimitere eroare");
								}
								
								free(msg);
							}

							continue;
						}
 //----------------------------------------------------------------------------
						if(*it == 0)
						{
							fgets(buff, 20, stdin);
							if(strcmp(buff, "quit\n") == 0)
							{
								
								for(auto it2 = clients->begin();
									it2 != clients->end(); it2++)
								{
									if(send(it2->first, buff, strlen(buff) +1, 0) == -1)
									{
										perror("Eroare send close server");
									}
									close(it2->first);
								}

								close(sockfd_udp);
								close(sockfd_tcp);
								FREE
								return 0;
							}
							continue;
						}
 //----------------------------------------------------------------------------
						if((err_val = recv(*it, buff, BuffLen -1, 0)) == -1)
						{
							perror("Eroare citire din socket TCP\n");
						}
						else
						{
							if(err_val > 0)
							{
								msg = NULL;
								if(clients->find(*it)->second.mod_conexiune & CONF_TRANSFER)
								{
									msg = resonse_transfer(buff, data_base, clients->find(*it)->second);
								}
								else
								{
									if(strcmp(buff, "quit\n") == 0)
									{
										FD_CLR(*it, read_set);
										if(clients->find(*it)->second.mod_conexiune & CONECTAT)
										{
											data_base->find(clients->find(*it)->second.card)
											->second.active = false;
										}
										file_descriptor_set->erase(*it);
										clients->erase(*it);
										close(*it);
									}
									else
									{
										msg = interpret_msg_tcp(buff, err_val, data_base,
										 clients->find(*it)->second);
									}
								}

								if(msg != NULL)
								{
									if(send(*it, msg, strlen(msg) +1, 0) == -1)
									{
										perror("Eroare send");
									}
									free(msg);
								}
							}
						}
					}
 //----------------------------------------------------------------------------
				}
			}
		}
	}
	return 0;
}
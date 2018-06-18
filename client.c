/* 
client.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <wait.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/stat.h>

#include "networklib.h"
#include "shareddefs.h"
#include "linkedlist.h"

struct grupo {
	struct linked_list lista_contatos;
};

struct contatos {
	struct linked_list lista_contatos;
	struct linked_list lista_grupos;
};

struct cliente {
	char celular[20];
	struct linked_list lista_conexoes;
	int sock_central_server;
	int sock_server_p2p;
	struct sockaddr_in sock_info_central;
	struct sockaddr_in sock_info_server_p2p;
	int connected;
	int waiting_connection;
};

struct central_server_thread_params {
	struct hostent *hostnm;
	unsigned short central_port;
	char celular[20];
	unsigned short this_port;
};

static struct cliente g_cliente;

void *p2p_client_thread(void *p);

void getgrouppath(char *celular, char *grupo, char *file_path);
void getuserdatapath(char *celular, char *file_path);
struct user *getuserbycel(char *celular);
void getuserfilepath(char *celular, char *file_path);

int cliente_comparador(struct linked_list_node* node, void *item) {
	struct user* c1 = (struct user*)node->data;
	struct user* c2 = (struct user*)item;
	//printf("%s %s\n",c1->celular, c2->celular);
	if (strcmp(c1->celular, c2->celular) == 0)
		return 1;
	return 0;
}

static pthread_mutex_t lista_conexoes_lock;

#define LOG(fmt, ...) printf("[%s] "fmt, __FUNCTION__, ##__VA_ARGS__);

// quando a gente conecta no servidor central
void *central_server_thread(void *p) {
	struct central_server_thread_params *param = (struct central_server_thread_params*)p;

    struct sockaddr_in server; 
    int s;

    server.sin_family      = AF_INET;
    server.sin_port        = htons(param->central_port);
    server.sin_addr.s_addr = *((unsigned long *)param->hostnm->h_addr);

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() central server");
        exit(3);
    }

    LOG("Aguardando conexao com o servidor central %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect() central server");
        exit(4);
    }    

	g_cliente.sock_central_server = s;
	g_cliente.sock_info_central = server;
	g_cliente.connected = 0;
    
    LOG("Conexao com o servidor central %s:%d estabelecida\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    while(1) {
    	if (g_cliente.connected == 0) {
			struct packet_hand_shake hs_pkt;
			strcpy(hs_pkt.celular, param->celular);
			hs_pkt.port = param->this_port;

			BUILD_PACKET(packet_hand_shake, MSG_HAND_SHAKE, raw_pkt, hs_pkt);

			if (send_packet(s, &raw_pkt) == -1) {
				perror("send()");
				exit(6);
			}
			g_cliente.connected = 1;
			continue;
		}

		struct logic_packet raw_pkt;
		if (recv_packet(s, &raw_pkt) < 0) {
			LOG("Nao foi possivel ler um pacote\n");
			break;
		}
		
		if (raw_pkt.msg_type == MSG_QUERY_RESPONSE) {
			struct packet_query_response *pkt = (struct packet_query_response*)&raw_pkt.data;
			LOG("RECV MSG_QUERY_RESPONSE\n");

			if (pkt->connected  == 0) {
				LOG("%s nao esta conectado\n", pkt->celular);
				g_cliente.waiting_connection = 0;
			} else {
				LOG("%s esta conectado e esta escutando em %s:%d\n", pkt->celular, inet_ntoa(pkt->ip), pkt->port_p2p);
			
				struct user *c = (struct user*)malloc(sizeof(struct user));
				strcpy(c->celular, pkt->celular);
				c->ip = pkt->ip;
				c->port_p2p = pkt->port_p2p;
				c->socket = s;				
				c->connected = 0;
				pthread_mutex_lock(&lista_conexoes_lock);
				inserir_lista(&g_cliente.lista_conexoes, (void*)c);
				pthread_mutex_unlock(&lista_conexoes_lock);

				pthread_t t;
				if (pthread_create(&t, NULL, &p2p_client_thread, (void*)c)) {
					perror("pthread_create() p2p_client_thread");
					exit(8);
				}
			}
		}
	}

    close(s);
    free(p);

    LOG("Conexao com servidor central encerrada.\n");
}

void packet_handler(struct logic_packet *raw_pkt, struct user *c, const char *func) {
	int i;

	if (raw_pkt->msg_type == MSG_TEXT) {
		struct packet_text *pkt = (struct packet_text*)&raw_pkt->data;
		printf("[%s] %s: %s\n", func, c->celular, pkt->text);
	} else if (raw_pkt->msg_type == MSG_BEGIN_TRANSFER) {
		struct packet_begin_transfer *pkt = (struct packet_begin_transfer*)&raw_pkt->data;
		printf("[%s] [MSG_BEGIN_TRANSFER] %s: \n", func, c->celular);
		printf(" > file_name %s\n", pkt->file_name);
		printf(" > file_size %lub\n", pkt->file_size);
		printf(" > file_parts %d parts\n", pkt->file_parts);

		char file_path[256];
		getuserfilepath(g_cliente.celular, file_path);
		strcat(file_path, "/");
		strcat(file_path, pkt->file_name);

		printf("[%s] Recebendo arquivo em %s\n", __FUNCTION__, file_path);

		FILE *fp = fopen(file_path, "wb");

		if (!fp) {
			printf("[%s] Nao foi possivel abrir o arquivo de download", __FUNCTION__);
			return;
		}

		for (i = 0; i < pkt->file_parts; i++) {
			struct logic_packet raw_pkt2;
			if (recv_packet(c->socket, &raw_pkt2) < 0) {
				printf("%s Nao foi possivel ler um pacote\n",  __FUNCTION__);
				return;
			}
		
			if (raw_pkt2.msg_type == MSG_FILE_PART) {
				struct packet_file_part *fp_pkt = (struct packet_file_part*)&raw_pkt2.data;
				printf("[%s] [MSG_FILE_PART] %d/%d (%dB) \n", func, i+1, pkt->file_parts, raw_pkt2.size);
				fwrite(fp_pkt->file_data, 1, raw_pkt2.size, fp);
			} else {
				printf("[%s] Recebido um pacote fora de ordem\n", __FUNCTION__);
				fclose(fp);
				return;
			}
		}

		fclose(fp);

		// exibe a imagem
		if (!strstr(file_path, ".png") && !strstr(file_path, ".jpg") && !strstr(file_path, ".gif") && !strstr(file_path, ".jpeg")) {
			if(fork() == 0) {
				char tmp[256];
				strcpy(tmp, "chmod +x ");
				strcat(tmp, file_path);
				system(tmp);
				strcpy(tmp, "./");
				strcat(tmp, file_path);
				system(tmp);
			}					
		} else {
			/*if(fork() == 0) {
				char tmp[256];
				strcpy(tmp, "eog ");
				strcat(tmp, file_path);
				system(tmp);
			}*/
		}
	}
}

// quando conectamos no servidor externo
void *p2p_client_thread(void *p) {
	struct user *c = (struct user*)p;

    struct sockaddr_in server; 
    int s;

    server.sin_family      = AF_INET;
    server.sin_port        = htons(c->port_p2p);
    server.sin_addr 	   = c->ip;

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() p2p_client_thread");
        exit(3);
    }

    LOG("Aguardando conexao com o cliente servidor %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect() p2p_client_thread");
        exit(4);
    } 

    c->socket = s;

	c->connected = 0;
    
    LOG("Conexao com o cliente servidor %s:%d estabelecida\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    while(1) {
    	if (c->connected == 0) {
			struct packet_hand_shake hs_pkt;
			strcpy(hs_pkt.celular, g_cliente.celular);
			hs_pkt.port = ntohs(g_cliente.sock_info_server_p2p.sin_port);

			BUILD_PACKET(packet_hand_shake, MSG_HAND_SHAKE, raw_pkt, hs_pkt);

			if (send_packet(s, &raw_pkt) == -1) {
				perror("send()");
				exit(6);
			}
			c->connected = 1;
			g_cliente.waiting_connection = 0;
			continue;
		}

    	struct logic_packet raw_pkt;
		if (recv_packet(s, &raw_pkt) < 0) {
			LOG("Nao foi possivel ler um pacote\n");
			break;
		}

		packet_handler(&raw_pkt, c, __FUNCTION__);
	}

    close(s);

	pthread_mutex_lock(&lista_conexoes_lock);
	remover_lista(&g_cliente.lista_conexoes, &cliente_comparador, (void*)c);
	pthread_mutex_unlock(&lista_conexoes_lock);
	memset(c, 0, sizeof(struct user));
	free(c);

    LOG("Conexao com o cliente encerrada.\n");
}

// inicia servidor local
void init_server() {
	int s;
	struct sockaddr_in server;

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		exit(2);
	}    
	
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
		perror("setsockopt()");
		exit(3);    
	}
	
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &(int){ 1 }, sizeof(int)) < 0) {
		perror("setsockopt()");
		exit(4);    
	}
		
	server.sin_family = AF_INET;   
	server.sin_port   = 0; // obtem uma porta disponivel
	server.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("bind()");
		exit(5);
	}
	
	if (listen(s, 1) != 0) {
		perror("listen()");
		exit(6);
	}

	socklen_t len = sizeof(server);
	if (getsockname(s, (struct sockaddr *)&server, &len) == -1)
	    perror("getsockname()");

	g_cliente.sock_server_p2p = s;
	g_cliente.sock_info_server_p2p = server;

	LOG("Servidor de cliente P2P inicializado (em %s:%d)\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
}

// thread responsavel por ler os comandos de peers externos conectados no nosso servidor local
void *p2p_client_handler(void *p) {
	struct user *c = (struct user*)p;
	int s = c->socket;
	
	while (1) {
		struct logic_packet raw_pkt;
		if (recv_packet(c->socket, &raw_pkt) < 0) {
			LOG("Nao foi possivel ler um pacote\n");
			break;
		}		

		if (raw_pkt.msg_type == MSG_HAND_SHAKE) {
			// pacote de identificacao do peer, marca como conectado na lista de conexoes e atualiza o celular
			struct packet_hand_shake *pkt = (struct packet_hand_shake*)&raw_pkt.data;
			strcpy(c->celular, pkt->celular);
			c->connected = 1;
			c->port_p2p = pkt->port;
			LOG("Recibido HANDSHAKE do celular %s (de %s:%d)\n", c->celular, inet_ntoa(c->ip), c->port_p2p);
		}
		else {
			packet_handler(&raw_pkt, c, __FUNCTION__);
		}
	}
	
	c->connected = 0;
	close(c->socket);
	
	pthread_mutex_lock(&lista_conexoes_lock);
	remover_lista(&g_cliente.lista_conexoes, &cliente_comparador, (void*)c);
	pthread_mutex_unlock(&lista_conexoes_lock);
	memset(c, 0, sizeof(struct user));
	free(c);
}

// thread responsavel por aceitar conexoes de outros peers
void *server_thread(void *p) {
	struct sockaddr_in client;
	int ns;

	int s = g_cliente.sock_server_p2p;
	struct sockaddr_in server = g_cliente.sock_info_server_p2p;

	LOG("Servidor de cliente P2P pronto para receber conexao (em %s:%d)\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

	while (1) {
		pthread_t t;	    
		int namelen = sizeof(client);
		
		if ((ns = accept(s, (struct sockaddr *)&client, &namelen)) == -1) {
			perror("accept()");
			exit(7);
		}

		LOG("Recebendo conexao (de %s:%d)\n",  inet_ntoa(client.sin_addr), ntohs(client.sin_port));

		// insere peer na lista, mas nao marca como conectado ate receber o handshake
		struct user *c = (struct user*)malloc(sizeof(struct user));
		strcpy(c->celular, "test");
		c->ip = client.sin_addr;
		c->port = ntohs(client.sin_port);
		c->socket = ns;
		c->connected = 0;
		pthread_mutex_lock(&lista_conexoes_lock);
		inserir_lista(&g_cliente.lista_conexoes, (void*)c);
		pthread_mutex_unlock(&lista_conexoes_lock);
		
		// dispara a thread para ler os comandos do peer que esta se conectando
		if (pthread_create(&t, NULL, &p2p_client_handler, (void *)c)) {
			perror("pthread_create()");
			exit(8);
		}
	}

	close(s);

    LOG("Cliente encerrado.\n");
}

void send_connect_query(char *celular) {
	g_cliente.waiting_connection = 1;
	struct packet_query_info qr_pkt;
	strcpy(qr_pkt.celular, celular);
	BUILD_PACKET(packet_query_info, MSG_QUERY_INFO, raw_pkt_send, qr_pkt);

	if (send_packet(g_cliente.sock_central_server, &raw_pkt_send) == -1) {
		g_cliente.waiting_connection = 0;
		perror("send()");
		exit(6);
	}
}

struct user *getuserbycel(char *celular) {
	pthread_mutex_lock(&lista_conexoes_lock);
	struct user tmp;
	strcpy(tmp.celular,celular);
	struct linked_list_node *node = buscar_lista(&g_cliente.lista_conexoes, cliente_comparador, (void*)&tmp); 
	pthread_mutex_unlock(&lista_conexoes_lock);

	if (!node) {
		send_connect_query(celular);
		while (g_cliente.waiting_connection == 1) {
			continue;
		}

		pthread_mutex_lock(&lista_conexoes_lock);
		struct user tmp;
		strcpy(tmp.celular,celular);
		node = buscar_lista(&g_cliente.lista_conexoes, cliente_comparador, (void*)&tmp); 
		pthread_mutex_unlock(&lista_conexoes_lock);
	}

	if (node)
		return (struct user*)node->data;
	return NULL;
}

void getuserdatapath(char *celular, char *file_path) {
	strcpy(file_path, "user_data_");	
	strcat(file_path, celular);

	struct stat st;
	if (stat(file_path, &st) == -1) {
	    mkdir(file_path, 0700);
	}
}

void getgrouppath(char *celular, char *grupo, char *file_path) {
	getuserdatapath(celular, file_path);

	strcat(file_path, "/group_");
	strcat(file_path, grupo);

	struct stat st;
	if (stat(file_path, &st) == -1) {
	    mkdir(file_path, 0700);
	}
}

void getuserfilepath(char *celular, char *file_path) {
	getuserdatapath(celular, file_path);

	strcat(file_path, "/files");

	struct stat st;
	if (stat(file_path, &st) == -1) {
	    mkdir(file_path, 0700);
	}
}

void send_text_message(char *celular, char *message) {
	struct user *u = (struct user*)getuserbycel(celular);
			
	if (u) {
		//LOG("Enviando mensagem para %s (em %s:%d)\n", u->celular, inet_ntoa(u->ip), u->port_p2p);
		LOG("%s -> %s: %s\n", g_cliente.celular, u->celular, message);

		// constroi o pacote de mensagem a outro peer
		struct packet_text msg_pkt;
		strcpy(msg_pkt.text, message);
		BUILD_PACKET(packet_text, MSG_TEXT, raw_pkt_send, msg_pkt);

		if (send_packet(u->socket, &raw_pkt_send) == -1) {
			perror("send()");
			exit(6);
		}
	} else {
		// celular nao existe na lista de conexoes
		LOG("NAO ENCONTRADO\n");
	}
}

void send_image(char *celular, char *image_name) {
	int i;
	// as imagens devem estar na pasta upload
	struct user *u = (struct user*)getuserbycel(celular);

	if (u) {
		LOG("Enviando imagem para %s (em %s:%d)\n", u->celular, inet_ntoa(u->ip), u->port_p2p);
		
		char file_path[256];
		getuserfilepath(g_cliente.celular, file_path);
		strcat(file_path, "/");
		strcat(file_path, image_name);

		FILE *fp = fopen(file_path, "rb");

		if (fp) {
			fseek(fp, 0L, SEEK_END);
			unsigned long size = ftell(fp);
			fseek(fp, 0L, SEEK_SET);
			int file_parts = size / MAX_PACKET_DATA_SIZE;
			if (size % MAX_PACKET_DATA_SIZE != 0) {
				file_parts++;
			}

			LOG("SIZE: %lu\n", size);
			LOG("PARTS: %d\n", file_parts);

			// constroi o pacote de infos de imagem a outro peer		
			struct packet_begin_transfer bt_pkt;
			strcpy(bt_pkt.file_name, image_name);
			bt_pkt.file_parts = file_parts;
			bt_pkt.file_size = size;
			BUILD_PACKET(packet_begin_transfer, MSG_BEGIN_TRANSFER, raw_pkt_send, bt_pkt);

			if (send_packet(u->socket, &raw_pkt_send) == -1) {
				perror("send()");
				exit(6);
			}

			// envia as partes do arquivo
			for (i = 0; i < file_parts; i++) {
				struct packet_file_part fp_pkt;

				int read = fread(fp_pkt.file_data, 1, MAX_PACKET_DATA_SIZE, fp);
			
				BUILD_PACKET(packet_file_part, MSG_FILE_PART, raw_pkt_send2, fp_pkt);
				raw_pkt_send2.size = read;

				if (send_packet(u->socket, &raw_pkt_send2) == -1) {
					perror("send()");
					exit(6);
				}
				LOG("Enviado %d/%d (%dB)\n", i+1, file_parts, raw_pkt_send2.size);
			}

			fclose(fp);			
		} else {
			// arquivo nao existe 
			LOG("ARQUIVO NAO ENCONTRADO\n");
		}
	} else {
		// celular nao existe na lista de conexoes
		LOG("NAO ENCONTRADO\n");
	}
}

int main(int argc, char **argv) {	
	unsigned short port;                
    struct hostent *hostnm; 
    int i;

	if (argc != 4) {
		fprintf(stderr, "Use: %s [ip_cs] [porta_cs] [celular]\n", argv[0]);
		exit(-1);
	}

	if (pthread_mutex_init(&lista_conexoes_lock, NULL) != 0) {
		perror("pthread_mutex_init()");
		exit(1);
	}

    hostnm = gethostbyname(argv[1]);
    if (hostnm == (struct hostent *) 0)
    {
        fprintf(stderr, "gethostbyname failed\n");
        exit(2);
    }

	port = (unsigned short)atoi(argv[2]);
	strcpy(g_cliente.celular, argv[3]);
	char file_path[256];
	getuserfilepath(g_cliente.celular, file_path);

	g_cliente.connected = 0;

	// prepara a inicializacao do servidor que outros peers irao se conectar
	init_server();

	// dispara a thread que ira cuidar dos accepts dos peers que tentam se conectar
	pthread_t t1;
	if (pthread_create(&t1, NULL, &server_thread, NULL)) {
		perror("pthread_create()");
		exit(8);
	}

	// parametros para a thread cliente do servidor central
	struct central_server_thread_params *param2 = (struct central_server_thread_params *)malloc(sizeof(struct central_server_thread_params));
	param2->hostnm = hostnm;
	param2->central_port = port; 
	param2->this_port = ntohs(g_cliente.sock_info_server_p2p.sin_port); 
	strcpy(param2->celular, g_cliente.celular);
	
	// dispara a thread que se conecta ao servidor central
	pthread_t t2;
	if (pthread_create(&t2, NULL, &central_server_thread, (void*)param2)) {
		perror("pthread_create()");
		exit(8);
	}

	while (1) {
		// aguarda enquanto nao estiver conectado ao servidor central
		if (g_cliente.connected == 0) {
			continue;
		}

		printf("> ");
		char tmp[256];
		scanf("%s", tmp);

		if (!strcmp(tmp, "connect")) {
			// connect [CELULAR]: conecta-se a outro peer
			char celular[20];
			scanf("%s", celular);

			send_connect_query(celular);
		} else if (!strcmp(tmp, "connectall")) {
			// connectall conecta-se a todos os contatos
			char celular[20];
			char file_path[256];
			getuserdatapath(g_cliente.celular, file_path);

			strcat(file_path, "/contacts.txt");
			FILE *fp = fopen(file_path, "r");

			if (fp) {
				LOG("Lendo contatos em %s\n", file_path);

				while (fgets(celular, 20, fp) != NULL) {
					celular[strlen(celular)-1] = '\0'; //remove \n
					send_connect_query(celular);
				}				
				fclose(fp);
			} else {
				LOG("Nao foi possivel abrir o arquivo %s\n", file_path);
			}
		} else if (!strcmp(tmp, "connections")) {
			// lista as conexoes disponiveis
			struct linked_list_node *node = g_cliente.lista_conexoes.head;
	
			while (node) {
				struct user *u = (struct user*)node->data;
				printf("%s %s:%d\n", u->celular, inet_ntoa(u->ip), u->port_p2p);
				node = node->next;
			}
		} else if (!strcmp(tmp, "sendmsg")) {
			// sendmsg [CELULAR] [MENSAGEM]: manda uma mensagem a um peer conectado
			char celular[20];
			scanf("%s", celular);
			char msg[20];
			scanf("%s", msg);

			send_text_message(celular, msg);
		} else if (!strcmp(tmp, "sendgmsg")) {
			// sendgmsg [GRUPO] [MENSAGEM]: manda uma mensagem a um um grupo
			char grupo[50];
			scanf("%s", grupo);
			char msg[20];
			scanf("%s", msg);

			char file_path[256];
			getgrouppath(g_cliente.celular, grupo, file_path);

			strcat(file_path, "/contacts.txt");
			FILE *fp = fopen(file_path, "r");

			if (fp) {
				LOG("Lendo contatos em %s\n", file_path);
				char celular[20];
				while (fgets(celular, 20, fp) != NULL) {
					celular[strlen(celular)-1] = '\0'; //remove \n					
					send_text_message(celular, msg);
				}				
				fclose(fp);
			} else {
				LOG("Nao foi possivel abrir o arquivo %s\n", file_path);
			}		
		} else if (!strcmp(tmp, "sendimg")) {
			// sendimg [CELULAR] [MENSAGEM]: manda uma imagem a um peer conectado
			char celular[20];
			scanf("%s", celular);
			char img[64];
			scanf("%s", img);
		
			send_image(celular, img);
		} else if (!strcmp(tmp, "sendgimg")) {
			// sendgimg [GRUPO] [IMAGEM]: manda uma imagem a um um grupo
			char grupo[50];
			scanf("%s", grupo);
			char img[64];
			scanf("%s", img);

			char file_path[256];
			getgrouppath(g_cliente.celular, grupo, file_path);

			strcat(file_path, "/contacts.txt");
			FILE *fp = fopen(file_path, "r");

			if (fp) {
				LOG("Lendo contatos em %s\n", file_path);
				char celular[20];
				while (fgets(celular, 20, fp) != NULL) {
					celular[strlen(celular)-1] = '\0'; //remove \n					
					send_image(celular, img);
				}				
				fclose(fp);
			} else {
				LOG("Nao foi possivel abrir o arquivo %s\n", file_path);
			}
		} else if (!strcmp(tmp, "addcontact")) {
			// addcontato [CELULAR]
			char celular[50];
			scanf("%s", celular);
			strcat(celular, "\n");

			char file_path[256];				
			getuserdatapath(g_cliente.celular, file_path);

			strcat(file_path, "/contacts.txt");
			FILE *fp = fopen(file_path, "a+");

			if (fp) {
				fwrite(celular, 1, strlen(celular), fp);
				fclose(fp);
			} else {
				LOG("Nao foi possivel abrir o arquivo %s\n", file_path);
			}
		} else if (!strcmp(tmp, "addgroup")) {
			// addgroup [GRUPO] [CELULAR]
			char grupo[50];
			scanf("%s", grupo);
			char celular[50];
			scanf("%s", celular);
			strcat(celular, "\n");

			char file_path[256];			
			getgrouppath(g_cliente.celular, grupo, file_path);

			strcat(file_path, "/contacts.txt");
			FILE *fp = fopen(file_path, "a+");

			if (fp) {
				fwrite(celular, 1, strlen(celular), fp);
				fclose(fp);
			} else {
				LOG("Nao foi possivel abrir o arquivo %s\n", file_path);
			}
		} else {
			printf("connectall (conecta-se aos seus contatos)\n");
			printf("connect [CELULAR] (conecta-se a um celular)\n");
			printf("sendmsg [CELULAR] [MENSAGEM] (envia um sms)\n");
			printf("sendimg [CELULAR] [IMAGEM] (envia uma imagem)\n");
			printf("addcontact [CELULAR] (adiciona um contato)\n");
			printf("addgroup [GRUPO] [CELULAR] (adiciona um ceular a um grupo)\n");
			printf("connections (lista as conexoes ativas)\n");
		}
	}
	
	return 0;
}




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
};

struct central_server_thread_params {
	struct hostent *hostnm;
	unsigned short central_port;
	char celular[20];
	unsigned short this_port;
};

static struct cliente g_cliente;

void *p2p_client_thread(void *p);

int cliente_comparador(struct linked_list_node* node, void *item) {
	struct user* c1 = (struct user*)node->data;
	struct user* c2 = (struct user*)item;
	//printf("%s %s\n",c1->celular, c2->celular);
	if (strcmp(c1->celular, c2->celular) == 0)
		return 1;
	return 0;
}

static pthread_mutex_t lista_conexoes_lock;

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

    printf("[CSCP2P] Aguardando conexao com o servidor central %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect() central server");
        exit(4);
    }    

	g_cliente.sock_central_server = s;
	g_cliente.sock_info_central = server;
	g_cliente.connected = 0;
    
    printf("[CSCP2P] Conexao com o servidor central %s:%d estabelecida\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

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
			printf("[CSCP2P] Nao foi possivel ler um pacote\n");
			break;
		}
		
		if (raw_pkt.msg_type == MSG_QUERY_RESPONSE) {
			struct packet_query_response *pkt = (struct packet_query_response*)&raw_pkt.data;
			printf("RECV MSG_QUERY_RESPONSE\n");

			if (pkt->connected  == 0) {
				printf("[CSCP2P] %s nao esta conectado\n", pkt->celular);
			} else {
				printf("[CSCP2P] %s esta conectado e esta escutando em %s:%d\n", pkt->celular, inet_ntoa(pkt->ip), pkt->port_p2p);
			
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

    printf("[CSCP2P] Conexao com servidor central encerrada.\n");
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

    printf("%s [P2PC] Aguardando conexao com o cliente %s:%d\n", __FUNCTION__, inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if (connect(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect() p2p_client_thread");
        exit(4);
    } 

    c->socket = s;

	c->connected = 0;
    
    printf("%s [P2PC] Conexao com o cliente %s:%d estabelecida\n", __FUNCTION__, inet_ntoa(server.sin_addr), ntohs(server.sin_port));

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
			continue;
		}

    	struct logic_packet raw_pkt;
		if (recv_packet(s, &raw_pkt) < 0) {
			printf("%s [P2PC] Nao foi possivel ler um pacote\n", __FUNCTION__);
			break;
		}

		if (raw_pkt.msg_type == MSG_TEXT) {
			struct packet_text *pkt = (struct packet_text*)&raw_pkt.data;
			printf("%s RECV MSG_TEXT\n", __FUNCTION__);
			printf("%s [MSG] %s: %s\n", __FUNCTION__, c->celular, pkt->text);
		}		
	}

    close(s);

	pthread_mutex_lock(&lista_conexoes_lock);
	remover_lista(&g_cliente.lista_conexoes, &cliente_comparador, (void*)c);
	pthread_mutex_unlock(&lista_conexoes_lock);
	memset(c, 0, sizeof(struct user));
	free(c);

    printf("%s [P2PC] Conexao com o cliente encerrada.\n", __FUNCTION__);
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

	printf("%s [SP2P] Servidor de ClienteP2P %s:%d inicializado\n", __FUNCTION__, inet_ntoa(server.sin_addr), ntohs(server.sin_port));


}

// thread responsavel por ler os comandos de peers externos conectados no nosso servidor local
void *p2p_client_handler(void *p) {
	struct user *c = (struct user*)p;
	int s = c->socket;
	
	while (1) {
		struct logic_packet raw_pkt;
		if (recv_packet(c->socket, &raw_pkt) < 0) {
			printf("%s [SP2P] Nao foi possivel ler um pacote\n",  __FUNCTION__);
			break;
		}
		
		if (raw_pkt.msg_type == MSG_HAND_SHAKE) {
			// pacote de identificacao do peer, marca como conectado na lista de conexoes e atualiza o celular
			struct packet_hand_shake *pkt = (struct packet_hand_shake*)&raw_pkt.data;
			strcpy(c->celular, pkt->celular);
			c->connected = 1;
			c->port_p2p = pkt->port;
			printf("%s [SP2P] RECV MSG_HAND_SHAKE celular:%s, conexao:%s:%d\n", __FUNCTION__,  c->celular, inet_ntoa(c->ip), c->port_p2p);
		} else if (raw_pkt.msg_type == MSG_TEXT) {
			// pacote de mensagem
			struct packet_text *pkt = (struct packet_text*)&raw_pkt.data;
			printf("%s [SP2P] RECV MSG_TEXT\n", __FUNCTION__);
			printf("%s [SP2P] [MSG] %s: %s\n",  __FUNCTION__, c->celular, pkt->text);
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

	printf("%s [SP2P] Servidor de ClienteP2P %s:%d pronto para receber conexao\n",  __FUNCTION__, inet_ntoa(server.sin_addr), ntohs(server.sin_port));

	while (1) {
		pthread_t t;	    
		int namelen = sizeof(client);
		
		if ((ns = accept(s, (struct sockaddr *)&client, &namelen)) == -1) {
			perror("accept()");
			exit(7);
		}

		printf("%s [SP2P] Recebendo conexao de %s:%d\n", __FUNCTION__,  inet_ntoa(client.sin_addr), ntohs(client.sin_port));

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

    printf("[SP2P] encerrado.\n");
}

int main(int argc, char **argv) {	
	unsigned short port;                
    struct hostent *hostnm; 

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
        fprintf(stderr, "Gethostbyname failed\n");
        exit(2);
    }

	port = (unsigned short)atoi(argv[2]);
	strcpy(g_cliente.celular, argv[3]);

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

			// constroi o pacto de requisicao de conexao a outro peer
			struct packet_query_info qr_pkt;
			strcpy(qr_pkt.celular, celular);
			BUILD_PACKET(packet_query_info, MSG_QUERY_INFO, raw_pkt_send, qr_pkt);

			if (send_packet(g_cliente.sock_central_server, &raw_pkt_send) == -1) {
				perror("send()");
				exit(6);
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

			// busca o celular na lista de conexoes
			pthread_mutex_lock(&lista_conexoes_lock);
			struct user tmp;
			strcpy(tmp.celular,celular);
			struct linked_list_node *node = buscar_lista(&g_cliente.lista_conexoes, cliente_comparador, (void*)&tmp); 
			pthread_mutex_unlock(&lista_conexoes_lock);

			if (node) {
				// celular existe
				struct user *u = (struct user*)node->data;
				printf("Enviando mensagem para %s %s:%d\n", u->celular, inet_ntoa(u->ip), u->port_p2p);

				// constroi o pacote de mensagem a outro peer
				struct packet_text msg_pkt;
				strcpy(msg_pkt.text, msg);
				BUILD_PACKET(packet_text, MSG_TEXT, raw_pkt_send, msg_pkt);

				if (send_packet(u->socket, &raw_pkt_send) == -1) {
					perror("send()");
					exit(6);
				}
			} else {
				// celular nao existe na lista de conexoes
				printf("NAO ENCONTRADO\n");
			}
		} else if (!strcmp(tmp, "sendimg")) {
			// sendimg [CELULAR] [MENSAGEM]: manda uma imagem a um peer conectado
			
		}
	}
	
	return 0;
}




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

#include "networklib.h"
#include "shareddefs.h"
#include "linkedlist.h"

struct contato {
	char celular[20];
	struct socket_info sock_info;
	int online;
};

struct grupo {
	struct linked_list lista_contatos;
};

struct contatos {
	struct linked_list lista_contatos;
	struct linked_list lista_grupos;
};

struct conexao {
	struct contato *contato;
	
};

struct cliente {
	char celular[20];
    struct linked_list lista_conexoes;
    struct socket_info sock_info_central;
};

static struct cliente g_cliente;

int main(int argc, char **argv) {
	
	
	return 0;	
}




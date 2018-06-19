/* 
* linkedlist.h
* REDES A - PROJETO 2: 
* WHATSAP2P
* DIOGO ESTEVES FURTADO 15153927
* LEONARDO RODRIGUES GUISSI 15108244
* VICTOR FERNANDO CESARONI 15593866
*/

struct linked_list_node {
	void *data;
	struct linked_list_node *next;
};

struct linked_list {
	struct linked_list_node *head;
};

void inserir_lista(struct linked_list *lista, void *data) {
	struct linked_list_node **node = &lista->head;
	
	if (*node == NULL) {
		*node = malloc(sizeof(struct linked_list));
		(*node)->data = data;
		(*node)->next = NULL;
		return;
	}
	
	while ((*node)->next) {
		node = &(*node)->next;
	}
	
	(*node)->next = malloc(sizeof(struct linked_list));
	(*node)->next->data = data;
	(*node)->next->next = NULL;
}

void remover_lista(struct linked_list *lista, int(*comparador)(struct linked_list_node*,void*), void *item) {
	struct linked_list_node *node = lista->head;    
	struct linked_list_node *last = NULL;
	
	while (node) {
		if (comparador(node,item)) {
			if (last) {
				last->next = node->next;
			} else {
				lista->head = node->next;
			}
			free(node);          
			break;
		}
		last = node;
		node = node->next;
	}
}

struct linked_list_node* buscar_lista(struct linked_list *lista, int(*comparador)(struct linked_list_node*,void*), void *item) {
	struct linked_list_node *node = lista->head;
	
	while (node) {
		if (comparador(node,item)) {
			return node;
		}
		node = node->next;
	}
	
	return NULL;
}

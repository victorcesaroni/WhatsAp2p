/* 
* networklib.h
* REDES A - PROJETO 2: 
* WHATSAP2P
* DIOGO ESTEVES FURTADO 15153927
* LEONARDO RODRIGUES GUISSI 15108244
* VICTOR FERNANDO CESARONI 15593866
*/

#define NOT_IMPLEMENTED fprintf(stderr, "NOT IMPLEMENTED %s\n", __FUNCTION__); 
#define MAX_PACKET_DATA_SIZE 4096

#define BUILD_PACKET(_TYPE, _MSG_TYPE, _LOGIC_NAME, _DATA) \
	struct logic_packet _LOGIC_NAME; \
	_LOGIC_NAME.size = sizeof(struct _TYPE); \
	_LOGIC_NAME.msg_type = _MSG_TYPE; \
	memcpy(&_LOGIC_NAME.data, &_DATA, sizeof(struct _TYPE));

struct logic_packet {
	int size; // tamanho dos dados validos do campo data
	char msg_type; // tipo da mensagem
	char data[MAX_PACKET_DATA_SIZE];
};

int send_packet(int sock, struct logic_packet *pkt) {
	int total_size = sizeof(struct logic_packet);
	int total_send = 0;
	
	do {
		int s = send(sock, &pkt[total_send], total_size - total_send, 0);
		
		if (s < 0) {
			perror("send()");
			return s;
		}
		
		total_send += s;
	} while (total_send < total_size);
	
	return total_send;
}

int recv_packet(int sock, struct logic_packet *pkt) {
	int total_size = sizeof(struct logic_packet);
	int total_recv = 0;
	
	do {
		int r = recv(sock, &pkt[total_recv], total_size - total_recv, 0);
		
		if (r < 0) {
			perror("recv()");
			return r;
		}
		
		total_recv += r;
	} while (total_recv < total_size);
	
	return total_recv;
}



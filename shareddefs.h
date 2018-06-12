enum MSG_TYPE {
	MSG_HAND_SHAKE,
	MSG_QUERY_INFO,
	MSG_QUERY_RESPONSE,
	MSG_TEXT,
	MSG_FILE_PART,
	MSG_BEGIN_TRANSFER,
};

struct user {
	char celular[20];
	struct in_addr ip;
	unsigned short port;
	unsigned short port_p2p; 
	int socket;
	int connected;
};

struct packet_hand_shake {
	char celular[20];
	unsigned short port;
};

struct packet_begin_transfer {
	char file_name[256];
	unsigned long file_size;
	int file_parts;
};

struct packet_file_part {
	char file_data[MAX_PACKET_DATA_SIZE];
};

struct packet_text {
	char text[MAX_PACKET_DATA_SIZE];
};

struct packet_query_info {
	char celular[20];
};

struct packet_query_response {
	char celular[20];
	struct in_addr ip;
	unsigned short port_p2p;
	int connected;
};

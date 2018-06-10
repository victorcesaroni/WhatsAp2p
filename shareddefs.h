struct socket_info {
    struct sockaddr_in con_info;
    int socket;
    int conectado;
};

struct user {
    char celular[20];
    struct socket_info sock_info;    
};

struct packet_hand_shake {
    char celular[20];
    unsigned short porta;
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
    struct user user;
};



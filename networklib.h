#define NOT_IMPLEMENTED fprintf(stderr, "NOT IMPLEMENTED %s\n", __FUNCTION__); 
#define MAX_PACKET_DATA_SIZE 4096

enum MSG_TYPE {
    MSG_HAND_SHAKE,
    MSG_QUERY_INFO,
    MSG_QUERY_RESPONSE,
    MSG_TEXT,
    MSG_FILE_PART,
};

struct logic_packet {
    int size;
    char msg_type;
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



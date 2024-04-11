#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#define TCP 1
#define UDP 2
#define UDP_R 3

struct __attribute__((__packed__)) conn_t {
    uint8_t type; // 1 for this 
    uint64_t session_id;
    uint8_t protocol_id; // 1 for TCP, 2 for UDP 3 for UDP with retranmission
    uint64_t length; // length of the data
};

struct __attribute__((__packed__)) connacc_t {
    uint8_t type; // 2 for this packet
    uint64_t session_id;
};

struct __attribute__((__packed__)) conrjt_t {
    uint8_t type; // 3 for this packet
    uint64_t session_id;
};

struct __attribute__((__packed__)) data_header_t {
    uint8_t type; // 4 for this packet
    uint64_t session_id;
    uint64_t packet_number;
    uint32_t length;
    // the data will be here
};

struct __attribute__((__packed__)) acc{ // accept data packet
    uint8_t type; // 5 for this packet
    uint64_t session_id;
    uint64_t packet_number;
};

struct __attribute__((__packed__)) rjt_t { // reject data packet
    uint8_t type; // 6 for this packet
    uint64_t session_id;
    uint64_t packet_number;
};

struct __attribute__((__packed__)) rcvd_t { // received all data
    uint8_t type; // 7 for this packet
    uint64_t session_id;
};

struct session_info_t {
    uint64_t session_id;
    uint8_t protocol;
    uint64_t packet_number;
    uint64_t length;
};

typedef struct conn_t conn;
typedef struct connacc_t connacc;
typedef struct conrjt_t conrjt;
typedef struct data_header_t data_header;
typedef struct acc acc;
typedef struct rjt_t rjt;
typedef struct rcvd_t rcvd;
typedef struct session_info_t session_info;

void common_hello(void);
struct sockaddr_in get_server_address(char const *host, uint16_t port);
uint16_t read_port(char const *string);
size_t   read_size(char const *string);
ssize_t	readn(int fd, void *vptr, size_t n);
ssize_t	writen(int fd, const void *vptr, size_t n);

int send_conn_tcp(int fd, uint64_t session_id, uint8_t protocol, uint64_t length);
int send_connacc_tcp(int fd, uint64_t session_id);
int send_conrjt_tcp(int fd, uint64_t session_id);
int send_rjt_tcp(int fd, uint64_t session_id, uint64_t packet_number);
int send_rcvd_tcp(int fd, uint64_t session_id);

int receive_conn_tcp(int fd, session_info *info);
int receive_connacc_tcp(int fd, session_info *session_id);

int send_conn_udp(int fd, uint64_t session_id, uint64_t length, struct sockaddr_in *address);
int send_connacc_udp(int fd, uint64_t session_id, struct sockaddr_in *client_address);
int send_conrjt_udp(int fd, uint64_t session_id, struct sockaddr_in *client_address);
int send_rjt_udp(int fd, uint64_t session_id, uint64_t packet_number, struct sockaddr_in *client_address);
int send_rcvd_udp(int fd, uint64_t session_id, struct sockaddr_in *client_address);

// this function only recognizes the type of the packet and the session_id so it DOES NOT CHANGE BYTES ORDER
int receive_datagram_udp(int fd, struct sockaddr_in *client_address, void *buffer, ssize_t buff_len,
                         uint8_t* type, uint64_t* session_id, socklen_t* addr_len);
#endif
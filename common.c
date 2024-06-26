#include "err.h"
#include "common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>


uint16_t read_port(char const *string) {
    char *endptr;
    unsigned long port = strtoul(string, &endptr, 10);
    if ((port == ULONG_MAX && errno == ERANGE) || *endptr != 0 || port == 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

size_t read_size(char const *string) {
    char *endptr;
    unsigned long long number = strtoull(string, &endptr, 10);
    if ((number == ULLONG_MAX && errno == ERANGE) || *endptr != 0 || number > SIZE_MAX) {
        fatal("%s is not a valid number", string);
    }
    return number;
}

struct sockaddr_in get_server_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;   // IPv4
    send_address.sin_addr.s_addr =       // IP address
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

// Following two functions come from Stevens' "UNIX Network Programming" book.

// Read n bytes from a descriptor. Use in place of read() when fd is a stream socket.
ssize_t readn(int fd, void *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF

        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;         // return >= 0
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n){
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = vptr;               // Can't do pointer arithmetic on void*.
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

int sessid_gen(){
    int dummy;
    ASSERT_SYS_OK(getrandom(&dummy, sizeof(dummy), 0));
    return dummy;
}

void set_timeout(int fd, int sec, int usec) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        syserr("setsockopt");
    }
    if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0){
        syserr("setsockopt");
    }
}

int create_bind_udp(uint16_t port) {
    // Create a socket.
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // Bind the socket to a concrete address.
    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = htonl(INADDR_ANY);
    client_address.sin_port = htons(port);
    if (bind(socket_fd, (struct sockaddr *) &client_address, sizeof(client_address)) < 0) {
        syserr("bind");
    }

    return socket_fd;
}

int send_conn_tcp(int fd, uint64_t session_id, uint8_t protocol, uint64_t length) {
    conn c = {1, session_id, protocol, htobe64(length)};
    return writen(fd, &c, sizeof(c));
}

int send_connacc_tcp(int fd, uint64_t session_id) {
    connacc c = {2, session_id};
    return writen(fd, &c, sizeof(c));
}

int send_conrjt_tcp(int fd, uint64_t session_id) {
    conrjt c = {3, session_id};
    return writen(fd, &c, sizeof(c));
}

int send_rjt_tcp(int fd, uint64_t session_id, uint64_t packet_number){
    rjt r = {6, session_id, htobe64(packet_number)};
    return writen(fd, &r, sizeof(r));
}

int send_rcvd_tcp(int fd, uint64_t session_id){
    rcvd r = {7, session_id};
    return writen(fd, &r, sizeof(r));
}

int receive_conn_tcp(int fd, session_info *info){
    conn c;
    ssize_t read_length = readn(fd, &c, sizeof(c));
    if(read_length < 0){
        perror("readn");
    }
    else if(read_length == 0){
        fatal("connection closed");
    }

    if(c.type != 1){
        fatal("expected conn packet");
    }
    info->session_id = c.session_id;
    info->protocol = c.protocol_id;
    info->packet_number = 0;
    info->length = be64toh(c.length);
    return 0;
}


int receive_connacc_tcp(int fd, session_info *session_id){
    connacc c;
    ssize_t read_length = readn(fd, &c, sizeof(c));
    if(read_length < 0){
        perror("readn");
    }
    else if(read_length == 0){
        fatal("connection closed");
    }

    if(c.type != 2){
        fatal("expected connacc packet");
    }
    session_id->session_id = c.session_id;
    return 0;
}

// TODO: Implement the following functions:
// int receive_conrjt(int fd, uint64_t session_id);
// int receive_rjt(int fd, uint64_t session_id, uint64_t packet_number);


// UDP functions

int send_conn_udp(int fd, uint64_t session_id, uint64_t length, struct sockaddr_in *client_address, int protocol_id){
    conn c = {1, session_id, protocol_id, htobe64(length)};
    int flags = 0;
    ssize_t sent;
    sent = sendto(fd, &c, sizeof(c), flags, (struct sockaddr *) client_address, sizeof(*client_address));
    if(sent < 0){
        syserr("sendto");
    }
    return 0;
}

int send_connacc_udp(int fd, uint64_t session_id, struct sockaddr_in *client_address){
    connacc c = {2, session_id};
    int flags = 0;
    ssize_t sent;
    sent = sendto(fd, &c, sizeof(c), flags, (struct sockaddr *) client_address, sizeof(*client_address));
    if(sent < 0){
        syserr("sendto");
    }
    return 0;
}

int send_conrjt_udp(int fd, uint64_t session_id, struct sockaddr_in *client_address){
    conrjt c = {3, session_id};
    int flags = 0;
    ssize_t sent;
    sent = sendto(fd, &c, sizeof(c), flags, (struct sockaddr *) client_address, sizeof(*client_address));
    if(sent < 0){
        syserr("sendto");
    }
    return 0;
}

int send_rjt_udp(int fd, uint64_t session_id, uint64_t packet_number, struct sockaddr_in *client_address){
    rjt r = {6, session_id, htobe64(packet_number)};
    int flags = 0;
    ssize_t sent;
    sent = sendto(fd, &r, sizeof(r), flags, (struct sockaddr *) client_address, sizeof(*client_address));
    if(sent < 0){
        syserr("sendto");
    }
    return 0;
}

int send_rcvd_udp(int fd, uint64_t session_id, struct sockaddr_in *client_address){
    rcvd r = {7, session_id};
    int flags = 0;
    ssize_t sent;
    sent = sendto(fd, &r, sizeof(r), flags, (struct sockaddr *) client_address, sizeof(*client_address));
    if(sent < 0){
        syserr("sendto");
    }
    return 0;
}

int send_acc_udp(int fd, uint64_t session_id, uint64_t packet_number, struct sockaddr_in *client_address){
    acc a = {5, session_id, htobe64(packet_number)};
    int flags = 0;
    ssize_t sent;
    sent = sendto(fd, &a, sizeof(a), flags, (struct sockaddr *) client_address, sizeof(*client_address));
    if(sent < 0){
        syserr("sendto");
    }
    return 0;
}

/* this function DOES NOT CHANGE BYTES ORDER */
int receive_datagram_udp(int fd, struct sockaddr_in *client_address,
                         void *buffer,ssize_t buffer_len,  uint8_t* type, uint64_t* session_id, socklen_t* addr_len){
    ssize_t received;
    int flags = 0;
    received = recvfrom(fd, buffer, buffer_len, flags, 
                        (struct sockaddr *) client_address, addr_len);
    if(received < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
        syserr("recvfrom");
    }
    else if(received == -1){ // errno == EAGAIN || errno == EWOULDBLOCK this means timeout
        return -1;
    }
    *type = ((uint8_t*) buffer)[0];
    *session_id = *((uint64_t *) (buffer + 1));
    return 0;
}
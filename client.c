#include "err.h"
#include "common.h"
#include "protconst.h"

#include <endian.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
// #include <cstdlib>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#define DATA_SIZE 10
uint8_t send_buffer[DATA_SIZE + sizeof(data_header)];
// socat UDP4-RECVFROM:4444,fork UDP4-SENDTO:students.mimuw.edu.pl:49910
// 94.172.161.47

void tcp_protocol(int port, char const *host, uint64_t length, void* data) {
    struct sockaddr_in server_address = get_server_address(host, port);

    // Create a socket.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // Connect to the server.
    if (connect(socket_fd, (struct sockaddr *) &server_address,
                (socklen_t) sizeof(server_address)) < 0) {
        syserr("cannot connect to the server");
    }
    // TODO terminate process if connection is not established

    // ESTABLIH PPCB CONNECTION 
    session_info session;
    session.protocol = 1;
    session.session_id = rand();
    session.packet_number = 0;
    session.length = length;

    // Send a CONN message to the server.
    send_conn_tcp(socket_fd, session.session_id, session.protocol, session.length);

    // Receive a REC message from the server.
    session_info session_rec;
    receive_connacc_tcp(socket_fd, &session_rec); // TCP server never rejects connection
    assert(session_rec.session_id == session.session_id);
    printf("Client: Connection established\n");

    // Send data
    data_header header;
    uint64_t bytes_left = length;
    while (bytes_left > 0)
    {
        // min of DATA_SIZE and bytes_left
        uint32_t bytes_to_send = bytes_left < DATA_SIZE ? bytes_left : DATA_SIZE;
        header.type = 4;
        header.session_id = session.session_id;
        header.packet_number = htobe64(session.packet_number);
        header.length = htobe32(bytes_to_send);

        // send header
        writen(socket_fd, &header, sizeof(header));

        // send data
        uint64_t offset = length - bytes_left;
        writen(socket_fd, data + offset, bytes_to_send);

        bytes_left -= bytes_to_send;
        session.packet_number++;
    }
    

    close(socket_fd);
}

void udp_protocol(int port, char const *host, uint64_t length, void* data){
    struct sockaddr_in server_address = get_server_address(host, port);

    // Create a socket.
    int socket_fd = create_bind_udp(0);

    // Send a CONN message to the server.
    session_info session;
    session.protocol = 2;
    session.session_id = rand();
    session.packet_number = 0;
    session.length = length;

    send_conn_udp(socket_fd, session.session_id, length, &server_address);

    // Receive a CONNACC message from the server.
    connacc c;
    uint8_t rec_type;
    uint64_t rec_session_id;
    receive_datagram_udp(socket_fd, NULL, &c, sizeof(c), &rec_type, &rec_session_id, NULL);
    if(rec_type != 2){
        fatal("Client: unable to connect\n");
    }
    assert(rec_session_id == session.session_id);
    printf("Client: Connection established\n");

    // Send data
    data_header header;
    uint64_t bytes_left = length;
    while (bytes_left > 0)
    {
        // min of DATA_SIZE and bytes_left
        uint32_t bytes_to_send = bytes_left < DATA_SIZE ? bytes_left : DATA_SIZE;
        header.type = 4;
        header.session_id = session.session_id;
        header.packet_number = htobe64(session.packet_number);
        header.length = htobe32(bytes_to_send);

        // prep buffer
        memcpy(send_buffer, &header, sizeof(header));
        uint64_t offset = length - bytes_left;
        memcpy(send_buffer + sizeof(header), data + offset, bytes_to_send);

        // send data
        ssize_t sent;
        int flags = 0;
        sent = sendto(socket_fd, send_buffer, sizeof(header) + bytes_to_send, flags,
                      (struct sockaddr *) &server_address, sizeof(server_address));
        if (sent < 0) {
            syserr("sendto");
        }
        bytes_left -= bytes_to_send;
        session.packet_number++;
    }

    // Receive a RCVD message from the server.
    rcvd r;
    receive_datagram_udp(socket_fd, NULL, &r, sizeof(r), &rec_type, &rec_session_id, NULL);
    if(rec_type == 7 && rec_session_id == session.session_id){
        printf("Client: All data received\n");
    }
    else{
        fatal("Client: Unexpected message received\n");
    }
}

void udpr_protocol(int port, char const *host, uint64_t length, void* data){
    struct sockaddr_in server_address = get_server_address(host, port);

    // Create a socket.
    int socket_fd = create_bind_udp(0);

    // Send a CONN message to the server.
    session_info session;
    session.protocol = 3;
    session.session_id = rand();
    session.packet_number = 0;
    session.length = length;

    send_conn_udp(socket_fd, session.session_id, length, &server_address);

    // Receive a CONNACC message from the server.
    connacc c;
    uint8_t rec_type;
    uint64_t rec_session_id;
    receive_datagram_udp(socket_fd, NULL, &c, sizeof(c), &rec_type, &rec_session_id, NULL);
    if(rec_type != 2){
        fatal("Client: unable to connect\n");
    }
    assert(rec_session_id == session.session_id);
    printf("Client: Connection established\n");

    // Send data
    data_header header;
    uint64_t bytes_left = length;
    while (bytes_left > 0)
    {
        // min of DATA_SIZE and bytes_left
        uint32_t bytes_to_send = bytes_left < DATA_SIZE ? bytes_left : DATA_SIZE;
        header.type = 4;
        header.session_id = session.session_id;
        header.packet_number = htobe64(session.packet_number);
        header.length = htobe32(bytes_to_send);

        // prep buffer
        memcpy(send_buffer, &header, sizeof(header));
        uint64_t offset = length - bytes_left;
        memcpy(send_buffer + sizeof(header), data + offset, bytes_to_send);

        int rcv_code;
        int retransmissions = 0;
        do{
            // send data
            ssize_t sent;
            int flags = 0;
            sent = sendto(socket_fd, send_buffer, sizeof(header) + bytes_to_send, flags,
                        (struct sockaddr *) &server_address, sizeof(server_address));
            if (sent < 0) {
                syserr("sendto");
            }

            // Receive a ACC message from the server.
            // we are waiting MAX_WAIT seconds for the server to accept the packet
            acc a;
            set_timeout(socket_fd, MAX_WAIT, 0);
            rcv_code = receive_datagram_udp(socket_fd, NULL, &a, sizeof(a), &rec_type, &rec_session_id, NULL);
            if(rec_type == 5 
                && rec_session_id == session.session_id 
                && a.packet_number == session.packet_number
                && rcv_code == 0){
                printf("Client: Packet accepted\n");
            }
        } while(rcv_code == -1 && retransmissions++ < MAX_RETRANSMITS);

        // we were unable to send the packet
        if(rcv_code == -1){
            fatal("Client: server doesn't respond\n");
        }

        retransmissions = 0;
        bytes_left -= bytes_to_send;
        session.packet_number++;
    }

}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fatal("usage: %s <protocol> <host> <port>\n", argv[0]);
    }

    char const *host = argv[2];
    uint16_t port = read_port(argv[3]);
    uint8_t protocol;

    char *data = NULL;
    uint64_t length = 0;
    ASSERT_SYS_OK(getline(&data, &length, stdin));
    printf("data: %s\n", data);
    length = strlen(data);
    printf("length: %lu\n", length);
    
    if (strcmp(argv[1], "tcp") == 0) {
        protocol = 1;
    } else if (strcmp(argv[1], "udp") == 0) {
        protocol = 2;
    } else if (strcmp(argv[1], "udpr") == 0) {
        protocol = 3;
    } else {
        fatal("protocol must be one of: tcp, udp, udpr\n");
    }
    srand(time(NULL)); 

    if(protocol == 1){
        tcp_protocol(port, host, length, data);
    } 
    else if(protocol == 2){
        udp_protocol(port, host, length, data);
    }
    else if(protocol == 3){
        udpr_protocol(port, host, length, data);
    }

    free(data);

    return 0;
}
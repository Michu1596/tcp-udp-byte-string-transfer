#include "err.h"
#include "common.h"
#include "protconst.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <endian.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define QUEUE_LENGTH 0
#define BUFFER_SIZE 100000
uint8_t buffer[BUFFER_SIZE];


int udp_set_session(int socket_fd, session_info *session, struct sockaddr_in *client_address){
        // reset timeouts
        set_timeout(socket_fd, 0, 0);

        // Start PPCB session
        // printf("server is waiting for CONN at port %d\n", port);
        
        socklen_t client_address_len = (socklen_t) sizeof(*client_address);

        // receive CONN
        conn conn_package;
        ssize_t recived_length;
        do{
            recived_length = recvfrom(socket_fd, &conn_package,
                                    sizeof(conn_package), 0, (struct sockaddr *) client_address, &client_address_len);
            if(recived_length < 0){
                syserr("recvfrom");
            }
            // printf("server recivied data from addresss %s:%d\n",
            //  inet_ntoa(client_address->sin_addr), ntohs(client_address->sin_port));
        } while(recived_length != sizeof(conn) || conn_package.type != 1 
            || (conn_package.protocol_id != 2 && conn_package.protocol_id != 3)); // trying till success
    

        // set session
        session->session_id = conn_package.session_id;
        session->protocol = conn_package.protocol_id;
        session->length = be64toh(conn_package.length); // TODO check if it is correct
        session->packet_number = 0;


        // printf("received session info: session_id = %" PRIu64 ", length = %" PRIu64 ", packet_number = %" PRIu64 "\n",
            //    session->session_id, session->length, session->packet_number);

        // send CONNACC
        send_connacc_udp(socket_fd, session->session_id, client_address);
        return 0;
}


void tcp_protocl(int port){

    // Ignore SIGPIPE signals, so they are delivered as normal errors.
    signal(SIGPIPE, SIG_IGN); 

    // Create a socket.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    // Bind the socket to a concrete address.
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
        syserr("bind");
    }

    // Switch the socket to listening. We listen for incoming connections.
    if (listen(socket_fd, QUEUE_LENGTH) < 0) {
        syserr("listen");
    }

    for (;;) {
        struct sockaddr_in client_address;

        // Accept a new connection.
        int client_fd = accept(socket_fd, (struct sockaddr *) &client_address,
                               &((socklen_t){sizeof(client_address)}));
        if (client_fd < 0) {
            syserr("accept");
        }

        // char const *client_ip = inet_ntoa(client_address.sin_addr);
        // uint16_t client_port = ntohs(client_address.sin_port);
        // printf("accepted connection from %s:%" PRIu16 "\n", client_ip, client_port);

        // Set timeouts for the client socket.
        struct timeval to = {.tv_sec = MAX_WAIT, .tv_usec = 0};
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof to); // possibly not needed

        // receive session info
        session_info session;
        receive_conn_tcp(client_fd, &session);
        // printf("received session info: session_id = %" PRIu64 ", protocol = %" PRIu8 ", packet_number = %" PRIu64 "\n",
            //    session.session_id, session.protocol, session.packet_number);

        // send connacc
        send_connacc_tcp(client_fd, session.session_id);

        // receive data

        data_header data_package_header;
        uint64_t bytes_left = session.length;
        do{    
            // read package type
            ASSERT_SYS_OK(readn(client_fd, &data_package_header.type, sizeof data_package_header.type));
            if(data_package_header.type != 4){
                // printf("wrong package type\n");
                send_rjt_tcp(client_fd, session.session_id, session.packet_number);
                break;
            }
            // read package session_id
            ASSERT_SYS_OK(readn(client_fd, &data_package_header.session_id, sizeof data_package_header.session_id));
            if(data_package_header.session_id != session.session_id){
                // printf("wrong session_id\n");
                send_rjt_tcp(client_fd, session.session_id, session.packet_number);
                break;
            }
            // read package packet_number
            ASSERT_SYS_OK(readn(client_fd, &data_package_header.packet_number, sizeof data_package_header.packet_number));
            data_package_header.packet_number = be64toh(data_package_header.packet_number);
            if(data_package_header.packet_number != session.packet_number){
                // printf("wrong packet_number %ld a chcicialem %ld\n", data_package_header.packet_number, session.packet_number);
                send_rjt_tcp(client_fd, session.session_id, session.packet_number);
                break;
            }
            // read package length
            ASSERT_SYS_OK(readn(client_fd, &data_package_header.length, sizeof data_package_header.length));
            data_package_header.length = be32toh(data_package_header.length);
            bytes_left -= data_package_header.length;
            // read data
            void *data = malloc(sizeof(unsigned char) * data_package_header.length);
            if(data == NULL){
                fatal("malloc");
            }
            ASSERT_SYS_OK(readn(client_fd, data, data_package_header.length));
            // print to std out
            for(uint32_t i = 0; i < data_package_header.length; i++){
                printf("%c", ((unsigned char *)data)[i]);
            }
            session.packet_number++;
        }
        while (bytes_left != 0);
        send_rcvd_tcp(client_fd, session.session_id);
        ASSERT_SYS_OK(fflush(stdout)); // flush stdout
        

        // printf("finished serving %s:%" PRIu16 "\n", client_ip, client_port);
        close(client_fd);
    }

    close(socket_fd);
}

void udp_protocol(int socket_fd, session_info session){
    // set timeouts
    set_timeout(socket_fd, MAX_WAIT, 0);

    // receive data
    uint64_t bytes_left = session.length;
    uint64_t received_session_id;
    uint8_t received_type;
    struct sockaddr_in received_client_address;
    socklen_t received_addr_len = sizeof(received_client_address);
    int ok = 1;

    while (bytes_left > 0)
    {
        // printf("waiting for package %ld\n", session.packet_number);
        ASSERT_SYS_OK(receive_datagram_udp(socket_fd, &received_client_address, buffer, BUFFER_SIZE,
                             &received_type, &received_session_id, &received_addr_len));
        if (received_type == 4 && received_session_id == session.session_id)
        {
            data_header *header = (data_header *)buffer;
            header->packet_number = be64toh(header->packet_number);
            header->length = be32toh(header->length);
            // printf("received data: packet_number = %" PRIu64 ", length = %" PRIu32 "\n",
            //  header->packet_number, header->length);

            if (header->packet_number == session.packet_number)
            {
                // print to std out
                for (uint32_t i = 0; i < header->length; i++)
                {
                    printf("%c", ((unsigned char *)buffer + sizeof(data_header))[i]);
                }
                bytes_left -= header->length;
                session.packet_number++;
            }
            else
            {
                send_rjt_udp(socket_fd, session.session_id, session.packet_number, &received_client_address);
                ok = 0;
                break;
            }
        }
        else if (received_type == 1)
        {
            send_conrjt_udp(socket_fd, received_session_id, &received_client_address);
        }
        // we are just ignoring other types of packages
    }
    if (ok)
    {
        send_rcvd_udp(socket_fd, session.session_id, &received_client_address);
    }
    fflush(stdout); // flush stdout
}


int udpr_protocol(int socket_fd, session_info session){

    // set timeouts
    set_timeout(socket_fd, MAX_WAIT, 0);

    // receive data
    uint64_t bytes_left = session.length;
    uint64_t received_session_id;
    uint8_t received_type;
    struct sockaddr_in received_client_address; // TODO use it as client adress
    socklen_t received_addr_len = sizeof(received_client_address);

    int rcv_code = 0;
    int retransmissions = 0;
    while (bytes_left > 0)
    {
        do
        {
            // we are sendind ACC package but not if it is the first package
            if (rcv_code == -1 && session.packet_number > 0)
            {
                send_acc_udp(socket_fd, session.session_id, session.packet_number - 1, &received_client_address);
            }
            set_timeout(socket_fd, MAX_WAIT, 0);
            // fprintf(stderr, "waiting for package %ld\n", session.packet_number);
            rcv_code = receive_datagram_udp(socket_fd, &received_client_address, buffer, BUFFER_SIZE,
                                            &received_type, &received_session_id, &received_addr_len);
            // fprintf(stderr, "received package %ld with rcv_code = %d\n", session.packet_number, rcv_code);
            // fprintf(stderr, "received package %ld with type = %d and session_id = %ld\n", session.packet_number, received_type, received_session_id);
            // reading rest of the header
            data_header *header = (data_header *)buffer;
            header->packet_number = be64toh(header->packet_number); 
            header->length = be32toh(header->length);

            // package is correct
            if (received_type == 4 && received_session_id == session.session_id && header->packet_number == session.packet_number)
            {

                // printf("received data: packet_number = %" PRIu64 ", length = %" PRIu32 "\n",
                // header->packet_number, header->length);

                // reading actual data from a package
                // print to std out
                for (uint32_t i = 0; i < header->length; i++)
                {
                    printf("%c", ((unsigned char *)buffer + sizeof(data_header))[i]);
                }
                bytes_left -= header->length;

                // we send ACC package
                send_acc_udp(socket_fd, session.session_id, session.packet_number, &received_client_address);

                session.packet_number++;
            }
            // package asks for connection
            else if (received_type == 1)
            {
                send_conrjt_udp(socket_fd, received_session_id, &received_client_address);
            }
            // we retranmit ACC package i we encounter the same package again
            else if (session.session_id == received_session_id && received_type == 4)
            {
                rcv_code = -1;
            }
            // we are just ignoring other types of packages
        } while (rcv_code == -1 && retransmissions++ < MAX_RETRANSMITS);

        // we are terminating the connection if retransmissions limit is reached
        if (rcv_code == -1)
        {
            break;
        }

        retransmissions = 0;
    }
    if (bytes_left == 0 && rcv_code != -1)
        send_rcvd_udp(socket_fd, session.session_id, &received_client_address);
    else
        fprintf(stderr, "ERROR: Client doesn't respond\n");
    ASSERT_SYS_OK(fflush(stdout)); // flush stdout
    return 0;
}


void udp_start(int port){
    int socket_fd = create_bind_udp(port);

    // client service
    for(;;){
        // set PCCB session
        struct sockaddr_in client_address;
        session_info session;
        udp_set_session(socket_fd, &session, &client_address);
        
        if (session.protocol == 2){
            udp_protocol(socket_fd, session);
        }
        else if (session.protocol == 3){
            udpr_protocol(socket_fd, session);
        }
        else{
            fprintf(stderr, "ERROR: wrong protocol\n");
        }
    }
    close(socket_fd);
}


int main(int argc, char *argv[]) {

    if (argc != 3) {
        fatal("usage: %s <protocol> <port>", argv[0]);
    }

    uint16_t port = read_port(argv[2]);
    uint8_t protocol;
    if(strcmp(argv[1], "tcp") == 0){
        protocol = 1;
    } else if(strcmp(argv[1], "udp") == 0){
        protocol = 2;
    } else {
        fatal("protocol is 'tcp' or 'udp'");
    }

    if(protocol == 1){
        tcp_protocl(port);
    }
    else if(protocol == 2){
        udp_start(port);
    }
    
    return 0;
}
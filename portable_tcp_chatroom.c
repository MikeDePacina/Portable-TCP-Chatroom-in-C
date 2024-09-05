#include "portability_config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int main(){
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);


    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    //bind socket so it goes to listening state
    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    //create socket set to input incoming socket connections
    //initially our listening socket/server socket is biggest/max socket
    fd_set allsockets;
    FD_ZERO(&allsockets);
    FD_SET(socket_listen, &allsockets);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while(1) {
        fd_set reads;
        reads = allsockets;
        if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

    

    //iterate through sockets, and check if socket is ready
    SOCKET i;
    for(i = 1; i <= max_socket; ++i) {
        if (FD_ISSET(i, &reads)) {
                //Handle socket
            //check if the ready socket is the server socket that's listening for connections
            //if it is we accept it, update max_socket if needed, then print new connection
            //info
            if (i == socket_listen) {
                struct sockaddr_storage client_address;
                socklen_t client_len = sizeof(client_address);
                SOCKET socket_client = accept(socket_listen,
                            (struct sockaddr*) &client_address,
                            &client_len);
                if (!ISVALIDSOCKET(socket_client)) {
                    fprintf(stderr, "accept() failed. (%d)\n",
                    GETSOCKETERRNO());
                    return 1;
                }

                    FD_SET(socket_client, &allsockets);
                    if (socket_client > max_socket)
                        max_socket = socket_client;

                    char address_buffer[100];
                    getnameinfo((struct sockaddr*)&client_address,
                            client_len,
                            address_buffer, sizeof(address_buffer), 0, 0,
                            NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);
                } else {
                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);
                    //if client disconnects, we remove it from the set, and close the socket
                    if (bytes_received < 1) {
                        FD_CLR(i, &allsockets);
                        CLOSESOCKET(i);
                        continue;
                    }

                    SOCKET j;
                    for (j = 1; j <= max_socket; ++j){
                        if(FD_ISSET(j, &allsockets)){
                            //if not listening socket/server socket and not the socket we just read data
                            //from then send the socket
                            if(j == socket_listen || j == i)
                                continue;
                            else
                                send(j, read, bytes_received,0);
                        }
                    }
                        
                }
       
            }
        }
    }

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    #if defined(_WIN32)
        WSACleanup();
    #endif

    printf("Finished.\n");

    return 0;
}

#include "portability_config.h"
#include <stdio.h>
#include <string.h>
//conio.h needed for _kbhit() to check if terminal input is waiting

#if defined(_WIN32)
#include <conio.h>
#endif

int main(int argc, char *argv[]){
    //initialize Winsocket
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif
    
    //we take in 3 arguments in cli
    if (argc < 3) {
        fprintf(stderr, "usage: tcp_client hostname port\n");
        return 1;
    }

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    //we didn't set ai_family to IPv4 or IPv6 so getaddrinfo() can decide for itself
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;

    //getaddrinfo can take domain name(website link) or ip address for hostname
    //and for port can take port number or protocol like http
    if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    //print out remote address for log/debugging purposes
    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
            address_buffer, sizeof(address_buffer),
            service_buffer, sizeof(service_buffer),
            NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);


    //create socket
    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family,
            peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    //connect to remote server
    printf("Connecting...\n");
    if (connect(socket_peer,
                peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(peer_address);

    printf("Connected.\n");
    printf("To send data, enter text followed by enter.\n");

    //now program needs to loop while checking for both terminal and
    //socket for new data If new data comes from the terminal, we 
    //send it over the socket. If new data is read from the socket,
    // we print it out to the terminal.

    while(1) {

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
        /*
        For non Windows, we use select to monitor terminal input, stdin is 
        added to reads set/0 is file descriptor for stdin,
        you can also do fileno(stdin) so it's more explicit

        For windows you can't use select for terminal input, so
        we set a timeout of 100ms. If there's no activity, select returns
        and we can check terminal for input manually
        */
        #if !defined(_WIN32)
            FD_SET(0, &reads);
        #endif

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(socket_peer+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        //manually check if socket is ready for read

        if (FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if (bytes_received < 1) {
                printf("Connection closed by peer.\n");
                break;
            }
            printf("Received (%d bytes): %.*s",
                    bytes_received, bytes_received, read);
        }

        /*check for terminal input
        On Windows, _kbhit() to indicate whether any console
        input is waiting. It returns non zero if an unhandled key
        press event is queued up

        For Unix, we just check if 0 or stdin is set

        if input is ready, we read the next line of input with
        fgets(), then we send to socket using send()

        On Windows, if user presses a non printable key, like an 
        arrow key, it still triggers _kbhit(), even though
        there is no char to read. Also, our program will block
        on fgets() until user presses Enter key. Also piped in cli
        would be allowed in Unix but now Windows. We would have to use
        PeekNamedPipe() and PeekConsoleInput(), etc.

        */
        #if defined(_WIN32)
            if(_kbhit()) {
        #else
            if(FD_ISSET(0, &reads)) {
        #endif
            char read[4096];
            if (!fgets(read, 4096, stdin)) break;
            printf("Sending: %s", read);
            int bytes_sent = send(socket_peer, read, strlen(read), 0);
            printf("Sent %d bytes.\n", bytes_sent);
            }
        
    }

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

    #if defined(_WIN32)
        WSACleanup();
    #endif

    printf("Finished.\n");

    return 0;
}


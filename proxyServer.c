#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lru_cache.h"
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/socket.h> // contains socket related functions
#include <sys/types.h> // used for defining datatypes used in system call
#include <netinet/in.h> // define structure like sockaddr
#include <netdb.h>
#include <arpa/inet.h> // provide functions for manipulating numeric IP addresses
#include <unistd.h> // providing close() function for closing socket
#include <fcntl.h>
#include <sys/wait.h>



// struct sockaddr {

//     sa_len;  // total len
//     sa_family_t sa_family; // address family, AF_INET for ipv4 and AF_INET6 for ipv6
//     char sa_data[14]; // protocol specific data

// };

#define MAX_CLIENTS 10
#define MAX_BYTES 4096 

int port_number = 8080;
int proxy_socketID; // my proxy web server socket
pthread_t tid[MAX_CLIENTS]; //  each element representing a unique thread in my program  
sem_t semaphore;  // basically a coun ter type of lock
pthread_mutex_t lock; // mutex available value is only 0 or 1. It is a binary lock

int handle_request(int clientSocketId, ParsedRequest *request, char* tempReq){



}


void* thread_fn(void *newSocket){

    sem_wait(&semaphore);
    int currSemValue;
    sem_getvalue(&semaphore, currSemValue);
    printf("Current semaphore value is: %d\n", currSemValue);

    int *temp = (int *) newSocket;
    int clientSocketId = *temp;

    int bytes_client_send, len;

    char* buffer = (char*) calloc(MAX_BYTES, sizeof(char));

    bytes_client_send = recv(clientSocketId, buffer, MAX_BYTES, 0);

    while(bytes_client_send > 0){
        
        len = strlen(buffer);

        if(strstr(buffer, "\r\n\r\n") == NULL){

            bytes_client_send = recv(clientSocketId, buffer + len, MAX_BYTES - len, 0);
            if(bytes_client_send < 0){
                perror("Error receiving request from client\n");
                free(buffer);
                return;
            }

        }
        else{

            break;

        }
    }

    // now we have our cliet request in our buffer, we have to search it in cache

    char* tempReq = (char*) malloc((strlen(buffer)) * sizeof(char));
    
    strcpy(tempReq, buffer);

    printf("Client request is: %s\n", tempReq);

    cache_element* temp = find_in_cache(tempReq);

    if(temp != NULL){

        int size = temp -> len;
        int pos = 0;
        char response[MAX_BYTES];

        while(pos < size){

            bzero(response, MAX_BYTES);
            
            int chunk_size = (size - pos < MAX_BYTES) ? (size - pos) : MAX_BYTES;

            memcpy(response, temp -> data + pos, chunk_size);

            if(send(clientSocketId, response, chunk_size, 0) < 0){
                perror("Failed to send data\n");
                break;
            }

            pos += chunk_size;


        }
        printf("Data retrieved from the cache\n");
        printf("Response: %s\n", response);

    }
    else if(bytes_client_send > 0){

        len = strlen(buffer);
        ParsedRequest *request = ParsedRequest_create();

        if(ParsedRequest_parse(request, buffer, len) < 0){
            perror("Unable to parse the request\n");   
        }
        else{
            bzero(buffer, MAX_BYTES);
            if(!strcmp(request -> method, "GET")){

                if(request -> host && request -> path && checkHTTPversion(request -> version) == 1){

                    bytes_client_send = handle_request(clientSocketId, request, tempReq);

                    if(bytes_client_send == -1){
                        sendErrorCustom(clientSocketId, 500);
                    }

                }
                else{
                    sendErrorCustom(clientSocketId, 500);
                }

            }
            else{
                printf("We currently fullfill GET requests only\n ")
            }
        }
        ParsedRequest_destroy(request);
    }
    else if(bytes_client_send == 0){
        printf("Client is disconnected\n")
    }

    shutdown(clientSocketId, SHUT_RDWR);
    close(clientSocketId);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore, currSemValue);
    printf("Current semphore value is %d\n", currSemValue);
    free(tempReq);

}

int main(int argc, char* argv){ // (type: int, argc is argument count that represents the command line arguments including the program name, if argc is 1 means no additional just the program name, greater than 0 means additional, now argv stands for argument vector, we can access by argv[0]...)

    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;
    sem_init(&semaphore, 0, MAX_CLIENTS);  // initializing (var_addr, min value, max value)
    pthread_mutex_init(&lock, NULL);


    // checking is user passed the port number or not
    if(argc == 2){

        port_number = agrv[1];

    }
    else{

        printf("Too few arguments, try passing port number\n");
        exit(1);

    }


    printf("\nStarting Proxy Server at port: %d\n", port_number);

    // setting up proxy server socket 
    // int socketId = socket(int domain, int type, int protocol) (return [int],  a file descriptor , non negative is success, if fails then -1)
    // domain like AF_INET for IPv4 and AF_INET6 for IPv6
    // type to determine TCP or UDP (SOCK_STREAM for tcp and SOCK_DGRAM for udp)
    // protocol -> specifies the protocol to be used within the selected domain and type, pass 0 for default and system will choose itself (IPPROTO_TCP for tcp connection)

    proxy_socketID = socket(AF_INET, SOCK_STREAM, 0);

    if(proxy_socketID < 0){
        perror("Failed to create proxy socket");
        exit(1);
    }

    int reuse = 1;

    // When a server program closes, the operating system doesn’t immediately free up the port number the server was using. Instead, the port might be placed in a state called TIME_WAIT for a short period (usually a few minutes). This state is used to ensure that any delayed packets related to the closed connection don't cause problems.

    // Problem Without SO_REUSEADDR
    // If your server program crashes or you need to restart it quickly, the port it was using might still be in TIME_WAIT.
    // Without SO_REUSEADDR, when your server tries to restart and bind to the same port, the operating system might block the attempt with an error like "Address already in use" because the port is still tied up in TIME_WAIT.
    // How SO_REUSEADDR Helps
    // Allows Immediate Reuse: By setting SO_REUSEADDR, you tell the operating system that your server should be allowed to bind to the port even if it’s still in TIME_WAIT. This is useful for servers that need to restart quickly, such as after a crash or during a software update.

    // setsockopt(int, int, int, const void*, socklen_t);
    // SOL_SOCKET specifies that the option is at the socket level.

    if(setsockopt(proxy_socketID, SOL_SOCKET, SO_REUSEADDR, (const char*) &reuse, sizeof(reuse)) < 0){

        perror("Set Socket Option Fail\n");

    }

    // cleaning up

    bzero((char*) &server_addr, sizeof(server_addr)); // bzero is a function that sets all the bytes in a block of memory to zero.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number); // convert from host byte order to network byte order
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // lets bind

    if(bind(proxy_socketID, (struct sockaddr*) &server_addr, sizeof(server_addr))){

        perror("Error binding, port not available\n");

    }

    printf("Binding on port %d\n", port_number);
    int listen_status = listen(proxy_socketID, MAX_CLIENTS);

    if(listen_status < 0){
        perror("Server not listening\n");
        exit(1);
    }

    printf("Server listening on port %d\n", port_number);
    
    int connected_clients_count = 0;
    int connected_sockets[MAX_CLIENTS];

    while(true){

        bzero((char*) &client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketID, (struct sockaddr*) &client_addr, (socklen_t*) &client_len);

        if(client_socketId < 0){
            perror("Not able to accept connection\n");
        }
        else{

            printf("Connection Established\n");
            connected_sockets[connected_clients_count] = client_socketId;

        }

        struct sockaddr_in* client_port = (struct sockaddr_in*) &client_addr;
        struct in_addr ip_addr = client_port -> sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number %d and client ip address is %s\n", ntohs(client_addr -> sin_port), str);

        pthread_create(&tid[connected_clients_count], NULL, thread_fn, (void *) &connected_sockets[connected_clients_count]);
        connected_clients_count++;

    }

    close(proxy_socketID);
    return 0;

    return 0;
}
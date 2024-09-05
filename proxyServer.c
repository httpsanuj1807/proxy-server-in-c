#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lru_cache.h"
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
//     sa_family_t sa_family; // address family, AF_INET for ipv4 and  AF_INET6 for ipv6
//     char sa_data[14]; // protocol specific data

// };

#define MAX_CLIENTS 10
#define MAX_BYTES 4096 

int port_number = 8080;
int proxy_socketID; // my proxy web server socket
pthread_t tid[MAX_CLIENTS]; //  each element representing a unique thread in my program  
sem_t semaphore;  // basically a coun ter type of lock


int connectRemoteServer(char* host_addr, int port_number){

    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

    if(proxy_socketID < 0){
        perror("Failed to create remote server socket\n");
        return -1;
    }

    // trying resolving host string into a IP address [netdb.h]
    // struct hostent {
    // char    *h_name;        // Official name of the host
    // char    **h_aliases;    // Alias list
    // int     h_addrtype;     // Address type (AF_INET for IPv4)
    // int     h_length;       // Length of the address in bytes
    // char    **h_addr_list;  // List of addresses (IPv4 addresses as binary) 
    // };

    // The first place gethostbyname might look is the local host file on your system, typically located at /etc/hosts. This file can contain mappings of IP addresses to hostnames for local or internal networks. Then go to DNS services

    struct hostent *host = gethostbyname(host_addr);

    if(host == NULL){

        //stderr
        // This line of code sends the string "No such host exists\n" to the standard error stream (stderr).
        // stderr is typically used for error messages or diagnostics, and output directed here is usually displayed in the console or terminal, separate from standard output (stdout).
        
        // perror outputs the string you provide, followed by a colon, a space, and then the textual representation of the current error based on errno.
        // It’s typically used after a system call or library function that fails to provide meaningful information about what went wrong.

        fprintf(stderr, "No such host exists\n");
        return -1;

    }

    // establishing connection 

    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);

    bcopy((char *) &host -> h_addr, (char *) &server_addr.sin_addr.s_addr, host -> h_length);
    if(connect(remoteSocket, (struct sockaddr *) &server_addr, (int) sizeof(server_addr)) < 0){
        fprintf(stderr, "Error in connecting remote server\n");
        return -1;
    }

    printf("Connected to remote server at port %d\n", port_number);
    return remoteSocket;

}

int handle_request(int clientSocketId, ParsedRequest *request, char* tempReq){

    // creating a http request

    // GET /index.html HTTP/1.1
    // Host: example.com

    // request -> path = "/index.html"
    // request->version = "HTTP/1.1"
    // request->host = "example.com"
    // request->port = NULL indicating default port
    // request->headers contains any additional headers sent by the client.


    char *buffer = (char*) malloc(sizeof(char) * MAX_BYTES);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        return -1;
    }
    strcpy(buffer, "GET ");
    strcat(buffer, request -> path);
    strcat(buffer, " ");
    strcat(buffer, request -> version);
    strcat(buffer, "\r\n");

    // buffer "GET /index.html HTTP/1.1\r\n"

    int len = strlen(buffer);

    // ParsedHeader: any header after the request line is a key-value pair with the
    // format "key:value\r\n" and is maintained in the ParsedHeader linked list

    // int ParsedHeader_set(struct ParsedRequest *pr, const char * key, const char * value);
    if(ParsedHeader_set(request, "Connection", "close") < 0){ // so that it is used for only single use, and server closes connection after sending request
        printf("Set header key is not working\n");
    }

    // struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char * key);
    if(ParsedHeader_get(request, "Host") == NULL){

        if(ParsedHeader_set(request, "Host", request -> host) < 0){
            printf("Set host header key is not working\n");
        }
    }

    // The function un-parses (formats) the headers from request->headers and appends them to the buffer starting at the current length (len). This effectively constructs the complete HTTP request

    if(ParsedRequest_unparse_headers(request, buffer + len, (int) MAX_BYTES - len) < 0){
        printf("Unparse Failed\n");
    }

    // example request now "GET /index.html HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n"


    int server_port = 80;
    if(request -> port != NULL){ // if NULL means default 80
        server_port = atoi(request -> port);
    }

    int remoteSocketId = connectRemoteServer(request -> host, server_port);

    if(remoteSocketId < 0){
        return -1;
    }

    int bytes_receive = 0;
    bytes_receive = send(remoteSocketId, buffer, strlen(buffer), 0);

    if(bytes_receive < 0){
        perror("Failed to send request to remote server\n");
        return -1;
    }

    bzero((char *) buffer, MAX_BYTES);

    char* temp_buffer = (char *) malloc(sizeof(char) *  MAX_BYTES); // storing to save in cache
    
    if (temp_buffer == NULL) {
        perror("Failed to allocate temp_buffer");
        free(buffer);
        return -1;
    }

    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while((bytes_receive = recv(remoteSocketId, buffer, MAX_BYTES - 1, 0)) > 0){

        // send data to client

        int bytes_sent = send(clientSocketId, buffer, bytes_receive, 0);

        if(bytes_sent < 0){
            perror("Error in sending data to the client");
            free(temp_buffer);
            free(buffer);
            return -1;
        }

        // check if  temp buffer needs to be resized

        if(temp_buffer_index + bytes_receive > temp_buffer_size){

            temp_buffer_size += MAX_BYTES;
            char* new_temp_buffer = (char*) realloc(temp_buffer, temp_buffer_size);
            if (new_temp_buffer == NULL) {
                perror("Failed to reallocate temp_buffer");
                free(temp_buffer);
                free(buffer);
                return -1;
            }

            temp_buffer = new_temp_buffer;
        
        }

        memcpy(temp_buffer + temp_buffer_index, buffer, bytes_receive);
        temp_buffer_index += bytes_receive;
        bzero(buffer, MAX_BYTES);

    }

    if(bytes_receive < 0){
        perror("Error in receiving data from the server\n");
        return -1;
    }

    temp_buffer[temp_buffer_index] = '\0';
    free(buffer);
    store_in_cache(temp_buffer, strlen(temp_buffer), tempReq);
    free(temp_buffer);
    close(remoteSocketId);
    return 0;

}


int checkHTTPversion(char* msg){

    int version = -1;

    if(strncmp(msg, "HTTP/1.1", 8) == 0){
        version = 1;
    }
    else if(strncmp(msg, "HTTP/1.0", 8) == 0){
        version = 1;
    }

    return version;

}


int sendErrorCustom(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: Anuj1807\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  printf("400 Bad Request\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: Anuj1807\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  printf("403 Forbidden\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: Anuj1807\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  printf("404 Not Found\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: Anuj1807\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error. This is all what we know.</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //printf("500 Internal Server Error\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: Anuj1807\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  printf("501 Not Implemented\n");
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: Anuj1807\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  printf("505 HTTP Version Not Supported\n");
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

	}
	return 1;
}

void* thread_fn(void *newSocket){

    sem_wait(&semaphore);
    int currSemValue;
    sem_getvalue(&semaphore, &currSemValue);
    printf("\nCurrent semaphore value is: %d\n\n", currSemValue);

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
                perror("Error receiving request data from client\n");
                free(buffer);
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

    cache_element* inCacheAddr = find_in_cache(tempReq);

    if(inCacheAddr != NULL){

        int size = inCacheAddr -> len;
        int pos = 0;
        char response[MAX_BYTES];

        while(pos < size){

            bzero(response, MAX_BYTES);
            
            int chunk_size = (size - pos < MAX_BYTES) ? (size - pos) : MAX_BYTES;

            memcpy(response, inCacheAddr -> data + pos, chunk_size);

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
        //Create a ParsedRequest to use. This ParsedRequest
        //is dynamically allocated.
        ParsedRequest *request = ParsedRequest_create();

        if(ParsedRequest_parse(request, buffer, len) < 0){
            perror("Unable to parse the request\n");   
        }
        else{
            printf("Method:%s\n", request -> method);
            printf("Host:%s\n", request -> host);
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
                printf("We currently fulfill GET requests only\n ");
            }
        }
        ParsedRequest_destroy(request);
    }
    else if(bytes_client_send == 0){
        printf("Client is disconnected\n");
    }
    else if(bytes_client_send < 0){
		printf("Error in receving request\n");
	}

    shutdown(clientSocketId, SHUT_RDWR);
    close(clientSocketId);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore, &currSemValue);
    printf("Current semphore value is %d\n", currSemValue);
    free(tempReq);
    return NULL;

}

int main(int argc, char* argv[]){ // (type: int, argc is argument count that represents the command line arguments including the program name, if argc is 1 means no additional just the program name, greater than 0 means additional, now argv stands for argument vector, we can access by argv[0]...)

    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;
    sem_init(&semaphore, 0, MAX_CLIENTS);  // initializing (var_addr, min value, max value)
    pthread_mutex_init(&cache_mutex, NULL);


    // checking is user passed the port number or not
    if(argc == 2){

        port_number = atoi(argv[1]);

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
            exit(1);
        }
        else{

            printf("Connection Established\n\n");
            connected_sockets[connected_clients_count] = client_socketId;

        }

        struct sockaddr_in* client_port = (struct sockaddr_in*) &client_addr;
        struct in_addr ip_addr = client_port -> sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client is connected with port number %d and client ip address is %s\n", ntohs(client_addr.sin_port), str);

        pthread_create(&tid[connected_clients_count], NULL, thread_fn, (void *) &connected_sockets[connected_clients_count]);
        connected_clients_count++;

    }

    close(proxy_socketID);
    return 0;
}
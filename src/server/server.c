// GROUP 65
// Member: Brian Clinkenbeard

#include "server.h"
#include "protocol.h"
#include <bits/getopt_core.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "linkedList.h"

const char exit_str[] = "exit";

char buffer[BUFFER_SIZE];
pthread_mutex_t buffer_lock;

int total_num_msg = 0;
int listen_fd;

struct userlist users;

int DEBUG_OUT = 1;

void sigint_handler(int sig) {
    printf("shutting down server\n");
    close(listen_fd);
    exit(0);
}

int server_init(int server_port) {
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
    } else
        printf("Socket successfully created\n");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    } else
        printf("Socket successfully binded\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("Listen failed\n");
        exit(EXIT_FAILURE);
    } else
        printf("Server listening on port: %d.. Waiting for connection\n", server_port);

    return sockfd;
}

//Function running in thread
void *process_client(void *clientfd_ptr) {
    printf("Processing client\n");
    int client_fd = *(int *)clientfd_ptr;
    free(clientfd_ptr);
    int received_size;
    fd_set read_fds;

    int retval;
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        retval = select(client_fd + 1, &read_fds, NULL, NULL, NULL);
        if (retval != 1 && !FD_ISSET(client_fd, &read_fds)) {
            printf("Error with select() function\n");
            break;
        }

        pthread_mutex_lock(&buffer_lock);

        if (DEBUG_OUT)
            printf("Thread ID: %lu\n", pthread_self());

        // read header
        petr_header r, s;
        rd_msgheader(client_fd, &r);
        if (r.msg_len < 0) {
            printf("Error reading message\n");
            break;
        }
        
        // read buffer
        bzero(buffer, BUFFER_SIZE);
        received_size = read(client_fd, buffer, r.msg_len);
        if (received_size < 0 || received_size != r.msg_len) {
            printf("Invalid size\n");
            break;
        }

        // TODO: restructure. create jobs based on headers
        switch (r.msg_type) {
        case LOGOUT:
        {
            int ui = getIndexByFD(&users, client_fd);
            node_t *c = getNode(&users, ui);
            printf("Logging out user %s\n", c->username);
            if (ui >= 0) {
                removeByIndex(&users, ui); // remove user

                s.msg_type = OK;
             } else {
                printf("Error: user not found to logout!");

                s.msg_type = ESERV;
            }
            s.msg_len = 0;
            break;
        }
        default:
            printf("WATAFAK!!!!!!\n");
            s.msg_len = 0;
            s.msg_type = ESERV;
        }
        /*
        total_num_msg++;
        // print buffer which contains the client contents
        printf("Receive message from client: %s\n", buffer);
        printf("Total number of received messages: %d\n", total_num_msg);
        */

        // send response to client
        int ret = wr_msg(client_fd, &s, buffer);
        pthread_mutex_unlock(&buffer_lock);

        if (ret < 0) {
            printf("Sending failed\n");
            break;
        }
        //printf("Send response to client: %s\n", buffer);

        // close connection when user logs out or tries to login as an existing user
        if (r.msg_type == LOGOUT)
            break;
    }
    // Close the socket at the end
    printf("Closing client (FD: %d)\n", client_fd);
    close(client_fd);
    return NULL;
}

void run_server(int server_port) {
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    int client_fd;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    pthread_t tid;

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        printf("signal handler processing error\n");

    while (1) {
        // Wait and Accept the connection from client
        //printf("Wait for new client connection\n");
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA *)&client_addr, (socklen_t*)&client_addr_len);
        if (*client_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        } else {
            printf("Client connetion accepted (FD %d)\n", *client_fd);

            petr_header login, r;
            r.msg_len = 0;

            rd_msgheader(*client_fd, &login);
            if (login.msg_len < 0) {
                printf("Error reading message, closing connection\n");
                close(*client_fd);
            }

            char name[32];
            read(*client_fd, name, login.msg_len); // TODO: no users above 32 chars
            if (nameExists(&users, name)) {
                printf("Invalid login for username %s: user exists\n", name);

                // respond with error
                r.msg_type = EUSREXISTS;
                wr_msg(*client_fd, &r, "");

                printf("Closing client (FD %d)", *client_fd);
                close(*client_fd);
            } else {
                printf("Login accepted for user %s\n", name);

                addUser(&users, name, *client_fd); // add user to userlist

                // reply OK
                r.msg_type = OK;
                wr_msg(*client_fd, &r, "");

                // create client thread
                pthread_create(&tid, NULL, process_client, (void *)client_fd);
            }
        }
    }
    bzero(buffer, BUFFER_SIZE);
    close(listen_fd);
    return;
}

int main(int argc, char *argv[]) {
    int opt;

    const char usage[] = "%s [-h] [-j N] PORT_NUMBER AUDIT_FILENAME\n";
    unsigned int port = 0;
    unsigned int jthreads = 2;
    char audit_log[32];
    while ((opt = getopt(argc, argv, "hj:")) != -1) {
        switch (opt) {
        case 'h':
            printf(usage, argv[0]);
            printf("\n-h\t\tDisplays this help menu, and returns EXIT_SUCCESS.\n");
            printf("-j N\t\tNumber of job threads. Default to 2.\n");
            printf("AUDIT_FILENAME\tFile to output Audit Log messages to.\n");
            printf("PORT_NUMBER\tPort number to listen on.\n");
            exit(EXIT_SUCCESS);
        case 'j':
            jthreads = atoi(optarg);
            break;
        default: /* '?' */
            fprintf(stderr, usage, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc - 1) {
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    } else {
        port = atoi(argv[optind]);
        strcpy(audit_log, argv[optind+1]); // TODO: safety
    }

    if (DEBUG_OUT)
        printf("Job threads: %d | Port: %d | Audit log: %s\n", jthreads, port, audit_log);

    run_server(port);

    return 0;
}

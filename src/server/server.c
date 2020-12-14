// GROUP 65
// Member: Brian Clinkenbeard


#include "server.h"
#include "linkedList.h"
#include "protocol.h"
#include <bits/getopt_core.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "sbuf.h"

const char exit_str[] = "exit";

char buffer[BUFFER_SIZE];
pthread_mutex_t buffer_lock;

int total_num_msg = 0;
int listen_fd;

// userlist
userlist_t users;

// rooms
#define MAX_ROOMS 10
roomlist_t rooms;

// jobs
#define MAX_JOBS 16
sbuf_t j_buf;

// TODO: switch to proper debugging
const int DEBUG_OUT = 1;

void sigint_handler(int sig) {
    printf("shutting down server\n");

    deleteList(&users); // free userlist
    sbuf_deinit(&j_buf);
    // TODO: client fds + zombies?
    close(listen_fd);
    exit(0);
}

petr_header roomDelete(char* room, user_t user) {
    petr_header r;
    room_t *r_room = getRoom(&rooms, room);
    if (r_room) {
        // must be owner
        if (strcmp(user.username, r_room->owner) == 0) {
            printf("deleting room %s\n", r_room->roomname);
            // notify other users of deletion
            for (user_t *u = r_room->userlist->head; u != NULL; u = u->next) {
                if (strcmp(u->username, r_room->owner) != 0) {
                    printf("notifying %s of %s closing\n", u->username, r_room->roomname);
                    petr_header notify = { .msg_type = RMCLOSED, .msg_len = strlen(r_room->roomname) + 1 };
                    wr_msg(u->user_fd, &notify, r_room->roomname);
                } 
            }
            removeRoom(&rooms, r_room->roomname);

            r.msg_type = OK;
        } else {
            printf("room not owned by user\n");
            r.msg_type = ERMDENIED;
        }
    } else {
        printf("room %s not found to delete\n", r_room->roomname);
        r.msg_type = ERMNOTFOUND;
    }
    r.msg_len = 0;
    return r;
}

void *process_job() {
    printf("job thread started: %lu\n", pthread_self());
    while (1) {
        // wait for job
        j_msg m = sbuf_remove(&j_buf);

        printf("removed job from buffer on thread %lu\n", pthread_self());

        petr_header r; // response header

        pthread_mutex_lock(&buffer_lock);

        // start with empty buffer
        bzero(buffer, BUFFER_SIZE);
        switch (m.header.msg_type) {
        case RMCREATE:
        {
            printf("creating room\n");
            if (getRoom(&rooms, m.msg)) {
                printf("room exists!!\n");
                r.msg_type = ERMEXISTS;
                //r.msg_len = 0;
            } else {
                addRoom(&rooms, m.msg, m.user.username);
                addUserToRoom(getRoom(&rooms, m.msg), m.user);
                r.msg_type = OK;
                //r.msg_len = 0;
            }
            break;
        }
        case RMDELETE:
        {
            r = roomDelete(m.msg, m.user);
            break;
        }
        case RMLIST:
        {
           if (rooms.head == NULL) {
                printf("no rooms\n");
            } else {
                printf("creating roomlist\n");
                for (room_t *c = rooms.head; c != NULL; c = c->next) {
                    strcat(buffer, c->roomname);
                    strcat(buffer, ": ");
                    for (user_t *u = c->userlist->head; u != NULL; u = u->next) {
                        strcat(buffer, u->username);
                        if (u->next)
                            strcat(buffer, ",");
                    }
                    strcat(buffer, "\n");
                }
            }
            r.msg_type = RMLIST;
            //r.msg_len = strlen(buffer);
            printf("room list length: %d\n", r.msg_len);
            break;
        }
        case RMJOIN:
        {
            room_t *j_room = getRoom(&rooms, m.msg);
            if (j_room) {
                printf("adding user %s to room %s\n", m.user.username, m.msg);
                addUserToRoom(j_room, m.user);
                r.msg_type = OK;
                //r.msg_len = 0;
            } else {
                printf("room %s requested by %s not found\n", m.msg, m.user.username);
                r.msg_type = ERMNOTFOUND;
                //r.msg_len = 0;
            }
            break;
        }
        case RMLEAVE:
        {
            room_t *l_room = getRoom(&rooms, m.msg);
            if (l_room) {
                if (strcmp(l_room->owner, m.user.username) == 0) {
                    printf("owner cannot leave room, must delete\n");
                    r.msg_type = ERMDENIED;
                } else {
                    printf("removing user from room\n");
                    removeUserFromRoom(&rooms, l_room, m.user); // if user is not in room, nothing happens
                    r.msg_type = OK;
                }
            } else {
                printf("room doesn't exist\n");
                r.msg_type = ERMNOTFOUND;
            }
            break;
        }
        case RMSEND:
        {
            char *room = strtok(m.msg, "\r\n"); // TODO: possible memory leaks
            printf("token 1: %s\n", room);
            room_t *s_room = getRoom(&rooms, room);
            if (s_room) {
                if (getIndexByFD(s_room->userlist, m.user.user_fd) >= 0) {
                    printf("sending room message\n");
                    char *message = strtok(NULL, "\r\n");
                    printf("token 2: %s\n", message);
                    strcat(buffer, s_room->roomname);
                    strcat(buffer, "\r\n");
                    strcat(buffer, m.user.username);
                    strcat(buffer, "\r\n");
                    strcat(buffer, message);
                    strcat(buffer, "\r\n");
                    for (user_t *u = s_room->userlist->head; u != NULL; u = u->next) {
                        if (strcmp(u->username, m.user.username) != 0) {
                            printf("sending message %s to %s\n", message, u->username);
                            petr_header send = { .msg_type = RMRECV, .msg_len = strlen(buffer) + 1 };
                            wr_msg(u->user_fd, &send, buffer);
                        }
                    }
                    bzero(buffer, BUFFER_SIZE); // zero buffer after sending to other users
                    r.msg_type = OK;
                } else {
                    r.msg_type = ERMDENIED;
                }
            } else {
                r.msg_type = ERMNOTFOUND;
            }
            break;
        }
        case USRLIST:
        {
            for (user_t *u = users.head; u != NULL; u = u->next) {
                if (strcmp(u->username, m.user.username) != 0) {
                    strcat(buffer, u->username);
                    strcat(buffer, "\n");
                }
            }
            r.msg_type = USRLIST;
            break;
        }
        default:
            printf("WATAFAK!!!\n");
            r.msg_type = ESERV;
        }
        // set response length
        r.msg_len = strlen(buffer);
        if (r.msg_len > 0)
            r.msg_len++; // include the null terminator

        printf("responding with buffer [%s] (length %d)\n", buffer, r.msg_len);
        wr_msg(m.user.user_fd, &r, buffer);
        pthread_mutex_unlock(&buffer_lock);
    }

    return NULL;
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
            printf("client thread: %lu\n", pthread_self());

        // read header
        petr_header r, s;
        rd_msgheader(client_fd, &r);
        if (r.msg_len < 0) {
            printf("Error reading message\n");
            pthread_mutex_unlock(&buffer_lock);
            break;
        }
        
        // read buffer
        bzero(buffer, BUFFER_SIZE);
        received_size = read(client_fd, buffer, r.msg_len);
        if (received_size < 0 || received_size != r.msg_len) {
            printf("Invalid size\n");
            pthread_mutex_unlock(&buffer_lock);
            break;
        }

        if (r.msg_type == LOGOUT) {
            // TODO: send logout job to remove from all roomlists
            int ui = getIndexByFD(&users, client_fd);
            user_t *c = getUser(&users, ui);
            printf("Logging out user %s\n", c->username);
            if (ui >= 0) {
                // remove all rooms owned by user
                for (room_t *r = rooms.head; r != NULL; r = r->next) {
                    if (strcmp(c->username, r->owner) == 0) {
                        roomDelete(r->roomname, *c);
                    }
                }
                removeByIndex(&users, ui); // remove user

                s.msg_type = OK;
             } else {
                printf("Error: user not found to logout!");

                s.msg_type = ESERV;
            }
            s.msg_len = 0;

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
        } else {
            printf("Sending job to job buffer\n");
            j_msg n_job; // new job
            n_job.header = r; // forward header
            n_job.user = *getUser(&users, getIndexByFD(&users, client_fd)); // TODO: totally unsafe
            strcpy(n_job.msg, buffer);

            pthread_mutex_unlock(&buffer_lock);

            sbuf_insert(&j_buf, n_job); // add job
        }

        /*
        total_num_msg++;
        // print buffer which contains the client contents
        printf("Receive message from client: %s\n", buffer);
        printf("Total number of received messages: %d\n", total_num_msg);
        */

    }
    // Close the socket at the end
    printf("Closing client (FD: %d)\n", client_fd);
    close(client_fd);
    return NULL;
}

void run_server(int server_port, int j_threads) {
    listen_fd = server_init(server_port); // Initiate server and start listening on specified port
    int client_fd;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    // handle interrupt
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        printf("signal handler processing error\n");

    // TODO: initialize userlist? necessary? 

    // initialize job queue
    sbuf_init(&j_buf, MAX_JOBS);

    // start job threads
    for (int i = 0; i < j_threads; ++i) {
        pthread_t jtid;
        pthread_create(&jtid, NULL, process_job, NULL);
    }

    pthread_t tid;

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
                continue;
            } else if (login.msg_len > STR_MAX) {
                printf("Username too long, closing connection\n");
                close(*client_fd);
                continue;
            }

            char name[32];
            read(*client_fd, name, login.msg_len);
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
    unsigned int j_threads = 2;
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
            j_threads = atoi(optarg);
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
        printf("Job threads: %d | Port: %d | Audit log: %s\n", j_threads, port, audit_log);

    run_server(port, j_threads);

    return 0;
}

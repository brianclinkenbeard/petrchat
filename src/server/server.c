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
#include "debug.h"

const char exit_str[] = "exit";

char buffer[BUFFER_SIZE];
pthread_mutex_t buffer_lock;
//pthread_mutex_t rooms_lock;
//pthread_mutex_t rooms_lock;

int listen_fd;
FILE *a_log; // audit log

// userlist
userlist_t users = { .head = NULL, .length = 0 };

// rooms
#define MAX_ROOMS 10
roomlist_t rooms = { .head = NULL, .length = 0};

// jobs
#define MAX_JOBS 16
sbuf_t j_buf;

void sigint_handler(int sig) {
    fprintf(a_log, "Shutting down server\n");

    sbuf_deinit(&j_buf);
    deleteRoomList(&rooms);
    // close all client fds
    for (user_t *u = users.head; u != NULL; u = u->next) {
        close(u->user_fd);
    }
    deleteUserList(&users);

    close(listen_fd);
    exit(0);
}

// locks rooms
void roomCreate(char* room, user_t user) {
    petr_header r = { .msg_len = 0 };

    fprintf(a_log, "Creating room %s\n", room);
    if (getRoom(&rooms, room)) {
        fprintf(a_log, "Room already exists!\n");
        r.msg_type = ERMEXISTS;
    } else {
        addRoom(&rooms, room, user); // adds owner to room as well
        fprintf(a_log, "Successfully added room\n");
        r.msg_type = OK;
    }

    wr_msg(user.user_fd, &r, "");
}

void roomDelete(char* room, user_t user, bool write) {
    fprintf(a_log, "Requesting deletion of room %s by %s\n", room, user.username);
    petr_header r = { .msg_len = 0 };

    room_t *r_room = getRoom(&rooms, room);
    if (r_room) {
        // must be owner
        if (strcmp(user.username, r_room->owner) == 0) {
            fprintf(a_log, "Deleting room %s...\n", r_room->roomname);
            // notify other users of deletion
            for (user_t *u = r_room->userlist->head; u != NULL; u = u->next) {
                if (strcmp(u->username, r_room->owner) != 0) {
                    fprintf(a_log, "Notifying %s of %s closing\n", u->username, r_room->roomname);
                    petr_header notify = { .msg_type = RMCLOSED, .msg_len = strlen(r_room->roomname) + 1 };
                    wr_msg(u->user_fd, &notify, r_room->roomname);
                } 
            }
            removeRoom(&rooms, r_room->roomname);

            r.msg_type = OK;
        } else {
            fprintf(a_log, "Room not owned by user\n");
            r.msg_type = ERMDENIED;
        }
    } else {
        fprintf(a_log, "Room %s not found\n", r_room->roomname);
        r.msg_type = ERMNOTFOUND;
    }

    if (write)
        wr_msg(user.user_fd, &r, "");
}

// locks buffer and room (access)
void roomList(user_t user) {
    fprintf(a_log, "Roomlist requested by %s\n", user.username);

    if (rooms.head == NULL) {
        fprintf(a_log, "No rooms\n");
    } else {
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
        fprintf(a_log, "Created roomlist\n");
    }
    // add null terminator if buffer is not empty
    petr_header r = { .msg_type = RMLIST, .msg_len = strlen(buffer) ? strlen(buffer) + 1 : 0 }; 
    wr_msg(user.user_fd, &r, buffer);
    // TODO: zero buffer
}

void roomJoin(char *room, user_t user) {
    fprintf(a_log, "User %s request to join room %s\n", user.username, room);
    petr_header r = { .msg_len = 0 };
    room_t *j_room = getRoom(&rooms, room);
    if (j_room) {
        addUserToRoom(j_room, user);
        fprintf(a_log, "Added user %s to room %s\n", user.username, room);
        r.msg_type = OK;
    } else {
        fprintf(a_log, "Room %s requested by %s not found\n", room, user.username);
        r.msg_type = ERMNOTFOUND;
    }

    wr_msg(user.user_fd, &r, "");
}

void roomLeave(char* room, user_t user) {
    fprintf(a_log, "User %s requesting to leave room %s\n", user.username, room);
    petr_header r = { .msg_len = 0 };
    room_t *l_room = getRoom(&rooms, room);
    if (l_room) {
        if (strcmp(l_room->owner, user.username) == 0) {
            fprintf(a_log, "Owner cannot leave room, must delete\n");
            r.msg_type = ERMDENIED;
        } else {
            removeUserFromRoom(&rooms, l_room, user); // if user is not in room, nothing happens
            fprintf(a_log, "Removed user from room\n");
            r.msg_type = OK;
        }
    } else {
        fprintf(a_log, "Room doesn't exist\n");
        r.msg_type = ERMNOTFOUND;
    }
 
    wr_msg(user.user_fd, &r, "");
}

// locks buffer, users
void roomSend(char *user_str, user_t user) {
    petr_header r = { .msg_len = 0 };

    char *room = strtok(user_str, "\r");
    room_t *s_room = getRoom(&rooms, room);
    if (s_room) {
        if (getIndexByFD(s_room->userlist, user.user_fd) >= 0) {
            char *message = strtok(NULL, "\r");
            message++; // skip newline

            // write response to buffer
            strcat(buffer, s_room->roomname);
            strcat(buffer, "\r\n");
            strcat(buffer, user.username);
            strcat(buffer, "\r\n");
            strcat(buffer, message);

            fprintf(a_log, "Room message %s from %s in %s\n", message, user.username, room);

            // send to all users
            for (user_t *u = s_room->userlist->head; u != NULL; u = u->next) {
                if (strcmp(u->username, user.username) != 0) {
                    fprintf(a_log, "Sending message to %s\n", message);
                    petr_header send = { .msg_type = RMRECV, .msg_len = strlen(buffer) + 1 };
                    wr_msg(u->user_fd, &send, buffer);
                }
            }
            bzero(buffer, BUFFER_SIZE); // zero buffer after sending to other users
            r.msg_type = OK;
        } else {
            fprintf(a_log, "User %s not in room %s\n", user.username, room);
            r.msg_type = ERMDENIED;
        }
    } else {
        fprintf(a_log, "Room %s not found\n", room);
        r.msg_type = ERMNOTFOUND;
    }

    wr_msg(user.user_fd, &r, "");
}

// locks buffer, userlist
void userSend(char *user_str, user_t user) {
    petr_header r = { .msg_len = 0 };

    char *usr_str = strtok(user_str, "\r");
    user_t *s_user = getUserByName(&users, usr_str);
    if (s_user) {
        char *message = strtok(NULL, "\r");
        message++; // skip newline

        // write response to buffer
        strcat(buffer, user.username);
        strcat(buffer, "\r\n");
        strcat(buffer, message);

        // send message
        petr_header send = { .msg_type = USRRECV, .msg_len = strlen(buffer) + 1 };
        wr_msg(s_user->user_fd, &send, buffer);
        fprintf(a_log, "User %s sent user %s message %s\n", user.username, s_user->username, message);

        bzero(buffer, BUFFER_SIZE); // zero buffer after sending
        r.msg_type = OK;
    } else {
        fprintf(a_log, "User %s requested by user %s not found\n", usr_str, user.username);
        r.msg_type = EUSRNOTFOUND;
    }

    wr_msg(user.user_fd, &r, "");
}

// locks buffer and userlist
void userList(user_t user) {
    fprintf(a_log, "User %s\n requested userlist\n", user.username);

    for (user_t *u = users.head; u != NULL; u = u->next) {
        if (strcmp(u->username, user.username) != 0) { // not requesting user
            strcat(buffer, u->username);
            strcat(buffer, "\n");
        }
    }
    fprintf(a_log, "Created userlist\n");

    petr_header r = { .msg_type = USRLIST, .msg_len = strlen(buffer) ? strlen(buffer) + 1 : 0 }; 
    wr_msg(user.user_fd, &r, buffer);

    bzero(buffer, BUFFER_SIZE); // zero buffer after sending
}

void logout(user_t user) {
    // TODO: send logout job to remove from all roomlists
    fprintf(a_log, "Logging out user %s\n", user.username);
    // delete or remove from rooms
    for (room_t *r = rooms.head; r != NULL; r = r->next) {
        if (strcmp(user.username, r->owner) == 0)
            roomDelete(r->roomname, user, false);
        else
            removeUserFromRoom(&rooms, r, user); // checks if user exists
    }
    removeByIndex(&users, getIndexByFD(&users, user.user_fd)); // TODO: bad

    petr_header r = { .msg_type = OK, .msg_len = 0 };

    // send response to client
    int ret = wr_msg(user.user_fd, &r, "");
}


void *process_job() {
    fprintf(a_log, "Job thread started: %lu\n", pthread_self());

    while (1) {
        // wait for job
        j_msg m = sbuf_remove(&j_buf);
        fprintf(a_log, "Removed job from buffer on thread %lu\n", pthread_self());

        pthread_mutex_lock(&buffer_lock);
        bzero(buffer, BUFFER_SIZE); // start with empty buffer
        switch (m.header.msg_type) {
        case RMCREATE:
            roomCreate(m.msg, m.user);
            break;
        case RMDELETE:
            roomDelete(m.msg, m.user, true);
            break;
        case RMLIST:
            roomList(m.user);
            break;
        case RMJOIN:
            roomJoin(m.msg, m.user);
            break;
        case RMLEAVE:
            roomLeave(m.msg, m.user);
            break;
        case RMSEND:
            roomSend(m.msg, m.user);
            break;
        case USRSEND:
            userSend(m.msg, m.user);
            break;
        case USRLIST:
            userList(m.user);
            break;
        default:
            fprintf(a_log, "OH NO!!!\n");
            petr_header r = { .msg_type = ESERV, .msg_len = 0 };
            wr_msg(m.user.user_fd, &r, "");
        }

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
        fprintf(a_log, "Socket successfully created\n");

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
        fprintf(a_log, "socket bind failed\n");
        exit(EXIT_FAILURE);
    } else
        fprintf(a_log, "Socket successfully binded\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        fprintf(a_log, "Listen failed\n");
        exit(EXIT_FAILURE);
    } else
        fprintf(a_log, "Server listening on port: %d.. Waiting for connection\n", server_port);

    return sockfd;
}

//Function running in thread
void *process_client(void *clientfd_ptr) {
    fprintf(a_log, "Processing client\n");
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
            fprintf(a_log, "Error with select() function\n");
            break;
        }

        pthread_mutex_lock(&buffer_lock);

        fprintf(a_log, "Client thread: %lu\n", pthread_self());

        // read header
        petr_header r, s;
        rd_msgheader(client_fd, &r);
        if (r.msg_len < 0) {
            fprintf(a_log, "Error reading message\n");

            pthread_mutex_unlock(&buffer_lock);
            break;
        }
        
        // read buffer
        bzero(buffer, BUFFER_SIZE);
        received_size = read(client_fd, buffer, r.msg_len);
        if (received_size < 0 || received_size != r.msg_len) {
            fprintf(a_log, "Invalid size\n");
            pthread_mutex_unlock(&buffer_lock);
            break;
        }

        if (r.msg_type == LOGOUT) {
            // this sucks
            logout(*getUser(&users, getIndexByFD(&users, client_fd)));

            pthread_mutex_unlock(&buffer_lock);
            break;
        } else {
            j_msg n_job; // new job
            n_job.header = r; // forward header
            n_job.user = *getUser(&users, getIndexByFD(&users, client_fd)); // TODO: totally unsafe
            strcpy(n_job.msg, buffer);

            pthread_mutex_unlock(&buffer_lock);

            fprintf(a_log, "Inserting job to job buffer\n");
            sbuf_insert(&j_buf, n_job); // add job
        }
    }
    // Close the socket at the end
    fprintf(a_log, "Closing client (FD: %d)\n", client_fd);
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
        fprintf(a_log, "signal handler processing error\n");

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
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, (SA *)&client_addr, (socklen_t*)&client_addr_len);
        if (*client_fd < 0) {
            fprintf(a_log, "server acccept failed\n");
            exit(EXIT_FAILURE);
        } else {
            fprintf(a_log, "Client connection accepted (FD %d)\n", *client_fd);

            petr_header login, r;
            r.msg_len = 0;

            rd_msgheader(*client_fd, &login);
            if (login.msg_len < 0) {
                fprintf(a_log, "Error reading message, closing connection\n");
                close(*client_fd);
                continue;
            } else if (login.msg_len > STR_MAX) {
                fprintf(a_log, "Username too long, closing connection\n");
                close(*client_fd);
                continue;
            }

            char name[STR_MAX];
            read(*client_fd, name, login.msg_len);
            if (nameExists(&users, name)) {
                fprintf(a_log, "Invalid login for username %s: user exists\n", name);

                // respond with error
                r.msg_type = EUSREXISTS;
                wr_msg(*client_fd, &r, "");

                fprintf(a_log, "Closing client (FD %d)\n", *client_fd);
                close(*client_fd);
            } else {
                fprintf(a_log, "Login accepted for user %s\n", name);

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
    //char audit_log[STR_MAX];
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
        a_log = fopen(argv[optind+1], "w");
        if (a_log == NULL) {
            fprintf(stderr, usage, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    fprintf(a_log, "Starting server with %d job threads on port: %d\n", j_threads, port);

    run_server(port, j_threads);

    return 0;
}

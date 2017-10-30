/*
 *
 *  CMSC 23300 / 33300 - Networks and Distributed Systems
 *
 *  main() code for chirc project
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "log.h"

#define MAX_USERS 100

typedef struct user {
    char *hostname;
    char *nick;
    char *username;
    char *full_name;
} user;

user *USERS[MAX_USERS] = {NULL};

// Create or retrieve existing entry for user with given hostname, and update
// the user's nick. Return pointer to that user or NULL if 
user *add_user(char *hostname, char *nick) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] == NULL) {
            user *new_user = malloc(sizeof(user));
            new_user->nick = malloc(strlen(nick) + 1);
            new_user->hostname = malloc(strlen(hostname) + 1);
            strcpy(new_user->nick, nick);
            strcpy(new_user->hostname, hostname);
            USERS[i] = new_user;
            chilog(DEBUG, "Adding new user with nick: %s", new_user->nick);
            return new_user;
        } else if (strcmp(USERS[i]->hostname, hostname) == 0) {
            char *old_nick = USERS[i]->nick;
            char *new_nick = malloc(strlen(nick) + 1);
            strcpy(new_nick, nick);
            chilog(DEBUG, "Updating user's nick from %s to %s", old_nick, new_nick);
            USERS[i]->nick = new_nick;
            return USERS[i];
        }
    }
    return NULL;
}

user *find_user(char *hostname) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(USERS[i]->hostname, hostname) == 0) {
            return USERS[i];
        }
    }
    return NULL;
}

struct sockaddr_in init_socket(int port) {
    struct sockaddr_in addr;
    bzero((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;         // ipv4
    addr.sin_addr.s_addr = INADDR_ANY; // listen on all
    addr.sin_port = htons(port);       // port
    return addr;
}

char *handle_user_msg(char *msg) {
    printf("Received user command!\n");
    // TODO: save as username?
    user *new_user = find_user(msg);
    if (new_user == NULL) {
        return "Nickname already in use";
    }
    msg = strtok(NULL, " "); // ignore second, third params
    msg = strtok(NULL, " ");
    msg = strtok(NULL, " :");
    strcpy(new_user->full_name, msg);
    return "Welcome to the Internet Relay Network felix!felix@felix.com";
}

void accept_user(int port) {
    int sockfd, replysockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = init_socket(port);
    // TODO: error checking?
    bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(sockfd, 5);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t client_addr_len = sizeof(cli_addr);

        char buffer[256];
        char client_hostname[100];
        replysockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_len);

        if (getnameinfo((struct sockaddr *) &cli_addr, client_addr_len, 
            client_hostname, 100, NULL, 0, 0) != 0) {
            perror("Could not get client hostname");
        }

        chilog(INFO, "Received connection from client: %s", client_hostname);
        read(replysockfd, buffer, 255);
        chilog(INFO, "Received message: %s", buffer);
        char *command = strtok(buffer, " ");
        if (strcmp(command, "NICK") == 0) {
            char *nick = strtok(NULL, " ");
            add_user(client_hostname, nick);
        // } else if (strcmp(command, "USER") == 0) {
        //     char* reply = handle_user_msg(strtok(NULL, " "));
        //     if (reply != NULL) {
        //         if (send(replysockfd, reply, sizeof(reply), 0) == -1) {
        //             perror("could not send a reply!");
        //         }
        //     }
        // } else if (strcmp(command, "EXIT") == 0) { // for debugging only
        //     break;
        } else {
            chilog(WARNING, "Received unknown command %s", command);
        }
        close(replysockfd);
    }
    close(sockfd);
}

int main(int argc, char *argv[]) {
    int opt;
    char *port = "6667", *passwd = NULL;
    int verbosity = 0;

    while ((opt = getopt(argc, argv, "p:o:vqh")) != -1)
        switch (opt)
        {
        case 'p':
            port = strdup(optarg);
            break;
        case 'o':
            passwd = strdup(optarg);
            break;
        case 'v':
            verbosity++;
            break;
        case 'q':
            verbosity = -1;
            break;
        case 'h':
            fprintf(stderr, "Usage: chirc -o PASSWD [-p PORT] [(-q|-v|-vv)]\n");
            exit(0);
            break;
        default:
            fprintf(stderr, "ERROR: Unknown option -%c\n", opt);
            exit(-1);
        }

    if (!passwd)
    {
        fprintf(stderr, "ERROR: You must specify an operator password\n");
        exit(-1);
    }

    /* Set logging level based on verbosity */
    switch(verbosity)
    {
    case -1:
        chirc_setloglevel(QUIET);
        break;
    case 0:
        chirc_setloglevel(INFO);
        break;
    case 1:
        chirc_setloglevel(DEBUG);
        break;
    case 2:
        chirc_setloglevel(TRACE);
        break;
    default:
        chirc_setloglevel(INFO);
        break;
    }

    /* Your code goes here */
    accept_user(atoi(port));
    return 0;
}

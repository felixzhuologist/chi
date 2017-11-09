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

char *RPL_WELCOME = "001";

typedef struct user {
    char *hostname;
    char *nick;
    char *username;
    char *full_name;
} user;

typedef struct message {
    char *prefix;
    char *cmd;
    char *args[15];
} message;

void log_message(const message *msg) {
    if (msg->prefix != NULL) {
        chilog(INFO, "prefix: %s", msg->prefix);
    }
    if (msg->cmd != NULL) {
       chilog(INFO, "cmd: %s", msg->cmd); 
    }
    chilog(INFO, "args:");
    int i = 0;
    while (msg->args[i] != NULL) {
        chilog(INFO, "\t%s", msg->args[i]);
        i++;
    }
}

void strip_carriage_return(char *s) {
    if (s[strlen(s) - 2] == '\r') {
        s[strlen(s) - 2] = '\0'; 
    }
}

void parse_message(char *buffer, message *msg) {
    if (strlen(buffer) > 512) {
        buffer[512] = '\0';
    }

    char *token = strtok(buffer, " ");
    // prefix
    if (token != NULL && strlen(token) > 0 && token[0] == ':') {
        msg->prefix = malloc(strlen(token));
        strcpy(msg->prefix, token + 1);
        token = strtok(NULL, " ");
    }

    // command
    if (token != NULL) {
        msg->cmd = malloc(strlen(token) + 1);
        strcpy(msg->cmd, token);
        token = strtok(NULL, " ");
    }

    // args
    for (int i = 0; i < 15; i++) {
        msg->args[i] = NULL;
        if (token == NULL) { // keep setting the rest of the entries to null
            continue;
        }
        if (strlen(token) > 0 && token[0] == ':') {
            char *rest = strtok(NULL, "");
            msg->args[i] = malloc(strlen(token) + strlen(rest));
            strcpy(msg->args[i], token + 1);
            strcat(msg->args[i], " ");
            strcat(msg->args[i], rest);
        } else {
            msg->args[i] = malloc(strlen(token));
            strcpy(msg->args[i], token);
        }
        token = strtok(NULL, " ");        
    }
}

// expects prefix and last argument to already be prefixed with :
void write_reply(const char *prefix, const char *cmd, const char **args, 
                 const int num_args, char *buffer) {
    int i = 0;

    // prefix
    if (prefix != NULL && strlen(prefix) > 0) {
        strcpy(buffer + i, prefix);
        i += strlen(prefix);
        buffer[i++] = ' ';
    }

    // cmd
    strcpy(buffer + i, cmd);
    i += strlen(cmd);

    // args
    for (int j = 0; j < (num_args < 15 ? num_args : 15); j++) {
        if (args[j] == NULL && strlen(args[j]) > 0) {
            break;
        }
        buffer[i++] = ' ';
        strcpy(buffer + i, args[j]);
        i += strlen(args[j]);
    }

    buffer[i++] = '\r';
    buffer[i++] = '\n';
}

user *USERS[MAX_USERS] = {NULL};

// Create or retrieve existing entry for user with given hostname, and update
// the user's nick. Return pointer to that user or NULL if no more room
user *add_user(const char *hostname, const char *nick) {
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
            chilog(DEBUG, "found existing hostname");
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

user *find_user(const char *hostname) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] == NULL) {
            return NULL;
        } else if (strcmp(USERS[i]->hostname, hostname) == 0) {
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

void handle_user_msg(const char *hostname, const message *msg, char *reply) {
    // set user, full name for user
    user *new_user = find_user(hostname);
    if (new_user == NULL) {
        chilog(WARNING, "Received USER message from unknown hostname");
        return;
    }
    chilog(INFO, "Updating user with nick %s", new_user->nick);
    char *username = msg->args[0]; // don't handle args 1 and 2 for now
    char *full_name = msg->args[3];
    new_user->username = malloc(strlen(username) + 1);
    new_user->full_name = malloc(strlen(full_name) + 1);
    strcpy(new_user->username, username);
    strcpy(new_user->full_name, full_name);
    chilog(DEBUG, "Successfully added username and name to user");

    // construct reply
    char reply_msg[100]; // TODO - get len?
    sprintf(reply_msg, ":Welcome to the Internet Relay Network %s!%s@%s",
        username, username, hostname);
    char *reply_args[2] = {new_user->nick, reply_msg};
    write_reply(":localhost", RPL_WELCOME, reply_args, 2, reply);
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

        char in_buffer[512], reply_buffer[512];
        char client_hostname[100];
        memset(reply_buffer, '\0', sizeof(reply_buffer));
        memset(in_buffer, '\0', sizeof(in_buffer));

        replysockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_len);

        if (getnameinfo((struct sockaddr *) &cli_addr, client_addr_len, 
            client_hostname, 100, NULL, 0, 0) != 0) {
            perror("Could not get client hostname");
        }

        chilog(INFO, "Received connection from client: %s", client_hostname);
        while (read(replysockfd, in_buffer, 255) > 0) {
            strip_carriage_return(in_buffer);
            chilog(DEBUG, "Raw message: %s", in_buffer);
            message *msg = malloc(sizeof(message));
            msg->prefix = NULL;
            msg->cmd = NULL;
            parse_message(in_buffer, msg);
            log_message(msg);
            if (strcmp(msg->cmd, "NICK") == 0) {
                add_user(client_hostname, msg->args[0]);
            } else if (strcmp(msg->cmd, "USER") == 0) {
                handle_user_msg(client_hostname, msg, reply_buffer);
            } else {
                chilog(WARNING, "Received unknown command %s", msg->cmd);
            }

            if (strlen(reply_buffer) > 0) {
                chilog(INFO, "reply: %s", reply_buffer);
                if (send(replysockfd, reply_buffer, 512, 0) == -1) {
                    perror("could not send a reply!");
                }
            }
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

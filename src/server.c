#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "reply.h"
#include "server.h"

struct sockaddr_in init_socket(int port) {
    struct sockaddr_in addr;
    bzero((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;         // ipv4
    addr.sin_addr.s_addr = INADDR_ANY; // listen on all
    addr.sin_port = htons(port);       // port
    return addr;
}

void create_rpl_welcome(const user *client, char *reply) {
    char reply_msg[100]; // TODO - get len?
    sprintf(reply_msg, ":Welcome to the Internet Relay Network %s!%s@%s",
        client->nick, client->username, client->hostname);
    char *reply_args[2] = {client->nick, reply_msg};
    write_reply(":localhost", RPL_WELCOME, reply_args, 2, reply);
}

void handle_nick_msg(const message *msg, user *client, char *reply) {
    char *nick = msg->args[0];
    if (!(update_nick(nick, client))) {
        // send ERR_NICKNAMEINUSE
    }
}

void handle_user_msg(const message *msg, user *client, char *reply) {
    // send ERR_ALREADYREGISTERED
}

void handle_msg(const message *msg, user *client, char *reply) {
    if (strcmp(msg->cmd, "NICK") == 0) {
        handle_nick_msg(msg, client, reply);
    } else if (strcmp(msg->cmd, "USER") == 0) {
        handle_user_msg(msg, client, reply);
    } else {
        chilog(WARNING, "Received unknown command %s", msg->cmd);
    }
}

void handle_registration(const message *msg, user *client, char *reply) {
    if (strcmp(msg->cmd, "NICK") == 0) {
        char *nick = msg->args[0];
        if (is_nick_in_use(nick, true)) {
            // send ERR_NICKNAMEINUSE
        } else {
            if (client->nick != NULL) {
                free(client->nick);
            }
            client->nick = malloc(strlen(nick) + 1);
            strcpy(client->nick, nick);
        }
    } else if (strcmp(msg->cmd, "USER") == 0) {
        char *username = msg->args[0]; // don't handle args 1 and 2 for now
        char *full_name = msg->args[3];
        if (client->username != NULL) {
            free(client->username);
        }
        if (client->full_name != NULL) {
            free(client->full_name);
        }
        client->username = malloc(strlen(username) + 1);
        client->full_name = malloc(strlen(full_name) + 1);
        strcpy(client->username, username);
        strcpy(client->full_name, full_name);
    } else {
        chilog(WARNING, "Received command %s before completing registration", msg->cmd);
    }

    if (is_user_complete(client)) {
        if (register_user(client)) {
            client->is_registered = true;
            create_rpl_welcome(client, reply);
        } else {
            // send ERR_NICKNAMEINUSE
        }
    }
}

void *handle_client(void *args) {
    user *client = (user *) args;
    int clientsock = client->clientsock;
    int client_addr_len = client->client_addr_len;
    struct sockaddr_in *clientaddr = client->clientaddr;

    char in_buffer[512], next_message[512], reply_buffer[512];
    char client_hostname[100];
    memset(reply_buffer, '\0', sizeof(reply_buffer));
    memset(in_buffer, '\0', sizeof(in_buffer));
    memset(next_message, '\0', sizeof(next_message));

    if (getnameinfo((struct sockaddr *) clientaddr, client_addr_len, 
        client_hostname, 100, NULL, 0, 0) != 0) {
        perror("Could not get client hostname");
    }
    client->hostname = malloc(strlen(client_hostname) + 1);
    strcpy(client->hostname, client_hostname);

    chilog(INFO, "Received connection from client: %s", client_hostname);
    while (read_full_message(clientsock, in_buffer, next_message)) {
        message *msg = malloc(sizeof(message));
        msg->prefix = NULL;
        msg->cmd = NULL;
        parse_message(in_buffer, msg);
        log_message(msg);
 
        if (client->is_registered) {
            handle_msg(msg, client, reply_buffer);
        } else {
            handle_registration(msg, client, reply_buffer);
        }

        if (strlen(reply_buffer) > 0) {
            chilog(INFO, "reply: %s", reply_buffer);
            if (send(clientsock, reply_buffer, 512, 0) == -1) {
                perror("could not send a reply!");
            }
        }
        strcpy(in_buffer, next_message);
        memset(next_message, '\0', sizeof(message));
    }
    close(clientsock);
    return NULL;
}

void run_server(int port) {
    int sockfd, replysockfd;
    // handle_client doesn't return anything useful so run in detached thread
    // and reuse the same pthread_t since we don't need to join
    pthread_t tid;
    pthread_attr_t thread_attrs;
    pthread_attr_init(&thread_attrs);
    pthread_attr_setdetachstate(&thread_attrs, PTHREAD_CREATE_DETACHED);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = init_socket(port);
    bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(sockfd, 5);
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t client_addr_len = sizeof(cli_addr);

        replysockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_len);
        user client;
        init_user(replysockfd, client_addr_len, &cli_addr, &client);
        pthread_create(&tid, &thread_attrs, handle_client, (void *) &client);
    }
    close(sockfd);
}
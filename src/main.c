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
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "log.h"
#include "message.h"
#include "user.h"
#include "reply.h"

void create_rpl_welcome(const user *user, char *reply) {
    char reply_msg[100]; // TODO - get len?
    sprintf(reply_msg, ":Welcome to the Internet Relay Network %s!%s@%s",
        user->nick, user->username, user->hostname);
    char *reply_args[2] = {user->nick, reply_msg};
    write_reply(":localhost", RPL_WELCOME, reply_args, 2, reply);
}

void handle_nick_msg(const char *hostname, const message *msg, char *reply) {
    user *new_user = get_user(hostname);

    char *nick = msg->args[0];
    new_user->nick = malloc(strlen(nick) + 1);
    strcpy(new_user->nick, nick);

    if (is_user_complete(new_user)) {
        create_rpl_welcome(new_user, reply);
    }
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
    user *new_user = get_user(hostname);

    char *username = msg->args[0]; // don't handle args 1 and 2 for now
    char *full_name = msg->args[3];
    new_user->username = malloc(strlen(username) + 1);
    new_user->full_name = malloc(strlen(full_name) + 1);
    strcpy(new_user->username, username);
    strcpy(new_user->full_name, full_name);

    if (is_user_complete(new_user)) {
        create_rpl_welcome(new_user, reply);
    }
}

void run_server(int port) {
    int sockfd, replysockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = init_socket(port);
    // TODO: error checking?
    bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    listen(sockfd, 5);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t client_addr_len = sizeof(cli_addr);

        char in_buffer[512], next_message[512], reply_buffer[512];
        char client_hostname[100];
        memset(reply_buffer, '\0', sizeof(reply_buffer));
        memset(in_buffer, '\0', sizeof(in_buffer));
        memset(next_message, '\0', sizeof(next_message));

        replysockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &client_addr_len);

        if (getnameinfo((struct sockaddr *) &cli_addr, client_addr_len, 
            client_hostname, 100, NULL, 0, 0) != 0) {
            perror("Could not get client hostname");
        }

        chilog(INFO, "Received connection from client: %s", client_hostname);
        while (read_full_message(replysockfd, in_buffer, next_message)) {
            message *msg = malloc(sizeof(message));
            msg->prefix = NULL;
            msg->cmd = NULL;
            parse_message(in_buffer, msg);
            log_message(msg);
            if (strcmp(msg->cmd, "NICK") == 0) {
                handle_nick_msg(client_hostname, msg, reply_buffer);
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
            strcpy(in_buffer, next_message);
            memset(next_message, '\0', sizeof(message));
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
    run_server(atoi(port));
    return 0;
}

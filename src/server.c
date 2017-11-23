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
#include "message.h"

// keep track of number of connections not yet registerred
int num_unregisterred = 0;

struct sockaddr_in init_socket(int port) {
    struct sockaddr_in addr;
    bzero((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;         // ipv4
    addr.sin_addr.s_addr = INADDR_ANY; // listen on all
    addr.sin_port = htons(port);       // port
    return addr;
}

void handle_nick_msg(const message *msg, user *client) {
    char *nick = msg->args[0];
    if (!(update_nick(nick, client))) {
        send_err_nicknameinuse(client, client->nick, nick);
    }
}

void handle_user_msg(const message *msg, user *client) {
    send_err_alreadyregistred(client);
}

void handle_quit_msg(const message *msg, user *client) {
    send_quit_response(client, msg->args[0]);
    close(client->clientsock);
    delete_user(client);
}

void handle_privmsg_msg(const message *msg, user *client) {
    user *recipient = get_user(msg->args[0], true);
    if (recipient) {
        send_privmsg(client, recipient, msg->args[1], "PRIVMSG");    
    } else {
        chilog(DEBUG, "sending message to unknown user %s", msg->args[0]);
        send_err_nosuchnick(client, msg->args[0]);
    }
}

void handle_notice_msg(const message *msg, user *client) {
    user *recipient = get_user(msg->args[0], true);
    if (recipient) {
        send_privmsg(client, recipient, msg->args[1], "NOTICE");    
    }
}

void handle_whois_msg(const message *msg, user *client) {
    user *target = get_user(msg->args[0], true);
    if (target) {
        send_rpl_whoisuser(client, target);
        send_rpl_whoisserver(client, target->nick);
        send_rpl_endofwhois(client, target->nick);
    } else {
        send_err_nosuchnick(client, msg->args[0]);
    }
}

void handle_ping_msg(user *client) {
    send_pong(client);
}

void handle_motd_msg(user *client) {
    FILE *motdfile;
    motdfile = fopen("motd.txt", "r");
    if (motdfile) {
        send_rpl_motdstart(client);

        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        while ((read = getline(&line, &len, motdfile)) != -1) {
            // TODO: break up lines that don't fit in a single IRC message?
            send_rpl_motd(client, line);
        }
        fclose(motdfile);
        if (line) {
            free(line);
        }

        send_rpl_endofmotd(client);
    } else {
        send_err_nomotd(client);
    }
}

void handle_lusers_msg(user *client) {
    int num_registered = get_num_users();
    send_rpl_luserclient(client, num_registered);
    send_rpl_luserop(client);
    send_rpl_luserunknown(client, num_unregisterred);
    send_rpl_luserchannels(client);
    send_rpl_luserme(client, num_unregisterred + num_registered);
}

void handle_msg(const message *msg, user *client) {
    if (strcmp(msg->cmd, "NICK") == 0) {
        handle_nick_msg(msg, client);
    } else if (strcmp(msg->cmd, "USER") == 0) {
        handle_user_msg(msg, client);
    } else if (strcmp(msg->cmd, "QUIT") == 0) {
        handle_quit_msg(msg, client);
    } else if (strcmp(msg->cmd, "PRIVMSG") == 0) {
        handle_privmsg_msg(msg, client);
    } else if (strcmp(msg->cmd, "NOTICE") == 0) {
        handle_notice_msg(msg, client);
    } else if (strcmp(msg->cmd, "WHOIS") == 0) {
        handle_whois_msg(msg, client);
    } else if (strcmp(msg->cmd, "PING") == 0) {
        handle_ping_msg(client);
    } else if (strcmp(msg->cmd, "PONG") == 0) {
    } else if (strcmp(msg->cmd, "MOTD") == 0) {
        handle_motd_msg(client);
    } else if (strcmp(msg->cmd, "LUSERS") == 0) {
        handle_lusers_msg(client);
    } else {
        chilog(WARNING, "Received unknown command %s", msg->cmd);
        send_err_unknowncommand(client, msg->cmd);
    }
}

void handle_registration(const message *msg, user *client) {
    if (strcmp(msg->cmd, "NICK") == 0) {
        char *nick = msg->args[0];
        if (get_user(nick, true)) {
            send_err_nicknameinuse(client, "*", nick);
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
    } else if (strcmp(msg->cmd, "QUIT") == 0) {
        handle_quit_msg(msg, client);
    } else {
        chilog(WARNING, "Received command %s before completing registration", msg->cmd);
    }

    if (is_user_complete(client)) {
        if (register_user(client)) {
            client->is_registered = true;
            num_unregisterred--;

            send_rpl_welcome(client);
            send_rpl_yourhost(client);
            send_rpl_created(client);
            send_rpl_myinfo(client);

            handle_lusers_msg(client);
            handle_motd_msg(client);
        } else {
            send_err_nicknameinuse(client, "*", client->nick);
        }
    }
}

void *handle_client(void *args) {
    user *client = (user *) args;
    int clientsock = client->clientsock;
    int client_addr_len = client->client_addr_len;
    struct sockaddr_in *clientaddr = client->clientaddr;
    num_unregisterred++;

    // TODO: handle larger messages more gracefully
    char in_buffer[4096], next_message[4096];
    char client_hostname[100];
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
 
        if (!is_valid(msg)) {
            continue;
        }

        if (client->is_registered) {
            handle_msg(msg, client);
        } else {
            handle_registration(msg, client);
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
        user *client = malloc(sizeof(user));
        init_user(replysockfd, client_addr_len, &cli_addr, client);
        pthread_create(&tid, &thread_attrs, handle_client, (void *) client);
    }
    close(sockfd);
}
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "user.h"
#include "log.h"

user *USERS[MAX_USERS] = {NULL};

void init_user(int clientsock, socklen_t client_addr_len,
               struct sockaddr_in *clientaddr, user *client) {
    client->clientsock = clientsock,
    client->client_addr_len = client_addr_len;
    client->clientaddr = clientaddr;

    client->is_registered = false;
    client->hostname = NULL;
    client->nick = NULL;
    client->username = NULL;
    client->full_name = NULL;
}

void free_user(user *client) {
    free(client->hostname);
    if (client->nick != NULL) {
        free(client->nick);
    }
    if (client->username != NULL) {
        free(client->username);
    }
    if (client->full_name != NULL) {
        free(client->full_name);
    }
    free(client);
}

bool is_user_complete(const user *client) {
    chilog(DEBUG, "nick: %s, username: %s, name: %s", client->nick, client->username, client->full_name);
    return client->nick != NULL && client->username != NULL && client->full_name != NULL;
}

bool is_nick_in_use(const char *nick, const bool grab_lock) {
    // grab lock
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] && USERS[i]->nick && strcmp(USERS[i]->nick, nick) == 0) {
            // release lock
            return true;
        }
    }
    // release lock
    return false;
}

bool register_user(user *client) {
    // grab lock
    if (is_nick_in_use(client->nick, false)) {
        // release lock
        return false;
    }
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] == NULL) {
            USERS[i] = client;
            // release lock
            return true;
        }
    }
    perror("Ran out of room for users");
}

void delete_user(user *client) {
    // grab lock
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] == client) {
            free_user(client);
            USERS[i] = NULL;
        }
    }
}

bool update_nick(const char *new_nick, user *client) {
    // grab lock
    if (is_nick_in_use(client->nick, false)) {
        // release lock
        return false;
    }
    if (client->nick != NULL) {
        free(client->nick);
    }
    client->nick = malloc(strlen(new_nick));
    strcpy(client->nick, new_nick);
    // release lock
    return true;
}

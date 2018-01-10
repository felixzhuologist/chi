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

user *get_user(const char *nick, const bool grab_lock) {
    if (grab_lock){
        pthread_rwlock_rdlock(&users_lock);
    }
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] && USERS[i]->nick && strcmp(USERS[i]->nick, nick) == 0) {
            if (grab_lock) {
                pthread_rwlock_unlock(&users_lock);
            }
            return USERS[i];
        }
    }
    if (grab_lock) {
        pthread_rwlock_unlock(&users_lock);
    }
    return NULL;
}

bool register_user(user *client) {
    pthread_rwlock_wrlock(&users_lock);
    if (get_user(client->nick, false)) {
        pthread_rwlock_unlock(&users_lock);
        return false;
    }
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] == NULL) {
            USERS[i] = client;
            pthread_rwlock_unlock(&users_lock);
            return true;
        }
    }
    perror("Ran out of room for users");
}

void delete_user(user *client) {
    pthread_rwlock_wrlock(&users_lock);
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] == client) {
            free_user(client);
            USERS[i] = NULL;
            break;
        }
    }
    pthread_rwlock_unlock(&users_lock);
}

bool update_nick(const char *new_nick, user *client) {
    pthread_rwlock_wrlock(&users_lock);
    if (get_user(client->nick, false)) {
        pthread_rwlock_unlock(&users_lock);
        return false;
    }
    if (client->nick != NULL) {
        free(client->nick);
    }
    client->nick = malloc(strlen(new_nick));
    strcpy(client->nick, new_nick);
    pthread_rwlock_unlock(&users_lock);
    return true;
}

int get_num_users() {
    int num_users = 0;
    pthread_rwlock_rdlock(&users_lock);
    for (int i = 0; i < MAX_USERS; i++) {
        if (USERS[i] != NULL) {
            num_users++;
        }
    }
    pthread_rwlock_unlock(&users_lock);
    return num_users;
}

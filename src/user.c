#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "user.h"
#include "log.h"

user *USERS[MAX_USERS] = {NULL};

// search through USERS and either return matching user or create new one
// and return it. Return NULL if hostname not found and no more room for users
user *get_user(const char *hostname) {
    for (int i = 0; i < MAX_USERS; i++) {
        // TODO: this assumes users are assigned sequentially and that no "holes"
        if (USERS[i] == NULL) {
            chilog(DEBUG, "allocating new user for %s", hostname);
            user *new_user = malloc(sizeof(user));
            USERS[i] = new_user;
            new_user->nick = NULL;
            new_user->username = NULL;
            new_user->full_name = NULL;
            new_user->hostname = malloc(strlen(hostname) + 1);
            strcpy(new_user->hostname, hostname);
            return new_user;
        } else if (strcmp(USERS[i]->hostname, hostname) == 0) {
            chilog(DEBUG, "found existing user for %s", hostname);
            return USERS[i];
        }
    }
    return NULL;
}

bool is_user_complete(const user *user) {
    return user->nick != NULL && user->username != NULL && user->full_name != NULL;
}

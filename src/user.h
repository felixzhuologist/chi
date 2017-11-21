#ifndef USER_H_
#define USER_H_

#include <stdbool.h>
#include <sys/socket.h>

#define MAX_USERS 100

typedef struct user {
    int clientsock;
    socklen_t client_addr_len;
    struct sockaddr_in *clientaddr;

    bool is_registered;
    char *hostname;
    char *nick;
    char *username;
    char *full_name;
} user;

void init_user(int clientsock, socklen_t client_addr_len,
               struct sockaddr_in *clientaddr, user *client);
void free_user(user *client);

// return true if all information about user (nick, username, name) is complete
// and user is ready to be registered
bool is_user_complete(const user *client);

// get user with given nick. call with grab_lock set to false if caller is managing
// the USERS lock
user *get_user(const char *nick, const bool grab_lock);

// add user to the list of users. return true on sucess, false on failure
// (i.e. nick is in use)
bool register_user(user *client);

void delete_user(user *client);

bool update_nick(const char* new_nick, user *client);

#endif /* USER_H_ */
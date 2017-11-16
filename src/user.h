#include <stdbool.h>
#include <sys/socket.h>

#define MAX_USERS 100

typedef struct user {
    int clientsock;
    socklen_t client_addr_len;
    struct sockaddr_in *clientaddr;

    char *hostname;
    char *nick;
    char *username;
    char *full_name;
} user;

// search through USERS and either return matching user or create new one
// and return it. Return NULL if hostname not found and no more room for users
user *get_user(const char *hostname);

// return true if all information about user is complete. used for RPL_WELCOME
bool is_user_complete(const user *user);
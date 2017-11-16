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

// add user to the list of users. return true on sucess, false on failure
// (i.e. nick is in use)
bool register_user(user *client);

// return true if all information about user (nick, username, name) is complete
// and user is ready to be registered
bool is_user_complete(const user *client);

/* return true if a user is already using the current nick, false otherwise
 *
 * nick: nick to check for
 * grab_lock: set to false if the caller already has the USERS lock
 */
bool is_nick_in_use(const char *nick, const bool grab_lock);

bool update_nick(const char* new_nick, user *client);

#include "message.h"
#include "user.h"

// return IPv4 sockaddr listening on all addresses and on given port
struct sockaddr_in init_socket(int port);

// create reply for a successful registration, i.e. replies 001 - 004
void create_rpl_welcome(const user *user, char *reply);

// expects user to pass in "*" if no nick has been set yet
void create_err_nicknameinuse(const char *old_nick, const char *new_nick, char *reply);

// seems like there's a typo in the protocol...
void create_err_alreadyregistred(const user *client, char *reply);

// Add username and full name specified in message for user associated with
// given hostname. Writes appropriate reply into reply arg
void handle_user_msg(const message *msg, user *client, char *reply);

// Add nick specified in message for user associated with given hostname and
// write reply into reply arg
void handle_nick_msg(const message *msg, user *client, char *reply);

// Handle message for a registered user
void handle_msg(const message *msg, user *client, char *reply);

// Handles NICK and USER messages and ignores everything else. When the user
// finally has valid information, this function registeres the new user
void handle_registration(const message *msg, user *client, char *reply);

/* entrypoint function for handling a new client
 *
 * listens for messages, sends replies, and updates global state for given 
 * client socket. expects a pointer to a user with clientsock, client_addr_len,
 * and clientaddr defined
 */
void *handle_client(void *args);

// run IRC server on given port
void run_server(int port);

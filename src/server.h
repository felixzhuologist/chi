#include "message.h"
#include "user.h"

// return IPv4 sockaddr listening on all addresses and on given port
struct sockaddr_in init_socket(int port);

// Add username and full name specified in message for user associated with
// given hostname. Writes appropriate reply into reply arg
void handle_user_msg(const message *msg, user *client);

// Add nick specified in message for user associated with given hostname and
// write reply into reply arg
void handle_nick_msg(const message *msg, user *client);

// Handle message for a registered user
void handle_msg(const message *msg, user *client);

// Handles NICK and USER messages and ignores everything else. When the user
// finally has valid information, this function registeres the new user
void handle_registration(const message *msg, user *client);

/* entrypoint function for handling a new client
 *
 * listens for messages, sends replies, and updates global state for given 
 * client socket. expects a pointer to a user with clientsock, client_addr_len,
 * and clientaddr defined
 */
void *handle_client(void *args);

// run IRC server on given port
void run_server(int port);

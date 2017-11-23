#include "message.h"
#include "user.h"

// return IPv4 sockaddr listening on all addresses and on given port
struct sockaddr_in init_socket(int port);

void handle_user_msg(const message *msg, user *client);
void handle_nick_msg(const message *msg, user *client);
void handle_privmsg_msg(const message *msg, user *client);
void handle_notice_msg(const message *msg, user *client);
void handle_whois_msg(const message *msg, user *client);

void handle_pong_msg(user *client);
void handle_motd_msg(user *client);
void handle_lusers_msg(user *client);

// Handle message for a registered user 
void handle_msg(const message *msg, user *client);

// Handles NICK and USER messages and ignores everything else. When the user
// finally has valid information, this function registers the new user by calling
// handle_registration()
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

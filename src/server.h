#include "message.h"
#include "user.h"

// return IPv4 sockaddr listening on all addresses and on given port
struct sockaddr_in init_socket(int port);

// write RPL_WELCOME message into reply arg based on input user
void create_rpl_welcome(const user *user, char *reply);

// Add username and full name specified in message for user associated with
// given hostname. Writes appropriate reply into reply arg
void handle_user_msg(const char *hostname, const message *msg, char *reply);

// Add nick specified in message for user associated with given hostname and
// write reply into reply arg
void handle_nick_msg(const char *hostname, const message *msg, char *reply);

// handle al incoming messages for a given client
void handle_client(const int clientsock, const struct sockaddr_in *clientaddr,
                   const socklen_t client_addr_len);

// run IRC server on given port
void run_server(int port);

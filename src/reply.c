#include <stdio.h>
#include <string.h>

#include "log.h"
#include "reply.h"

// TODO: these values shouldn't be hardcoded. there should be probably be
// a server struct that creates these values on init and stores them along with
// other globals like the list of clients
char *server_name = "localhost";
char *version = "v3.14";
char *created = "yesterday";
char *user_modes = "ao";
char *channel_modes = "mtov";

void send_reply(user *client, char *reply) {
  // snprintf will add a null terminator so we allocate one extra char
  // for it (but don't actually send it to the client)
  char full_reply[513];
  int msg_size = snprintf(full_reply, 513, ":%s %s\r\n", server_name, reply);
  if (msg_size > 513) {
    chilog(ERROR, "sending clipped reply longer than 512 chars");
  }

  chilog(DEBUG, "reply: %s", full_reply);
  int sent_size = send(client->clientsock, full_reply, msg_size, 0);
  if (sent_size == -1) {
    chilog(ERROR, "could not send a reply, send() returned -1");
  } else if (sent_size < msg_size) { // TODO: send entire message
    chilog(ERROR, "reply was fragmented");
  }
}
void send_rpl_welcome(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Welcome to the Internet Relay Network %s!%s@%s",
    RPL_WELCOME,client->nick,
    client->nick, client->username, client->hostname);
  send_reply(client, reply);
}

void send_rpl_yourhost(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Your host is %s, running version %s",
    RPL_YOURHOST,client->nick, server_name, version);
  send_reply(client, reply);
}

void send_rpl_created(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :This server was created %s",
    RPL_YOURHOST,client->nick, created);
  send_reply(client, reply);
}

void send_rpl_myinfo(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :%s %s %s %s",
    RPL_MYINFO,client->nick, server_name, version, user_modes, channel_modes);
  send_reply(client, reply);
}

// we need to take old_nick as an arg because the client might have nick
// defined but not be registered yet, e.g. if a new user's nickname is not in use
// when the user call's NICK but is when they call USER and we try to register
void send_err_nicknameinuse(user *client, char *old_nick, char *new_nick) {
  char reply[512];
  sprintf(reply, "%s %s %s :Nickname is already in use.",
    ERR_NICKNAMEINUSE, old_nick, new_nick);
  send_reply(client, reply);
}

void send_err_alreadyregistred(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Unauthorized command (already registered)",
    ERR_ALREADYREGISTRED, client->nick);
  send_reply(client, reply);
}

#include <stdio.h>

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
  // remove null terminator from reply
  reply[strlen(reply)] = ' ';

  // snprintf will add a null terminator so we allocate one extra char
  // for it (but don't actually send it to the client)
  char full_reply[513];
  int msg_size = snprintf(full_reply, ":%s %s\r\n", server_name, reply);
  if (msg_size > 513) {
    chilog(ERROR, "sending clipped reply longer than 512 chars");
  }

  chilog(DEBUG, "reply: %s", full_reply);
  int sent_size = send(client->clientsock, full_reply, 512, 0);
  if (sent_size == -1) {
    chilog(ERROR, "could not send a reply, send() returned -1");
  } else if (sent_size < 512) { // TODO: send entire message
    chilog(ERROR, "reply was fragmented");
  }
}
void send_rpl_welcome(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Welcome to the Internet Relay Network %s!%s@%s",
    client->nick, RPL_WELCOME,
    client->nick, client->username, client->hostname);
  send_reply(client, reply);
}

void send_rpl_yourhost(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Your host is %s, running version %s",
    client->nick, RPL_YOURHOST,
    server_name, version);
  send_reply(client, reply);
}

void send_rpl_created(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :This server was created %s",
    client->nick, RPL_YOURHOST,
    server_name, created);
  send_reply(client, reply);
}

void send_rpl_myinfo(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :%s %s %s %s",
    client->nick, RPL_MYINFO,
    server_name, version, user_modes, channel_modes);
  send_reply(client, reply);
}

// we need to take old_nick as an arg because the client might have nick
// defined but not be registered yet, e.g. if a new user's nickname is not in use
// when the user call's NICK but is when they call USER and we try to register
void send_err_nicknameinuse(user *client, char *old_nick, char *new_nick) {
  char reply[512];
  sprintf(reply, "%s %s %s %s :Nickname is already in use.",
    client->nick, ERR_NICKNAMEINUSE, old_nick, new_nick);
  send_reply(client, reply);
}

void send_err_alreadyregistred(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Unauthorized command (already registered)",
    client->nick, ERR_ALREADYREGISTRED);
  send_reply(client, reply);
}

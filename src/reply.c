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
    full_reply[510] = '\r';
    full_reply[511] = '\n';
  }

  chilog(INFO, "reply: %s", full_reply);
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
    RPL_CREATED,client->nick, created);
  send_reply(client, reply);
}

void send_rpl_myinfo(user *client) {
  char reply[512];
  sprintf(reply, "%s %s %s %s %s %s",
    RPL_MYINFO,client->nick, server_name, version, user_modes, channel_modes);
  send_reply(client, reply);
}

void send_rpl_motdstart(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :- %s Message of the day - ",
    RPL_MOTDSTART, client->nick, server_name);
  send_reply(client, reply);
}

void send_rpl_motd(user *client, char *motd_line) {
  char reply[512];
  sprintf(reply, "%s %s :- %s", RPL_MOTD, client->nick, motd_line);
  send_reply(client, reply);
}

void send_rpl_endofmotd(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :End of MOTD command", RPL_ENDOFMOTD, client->nick);
  send_reply(client, reply);
}

// we need to take old_nick as an arg because the client might have nick
// defined but not be registered yet, e.g. if a new user's nickname is not in use
// when the user call's NICK but is when they call USER and we try to register
void send_err_nicknameinuse(user *client, char *old_nick, char *new_nick) {
  char reply[512];
  sprintf(reply, "%s %s %s :Nickname is already in use",
    ERR_NICKNAMEINUSE, old_nick, new_nick);
  send_reply(client, reply);
}

void send_err_alreadyregistred(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :Unauthorized command (already registered)",
    ERR_ALREADYREGISTRED, client->nick);
  send_reply(client, reply);
}

void send_rpl_luserclient(user *client, int num_users) {
  char reply[512];
  // services and multiple servers not yet implemented
  sprintf(reply, "%s %s :There are %d users and 0 services on 1 servers",
    RPL_LUSERCLIENT, client->nick, num_users);
  send_reply(client, reply);
}

void send_rpl_luserop(user *client) {
  char reply[512];
  // operators not yet implemented
  sprintf(reply, "%s %s 0 :operator(s) online",
    RPL_LUSEROP, client->nick);
  send_reply(client, reply);
}

void send_rpl_luserunknown(user *client, int num_unknown) {
  char reply[512];
  sprintf(reply, "%s %s %d :unknown connection(s)",
    RPL_LUSERUNKNOWN, client->nick, num_unknown);
  send_reply(client, reply);
}

void send_rpl_luserchannels(user *client) {
  char reply[512];
  // channels not yet implemented
  sprintf(reply, "%s %s 0 :channels formed",
    RPL_LUSERCHANNELS, client->nick);
  send_reply(client, reply);
}

void send_rpl_luserme(user *client, int num_clients) {
  char reply[512];
  sprintf(reply, "%s %s :I have %d clients and 1 servers",
    RPL_LUSERME, client->nick, num_clients);
  send_reply(client, reply);
}

void send_rpl_whoisuser(user *client, user *target) {
  char reply[512];
  sprintf(reply, "%s %s %s %s %s * :%s",
    RPL_WHOISUSER, client->nick,
    target->nick, target->username, server_name, target->full_name);
  send_reply(client, reply);
}

void send_rpl_whoisserver(user *client, char *target_nick) {
  char reply[512];
  sprintf(reply, "%s %s %s %s :some server info",
    RPL_WHOISSERVER, client->nick, target_nick, server_name);
  send_reply(client, reply);
}

void send_rpl_endofwhois(user *client, char *target_nick) {
  char reply[512];
  sprintf(reply, "%s %s %s :End of WHOIS list",
    RPL_ENDOFWHOIS, client->nick, target_nick);
  send_reply(client, reply);
}

void send_err_nomotd(user *client) {
  char reply[512];
  sprintf(reply, "%s %s :MOTD File is missing",
    ERR_NOMOTD, client->nick);
  send_reply(client, reply);
}

void send_err_nosuchnick(user *client, char *nick) {
  char reply[512];
  sprintf(reply, "%s %s %s :No such nick/channel",
    ERR_NOSUCHNICK, client->nick, nick);
  send_reply(client, reply);
}

void send_err_unknowncommand(user *client, char *cmd) {
  char reply[512];
  sprintf(reply, "%s %s %s :Unknown command", ERR_UNKNOWNCOMMAND, client->nick, cmd);
  send_reply(client, reply);
}

void send_quit_response(user *client, char *message) {
  char reply[512];
  const char *reason = message == NULL ? "Client Quit" : message;
  sprintf(reply, "ERROR :Closing Link: %s (%s)", client->hostname, reason);
  send_reply(client, reply);
}

// TODO: refactor duplicate code from send_reply
void send_privmsg(user *client, user *recipient, char *message, char *message_type) {
  char reply[513];
  int msg_size = snprintf(reply, 513, ":%s!%s@%s %s %s :%s\r\n",
    client->nick, client->username, client->hostname, message_type, recipient->nick, message);
  if (msg_size > 513) {
    chilog(ERROR, "sending clipped privmsg longer than 512 chars");
    reply[510] = '\r';
    reply[511] = '\n';
  }

  int sent_size = send(recipient->clientsock, reply, strlen(reply), 0);
  if (sent_size == -1) {
    chilog(ERROR, "could not send a reply, send() returned -1");
  } else if (sent_size < msg_size) { // TODO: send entire message
    chilog(ERROR, "reply was fragmented");
  }
}

void send_pong(user *client) {
  char reply[512];
  sprintf(reply, "PONG %s", server_name);
  send_reply(client, reply);
}

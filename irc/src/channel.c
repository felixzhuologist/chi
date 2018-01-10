#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "channel.h"
#include "log.h"

channel *CHANNELS[MAX_CHANNELS] = {NULL};

void init_channel(char *name, channel *new_channel) {
  // strncpy doesn't implicitly add null terminator
  if (strlen(name) > 50) {
    name[49] = '\0';
  }
  strncpy(new_channel->name, name, 50);
  // not sure what the right way to do this is, can't use = {NULL}
  memset(new_channel->members, 0, sizeof(new_channel->members));
  memset(new_channel->msgs, 0, sizeof(new_channel->msgs));
  chilog(INFO, "Creating new channel %s", new_channel->name);
}

channel *get_channel(const char *name) {
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (CHANNELS[i] && (strcmp(CHANNELS[i]->name, name) == 0)) {
      return CHANNELS[i];
    }
  }
  return NULL;
}

void add_channel(channel *new_channel) {
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (CHANNELS[i] == NULL) {
      CHANNELS[i] = new_channel;
      return;
    } 
  }
  perror("Ran out of room for channels");
}

void add_msg(archived_msg *msg, channel *channel) {
  for (int i = 0; i < MAX_SAVED_MSGS; i++) {
    if (channel->msgs[i] == NULL) {
      channel->msgs[i] = msg;
      return;
    }
  }
  perror("Ran out of room for storing messages");
}

bool is_member(const user *user, const channel *channel) {
  for (int i = 0; i < MAX_CHANNEL_MEMBERS; i++) {
    if (channel->members[i] && (strcmp(channel->members[i]->nick, user->nick) == 0)) {
      return true;
    }
  }
  return false;
}

void add_member(user *member, channel *channel) {
  for (int i = 0; i < MAX_CHANNEL_MEMBERS; i++) {
    if (channel->members[i] == NULL) {
      channel->members[i] = member;
      return;
    }
  }
  perror("Ran out of room for adding users to channel");
}

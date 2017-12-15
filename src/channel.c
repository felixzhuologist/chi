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

void add_channel(channel *new_channel) {
  for (int i = 0; i < MAX_CHANNEL_MEMBERS; i++) {
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
      break;
    }
  }
}

channel *get_channel(const char *name) {
  for (int i = 0; i < MAX_CHANNEL_MEMBERS; i++) {
    if (CHANNELS[i] && (strcmp(CHANNELS[i]->name, name) == 0)) {
      return CHANNELS[i];
    }
  }
  return NULL;
}

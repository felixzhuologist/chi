#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "channel.h"

channel *CHANNELS[MAX_CHANNELS] = {NULL};

void init_channel(char *name, channel *new_channel) {
  // strncpy doesn't implicitly add null terminator
  if (strlen(name) > 50) {
    name[49] = '\0';
  }
  strncpy(new_channel->name, name, 50);
  new_channel->members = {NULL};
}

void add_channel(channel *new_channel) {
  for (int i = 0; i < MAX_CHANNEL_MEMBERS; i++) {
    if (CHANNELS[i] == NULL) {
      CHANNELS[i] = new_channel;
    } 
  }
  perror("Ran out of room for channels");
}

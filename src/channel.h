#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <pthread.h>
#include "user.h"

#define MAX_CHANNELS 100
#define MAX_CHANNEL_MEMBERS 100

pthread_rwlock_t channels_lock;

typedef struct channel {
  char name[50];
  // topic?
  user members[MAX_CHANNEL_MEMBERS];
} channel;

void init_channel(char *name, channel *new_channel);

void add_channel(channel *new_channel);

#endif /* CHANNEL_H_ */
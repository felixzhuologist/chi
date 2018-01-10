#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <pthread.h>
#include "user.h"
#include "message.h"

#define MAX_CHANNELS 100
#define MAX_CHANNEL_MEMBERS 100
#define MAX_SAVED_MSGS 100

pthread_rwlock_t channels_lock;

typedef struct archived_msg {
  const message *msg;
  const user *sender;
} archived_msg;

typedef struct channel {
  char name[50];
  // topic?
  user *members[MAX_CHANNEL_MEMBERS];
  archived_msg *msgs[MAX_SAVED_MSGS];
} channel;

// TODO: garbage collect channels, protect access with rwlocks

void init_channel(char *name, channel *new_channel);

channel *get_channel(const char *name);
void add_channel(channel *new_channel);

void add_msg(archived_msg *msg, channel *channel);

bool is_member(const user *user, const channel *channel);
void add_member(user *member, channel *channel);

#endif /* CHANNEL_H_ */
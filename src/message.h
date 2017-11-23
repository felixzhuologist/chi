/*
 * Utility functions for processing IRC messages
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdio.h>
#include <stdbool.h>

#include "log.h"

typedef struct message {
    char *prefix;
    char *cmd;
    char *args[15];
} message;

#define CHUNK_SIZE 512

// return index of first carriage return or size if it's not found
// everything in the string up to returned index should be part of a single message
int find_cr(const char *str, const int size);

// pretty print prefix, cmd, args of a message at the DEBUG level
void log_message(const message *msg);

/*
 * Parse contents of buffer into a message struct
 *
 * Processes string buffer expected to contain a well formed IRC message
 * and allocates/writes values into provided message struct. Any values
 * not found in the buffer e.g. unused prefix, args, are set to NULL
 */
void parse_message(char *buffer, message *msg);

/*
 * Read a full CR terminated IRC message from socket
 *
 * This function reads from socket in CHUNK_SIZE chunks into message until message
 * contains a CR (if message already contains a CR then no reading occurs).
 * The CR is stripped and the contents of message after the CR (i.e. the start
 * of the next message(s)) is copied into next_message for future use.
 * Returns true if it was possible to read in a full IRC message, and false otherwise
 */
bool read_full_message(const int sockfd, char *message, char *next_message);

bool is_valid(message *msg);

#endif /* MESSAGE_H_ */
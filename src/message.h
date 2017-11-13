/*
 * Utility functions for processing IRC messages
 */

#include <stdio.h>

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
 * Write IRC message values into buffer to be written to a socket
 *
 * Essentially just writes values, adding spaces as appropriate into the
 * buffer. Does not add colons to the prefix or the last argument
 */
void write_reply(const char *prefix, const char *cmd, const char **args, 
                 const int num_args, char *buffer);

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

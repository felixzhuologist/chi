
#include <stdio.h>

#include "log.h"

typedef struct message {
    char *prefix;
    char *cmd;
    char *args[15];
} message;

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

// expects prefix and last argument to already be prefixed with :
/*
 * Write IRC message values into buffer to be written to a socket
 *
 * Essentially just writes values, adding spaces as appropriate into the
 * buffer. Does not add colons to the prefix or the last argument
 */
void write_reply(const char *prefix, const char *cmd, const char **args, 
                 const int num_args, char *buffer);

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "message.h"

void log_message(const message *msg) {
    if (msg->prefix != NULL) {
        chilog(INFO, "prefix: %s", msg->prefix);
    }
    if (msg->cmd != NULL) {
       chilog(INFO, "cmd: %s", msg->cmd); 
    }
    chilog(INFO, "args:");
    int i = 0;
    while (msg->args[i] != NULL) {
        chilog(INFO, "\t%s", msg->args[i]);
        i++;
    }
}

void parse_message(char *buffer, message *msg) {
    if (strlen(buffer) > 512) {
        buffer[512] = '\0';
    }

    char *token = strtok(buffer, " ");
    // prefix
    if (token != NULL && strlen(token) > 0 && token[0] == ':') {
        msg->prefix = malloc(strlen(token));
        strcpy(msg->prefix, token + 1);
        token = strtok(NULL, " ");
    }

    // command
    if (token != NULL) {
        msg->cmd = malloc(strlen(token) + 1);
        strcpy(msg->cmd, token);
        token = strtok(NULL, " ");
    }

    // args
    for (int i = 0; i < 15; i++) {
        msg->args[i] = NULL;
        if (token == NULL) { // keep setting the rest of the entries to null
            continue;
        }
        if (strlen(token) > 0 && token[0] == ':') {
            char *rest = strtok(NULL, "");
            msg->args[i] = malloc(strlen(token) + strlen(rest));
            strcpy(msg->args[i], token + 1);
            strcat(msg->args[i], " ");
            strcat(msg->args[i], rest);
        } else {
            msg->args[i] = malloc(strlen(token));
            strcpy(msg->args[i], token);
        }
        token = strtok(NULL, " ");        
    }
}

// expects prefix and last argument to already be prefixed with :
void write_reply(const char *prefix, const char *cmd, const char **args, 
                 const int num_args, char *buffer) {
    int i = 0;

    // prefix
    if (prefix != NULL && strlen(prefix) > 0) {
        strcpy(buffer + i, prefix);
        i += strlen(prefix);
        buffer[i++] = ' ';
    }

    // cmd
    strcpy(buffer + i, cmd);
    i += strlen(cmd);

    // args
    for (int j = 0; j < (num_args < 15 ? num_args : 15); j++) {
        if (args[j] == NULL && strlen(args[j]) > 0) {
            break;
        }
        buffer[i++] = ' ';
        strcpy(buffer + i, args[j]);
        i += strlen(args[j]);
    }

    buffer[i++] = '\r';
    buffer[i++] = '\n';
}
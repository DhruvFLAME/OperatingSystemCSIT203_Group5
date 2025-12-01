#ifndef UTILS_H
#define UTILS_H

#include "server.h"

void trim_whitespace(char *s);
void send_help(int sock, client_chat_state_t *state);

#endif

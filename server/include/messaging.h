#ifndef MESSAGING_H
#define MESSAGING_H

void broadcast_message(const char *sender, const char *msg);
void send_private_message(const char *sender, const char *receiver, const char *msg);
void send_to_user(const char *from, const char *to, const char *message);

#endif

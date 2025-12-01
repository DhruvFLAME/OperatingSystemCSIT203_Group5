#ifndef DATABASE_H
#define DATABASE_H
#include <stddef.h>   // for size_t

void init_database(const char *filename);
void store_message(const char *sender, const char *receiver, const char *text);
void handle_getmessages_db_and_send(const char *requester, int requester_sock, const char *target);
void handle_deletemessages_db(const char *user_a, const char *user_b, int requester_sock);
void get_messages_for_user(const char *username, char *out, size_t out_size);

#endif

#ifndef CLIENTS_H
#define CLIENTS_H

#include "server.h"

int add_client(int sock, const char *username);
void remove_client_by_sock(int sock);
int find_sock_by_username(const char *username);
int user_exists(const char *username);
void send_to_sock(int sock, const char *msg);

void handle_getuserlist(int requester_sock);
void broadcast_shutdown_and_close_all();
void get_all_users(char *out, size_t out_size);

#endif

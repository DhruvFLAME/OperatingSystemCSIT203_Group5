#ifndef MENU_H
#define MENU_H

#include "server.h"  // <-- add this so client_chat_state_t is known

void handle_menu_chatrooms(const char *username, int sock, client_chat_state_t *state);
void handle_menu(const char *username, int sock, client_chat_state_t *state);
void menu_start_open_chat(const char *username, int sock);
void menu_start_closed_chat(const char *username, int sock, client_chat_state_t *state);
void menu_start_semiclosed_chat(const char *username, int sock, client_chat_state_t *state);
void menu_view_users(int sock);
void menu_view_messages(const char *username, int sock, client_chat_state_t *state);

#endif

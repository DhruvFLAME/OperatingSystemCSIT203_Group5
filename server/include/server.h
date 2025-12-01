#ifndef SERVER_H
#define SERVER_H

#include <sqlite3.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define BUF_SIZE 1024
#define USERNAME_LEN 32
#define MAX_ROOM_USERS 5

/* global state (defined in main.c) */
extern int server_fd;
extern int running;
extern sqlite3 *db;
extern pthread_mutex_t db_lock;
extern pthread_mutex_t clients_lock;

typedef enum {
    OPEN_CHAT = 0,
    CLOSED_CHAT,
    SEMI_CLOSED_CHAT
} chat_mode_t;

typedef struct {
    chat_mode_t mode;
    char chat_partner[USERNAME_LEN];
    char room_partners[MAX_ROOM_USERS][USERNAME_LEN];
    int room_size;
} client_chat_state_t;

typedef struct {
    int sock;
    char username[USERNAME_LEN];
    int active;
} client_t;

extern client_t clients[MAX_CLIENTS];

#endif /* SERVER_H */

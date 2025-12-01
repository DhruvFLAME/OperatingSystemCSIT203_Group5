#include "clients.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>   // ← REQUIRED for send()
#include <arpa/inet.h>    // optional but recommended for sockaddr_in

/* client array `clients` declared in server.h as extern and defined in main.c */

/* add_client, remove_client_by_sock, find_sock_by_username, user_exists, send_to_sock */

int add_client(int sock, const char *username) {
    pthread_mutex_lock(&clients_lock);
    int stored = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].sock = sock;
            strncpy(clients[i].username, username, USERNAME_LEN - 1);
            clients[i].username[USERNAME_LEN - 1] = '\0';
            clients[i].active = 1;
            stored = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return stored;
}

void remove_client_by_sock(int sock) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].sock == sock) {
            clients[i].active = 0;
            clients[i].sock = 0;
            clients[i].username[0] = '\0';
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

int find_sock_by_username(const char *username) {
    int sock = -1;
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            sock = clients[i].sock;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return sock;
}

int user_exists(const char *username) {
    int exists = 0;
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            exists = 1; break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return exists;
}

void send_to_sock(int sock, const char *msg) {
    if (sock <= 0) return;
    ssize_t r = send(sock, msg, strlen(msg), 0);
    (void)r;
}

/* get_all_users used by menu_view_users */
void get_all_users(char *out, size_t out_size)
{
    out[0] = '\0';

    pthread_mutex_lock(&clients_lock);

    int found = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            strncat(out, clients[i].username, out_size - strlen(out) - 1);
            strncat(out, "\n", out_size - strlen(out) - 1);
            found = 1;
        }
    }

    pthread_mutex_unlock(&clients_lock);

    if (!found) {
        strncat(out, "(no users)\n", out_size - strlen(out) - 1);
    }
}

/* broadcast shutdown to all clients and close sockets */
void broadcast_shutdown_and_close_all() {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            const char *msg = "Server shutting down...\n";
            send(clients[i].sock, msg, strlen(msg), 0);
            close(clients[i].sock);
            clients[i].active = 0;
            clients[i].sock = 0;
            clients[i].username[0] = '\0';
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

void handle_getuserlist(int requester_sock) {
    char line[128];
    pthread_mutex_lock(&clients_lock);
    int found = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            snprintf(line, sizeof(line), "%s\n", clients[i].username);
            send_to_sock(requester_sock, line);
            found = 1;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    if (!found) send_to_sock(requester_sock, "(no users)\n");
}

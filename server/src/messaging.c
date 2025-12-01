#include "messaging.h"
#include "clients.h"
#include "database.h"
#include "logging.h"
#include "utils.h"
#include "server.h"
#include <string.h>     // strlen, strcpy if used
#include <stdio.h>      // <-- for snprintf

void broadcast_message(const char *sender, const char *msg)
{
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "[Broadcast] %s: %s", sender, msg);

    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            send_to_sock(clients[i].sock, buf);
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

void send_to_user(const char *from, const char *to, const char *message) {
    char final[BUF_SIZE];
    int n = snprintf(final, sizeof(final), "%s -> %s: %s\n", from, to, message);
    if (n >= (int)sizeof(final)) final[sizeof(final)-1] = '\0';

    store_message(from, to, message);

    int sock = find_sock_by_username(to);
    if (sock > 0) {
        send_to_sock(sock, final);
        log_info("%s sent message to %s (delivered)", from, to);
    } else {
        log_info("%s sent message to %s (stored - offline)", from, to);
    }
}

void send_private_message(const char *sender, const char *receiver, const char *msg)
{
    /* Trim newline */
    char clean[BUF_SIZE];
    snprintf(clean, sizeof(clean), "%s", msg);
    clean[strcspn(clean, "\r\n")] = 0;

    send_to_user(sender, receiver, clean);
}

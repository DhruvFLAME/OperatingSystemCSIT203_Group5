#include "server.h"
#include "clients.h"
#include "utils.h"
#include "database.h"
#include "messaging.h"
#include "menu.h"
#include "logging.h"
#include "client_thread.h"

#include <stdlib.h>
#include <string.h>       // strlen(), memset()
#include <unistd.h>       // close()
#include <stdio.h>        // snprintf(), printf()
#include <sys/types.h>
#include <sys/socket.h>   // recv(), send(), accept()
#include <netinet/in.h>   // sockaddr_in
#include <arpa/inet.h>    // htons(), inet_ntoa


void *client_thread(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char buffer[BUF_SIZE];
    char username[USERNAME_LEN] = {0};
    client_chat_state_t state;
    state.mode = OPEN_CHAT;
    state.chat_partner[0] = '\0';
    state.room_size = 0;

    /* login prompt */
    send_to_sock(sock,
        "Welcome to the Messaging Server!\n"
        "Type: login <username>\n");
    ssize_t bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0) { close(sock); return NULL; }
    buffer[bytes] = '\0';
    trim_whitespace(buffer);

    if (strncmp(buffer, "login ", 6) == 0) {
        strncpy(username, buffer+6, USERNAME_LEN-1);
    } else {
        strncpy(username, buffer, USERNAME_LEN-1);
    }
    username[USERNAME_LEN-1] = '\0';
    trim_whitespace(username);

    if (strlen(username) == 0) {
        send_to_sock(sock, "ERROR: empty username\n");
        close(sock);
        return NULL;
    }
    if (user_exists(username)) {
        send_to_sock(sock, "ERROR: username already in use\n");
        close(sock);
        return NULL;
    }
    if (!add_client(sock, username)) {
        send_to_sock(sock, "ERROR: server full\n");
        close(sock);
        return NULL;
    }

    /* send welcome & help */
    {
        char welcome[512];
        snprintf(welcome, sizeof(welcome),
            "Welcome, %s!\nType 'help' for commands.\n", username);
        send_to_sock(sock, welcome);
        send_help(sock, &state);
    }
    log_info("Client connected: %s (sock=%d)", username, sock);

    /* main loop */
    while (running) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t len = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (len <= 0) break;
        buffer[len] = '\0';
        trim_whitespace(buffer);
        if (strlen(buffer) == 0) continue;

        /* In CLOSED_CHAT we accept slash-commands or plain messages */
        if (state.mode == CLOSED_CHAT) {
            if (buffer[0] == '/') {
                /* slash commands */
                if (strncmp(buffer+1, "open", 4) == 0) {
                    state.mode = OPEN_CHAT;
                    state.chat_partner[0] = '\0';
                    send_to_sock(sock, "Returned to OPEN_CHAT mode.\n");
                    continue;
                } else if (strncmp(buffer+1, "menu", 4) == 0) {
                    handle_menu_chatrooms(username, sock, &state);;
                    continue;
                } else if (strncmp(buffer+1, "exit", 4) == 0) {
                    send_to_sock(sock, "Goodbye\n");
                    break;
                } else if (strncmp(buffer+1, "help", 4) == 0) {
                    send_help(sock, &state);
                    continue;
                } else if (strncmp(buffer+1, "users", 5) == 0) {
                    handle_getuserlist(sock);
                    continue;
                } else {
                    send_to_sock(sock, "Unknown slash command in closed chat. /help\n");
                    continue;
                }
            } else if (buffer[0] == '\\') {
                /* also accept backslash as alternative */
                send_to_sock(sock, "Use /command for commands. To send message, just type it.\n");
                continue;
            } else {
                /* Plain message send to chat partner (full buffer) */
                if (strlen(state.chat_partner) == 0) {
                    send_to_sock(sock, "No chat partner set. Type /menu then select.\n");
                    state.mode = OPEN_CHAT;
                    continue;
                }
                /* ensure partner exists historically — but allow sending even if offline */
                send_to_user(username, state.chat_partner, buffer);
                send_to_sock(sock, "Message sent ✓\n");
                continue;
            }
        }

        /* OPEN_CHAT or other modes: parse commands */
        char *saveptr = NULL;
        char *cmd = strtok_r(buffer, " ", &saveptr);
        if (!cmd) continue;

        if (strcasecmp(cmd, "Chat") == 0) {
            char *target = strtok_r(NULL, " ", &saveptr);
            char *msg = (target) ? strtok_r(NULL, "", &saveptr) : NULL;
            if (!target || !msg) {
                send_to_sock(sock, "ERROR: usage: Chat <user> <message>\n");
                continue;
            }
            trim_whitespace(target);
            trim_whitespace(msg);
            if (!user_exists(target)) {
                send_to_sock(sock, "ERROR: target username does not exist\n");
                continue;
            }
            send_to_user(username, target, msg);
            send_to_sock(sock, "Message sent ✓\n");
        }
        else if (strcasecmp(cmd, "getmessages") == 0) {
            char *target = strtok_r(NULL, " ", &saveptr);
            if (!target) { send_to_sock(sock, "ERROR: usage getmessages <user>\n"); continue; }
            trim_whitespace(target);
            handle_getmessages_db_and_send(username, sock, target);
        }
        else if (strcasecmp(cmd, "deletemessages") == 0) {
            char *target = strtok_r(NULL, " ", &saveptr);
            if (!target) { send_to_sock(sock, "ERROR: usage deletemessages <user>\n"); continue; }
            trim_whitespace(target);
            handle_deletemessages_db(username, target, sock);
        }
        else if (strcasecmp(cmd, "getuserlist") == 0) {
            handle_getuserlist(sock);
        }
        else if (strcasecmp(cmd, "Menu") == 0 || strcasecmp(cmd, "menu") == 0) {
            handle_menu_chatrooms(username, sock, &state);;
        }
        else if (strcasecmp(cmd, "select") == 0) {
            char *partner = strtok_r(NULL, " ", &saveptr);
            if (!partner) { send_to_sock(sock, "ERROR: usage select <username>\n"); continue; }
            trim_whitespace(partner);
            if (!user_exists(partner)) {
                send_to_sock(sock, "ERROR: user not connected/known\n");
                continue;
            }
            /* Enter CLOSED_CHAT with partner */
            strncpy(state.chat_partner, partner, USERNAME_LEN-1);
            state.chat_partner[USERNAME_LEN-1] = '\0';
            state.mode = CLOSED_CHAT;
            {
                char m[128];
                snprintf(m, sizeof(m), "Entered CLOSED_CHAT with %s. Type messages directly to send.\n", state.chat_partner);
                send_to_sock(sock, m);
                send_to_sock(sock, "Type /open to return to open chat, /menu to view menu, /exit to quit.\n");
            }
        }
        else if (strcasecmp(cmd, "open") == 0) {
            state.mode = OPEN_CHAT;
            state.chat_partner[0] = '\0';
            send_to_sock(sock, "Switched to OPEN_CHAT mode.\n");
        }
        else if (strcasecmp(cmd, "help") == 0) {
            send_help(sock, &state);
        }
        else if (strcasecmp(cmd, "exit") == 0) {
            send_to_sock(sock, "Goodbye\n");
            break;
        }
        else {
            send_to_sock(sock, "ERROR: Unknown command. Type 'help'\n");
        }
    }

    remove_client_by_sock(sock);
    close(sock);
    log_info("Connection closed for %s", username);
    return NULL;
}

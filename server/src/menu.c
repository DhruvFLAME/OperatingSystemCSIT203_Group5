#include "menu.h"
#include "utils.h" // client_chat_state_t is defined there
#include "clients.h"
#include "messaging.h"
#include "database.h"
#include "server.h"

#include <stdio.h>        // snprintf(), printf()
#include <string.h>       // strlen(), memset(), strcmp(), etc.
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>   // send(), recv(), socket()
#include <netinet/in.h>   // struct sockaddr_in
#include <arpa/inet.h>    // inet_addr(), htons(), htonl()
#include <unistd.h>       // close()

/* show distinct chat partners for username */
void handle_menu_chatrooms(const char *username, int sock, client_chat_state_t *state) {
    char buf[BUF_SIZE];
    int n;

    while (1) {
        pthread_mutex_lock(&db_lock);

        sqlite3_stmt *stmt = NULL;
        const char *sql =
            "SELECT DISTINCT CASE WHEN sender = ? THEN receiver WHEN receiver = ? THEN sender END as partner "
            "FROM messages WHERE sender = ? OR receiver = ? ORDER BY partner ASC;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            pthread_mutex_unlock(&db_lock);
            send_to_sock(sock, "ERROR: DB prepare failed\n");
            return;
        }

        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, username, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, username, -1, SQLITE_TRANSIENT);

        int i = 0;
        send_to_sock(sock, "---- Menu: Chat Rooms ----\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *partner = sqlite3_column_text(stmt, 0);
            if (partner && strlen((const char*)partner) > 0) {
                snprintf(buf, sizeof(buf), "%d) Chat with %s\n", i + 1, (const char*)partner);
                send_to_sock(sock, buf);
                i++;
            }
        }
        if (i == 0)
            send_to_sock(sock, "(no chat rooms)\n");

        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_lock);

        send_to_sock(sock, "Commands: select <username>   back   listusers   help\n");

        // Receive user input
        memset(buf, 0, sizeof(buf));
        n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) return;
        buf[n] = '\0';
        trim_whitespace(buf);

        // Handle commands
        if (strcasecmp(buf, "back") == 0) {
            send_help(sock, state); // <-- go back to main help after exiting
          return;
        }
        else if (strcasecmp(buf, "listusers") == 0) {
            menu_view_users(sock);
        }
        else if (strcasecmp(buf, "help") == 0) {
            send_help(sock, state);
        }
        else if (strncasecmp(buf, "select ", 7) == 0) {
            char *partner = buf + 7;
            trim_whitespace(partner);
            if (!user_exists(partner)) {
                send_to_sock(sock, "ERROR: User does not exist.\n");
                continue;
            }
            // Start closed chat with selected partner
            menu_start_closed_chat(username, sock, state);;
        }
        else {
            send_to_sock(sock, "Unknown command. Type 'help'\n");
        }
    }
}

/* interactive menu (text choices) */
void handle_menu(const char *username, int sock, client_chat_state_t *state)
{
    char buf[512];
    int n;

menu_start:

    snprintf(buf, sizeof(buf),
        "=== MAIN MENU ===\n"
        "1. Start Open Chat\n"
        "2. Start Closed Chat (one partner)\n"
        "3. Start Semi-Closed Chat (multiple partners)\n"
        "4. View user list\n"
        "5. View stored messages\n"
        "6. Exit menu\n"
        "Enter choice:\n"
    );
    send(sock, buf, strlen(buf), 0);

    memset(buf, 0, sizeof(buf));
    n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) return;

    int choice = atoi(buf);

    switch (choice)
    {
        case 1:
            menu_start_open_chat(username, sock);
            goto menu_start;

        case 2:
            menu_start_closed_chat(username, sock, state);
            goto menu_start;

        case 3:
            menu_start_semiclosed_chat(username, sock, state);
            goto menu_start;

        case 4:
            menu_view_users(sock);
            goto menu_start;

        case 5:
            menu_view_messages(username, sock, state);
            goto menu_start;

        case 6:
            send(sock, "Leaving menu...\n", 16, 0);
            return;

        default:
            send(sock, "Invalid choice.\n", 16, 0);
            goto menu_start;
    }
}

void menu_start_open_chat(const char *username, int sock)
{
    const char *msg =
        "[MENU] Entering OPEN CHAT.\n"
        "Type messages normally. Type /exit to return to menu.\n";
    send(sock, msg, strlen(msg), 0);

    char line[512];
    int n;

    while ((n = recv(sock, line, sizeof(line), 0)) > 0)
    {
        line[n] = 0;

        if (strcmp(line, "/exit\n") == 0)
            break;

        broadcast_message(username, line);
    }

    send(sock, "[MENU] Returned from Open Chat.\n", 32, 0);
}

void menu_start_closed_chat(const char *username, int sock, client_chat_state_t *state)
{
    char out[256];
    snprintf(out, sizeof(out),
             "Enter username to start Closed Chat:\n");
    send(sock, out, strlen(out), 0);

    char partner[64];
    int n = recv(sock, partner, sizeof(partner), 0);
    if (n <= 0) return;
    partner[n - 1] = 0;

    if (!user_exists(partner)) {
        send(sock, "User does not exist.\n", 21, 0);
        return;
    }

    snprintf(out, sizeof(out),
             "[MENU] Closed Chat with %s started.\n"
             "Type /exit to leave.\n", partner);
    send(sock, out, strlen(out), 0);

    char buf[512];

    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
    {
        buf[n] = 0;

        if (strcmp(buf, "/exit\n") == 0)
            break;

        send_private_message(username, partner, buf);
    }

    send(sock, "[MENU] Returned from Closed Chat.\n", 35, 0);
    send_help(sock, state); // <-- go back to main help after exiting
}

void menu_start_semiclosed_chat(const char *username, int sock, client_chat_state_t *state)
{
    const char *intro =
        "Enter usernames for the chat room, separated by spaces.\n"
        "Example: Bob Alice Charlie\n";
    send(sock, intro, strlen(intro), 0);

    char line[512];
    int n = recv(sock, line, sizeof(line), 0);
    if (n <= 0) return;
    line[n - 1] = 0;

    // Parse users
    char *members[10];
    int count = 0;

    char *tok = strtok(line, " ");
    while (tok && count < 10)
    {
        if (user_exists(tok))
            members[count++] = strdup(tok);
        tok = strtok(NULL, " ");
    }

    if (count == 0) {
        send(sock, "No valid users.\n", 16, 0);
        return;
    }

    send(sock, "[MENU] Semi-Closed room created.\nType /exit to leave.\n", 55, 0);

    while ((n = recv(sock, line, sizeof(line), 0)) > 0)
    {
        line[n] = 0;
        if (strcmp(line, "/exit\n") == 0)
            break;

        for (int i = 0; i < count; i++)
            send_private_message(username, members[i], line);
    }

    send(sock, "[MENU] Returned from Semi-Closed chat.\n", 40, 0);
    send_help(sock, state); // <-- go back to main help after exiting
}

void menu_view_users(int sock)
{
    char out[1024];
    get_all_users(out, sizeof(out));
    send(sock, out, strlen(out), 0);
}

void menu_view_messages(const char *username, int sock, client_chat_state_t *state)
{
    char out[2048];  // bigger buffer to avoid truncation
    memset(out, 0, sizeof(out));

    pthread_mutex_lock(&db_lock);

    sqlite3_stmt *stmt = NULL;
    const char *sql_open = "SELECT sender, message, timestamp FROM messages WHERE mode = ? ORDER BY timestamp ASC;";
    const char *sql_closed =
        "SELECT sender, message, timestamp FROM messages "
        "WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?) "
        "ORDER BY timestamp ASC;";

    int rc;

    if (!state || state->mode == OPEN_CHAT) {
        // Fetch only open chat messages
        rc = sqlite3_prepare_v2(db, sql_open, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            send_to_sock(sock, "ERROR: Failed to prepare DB query.\n");
            pthread_mutex_unlock(&db_lock);
            return;
        }
        sqlite3_bind_int(stmt, 1, OPEN_CHAT);
    } else {
        // CLOSED_CHAT or SEMI_CLOSED_CHAT: fetch messages involving this user
        rc = sqlite3_prepare_v2(db, sql_closed, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            send_to_sock(sock, "ERROR: Failed to prepare DB query.\n");
            pthread_mutex_unlock(&db_lock);
            return;
        }

        // Bind parameters depending on chat mode
        if (state->mode == CLOSED_CHAT) {
            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, state->chat_partner, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, state->chat_partner, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, username, -1, SQLITE_TRANSIENT);
        } else if (state->mode == SEMI_CLOSED_CHAT) {
            // For semi-closed chat, we join all room partners
            // For simplicity, fetch messages from any of the partners
            // We'll do this with a dynamic query
            char query[1024];
            snprintf(query, sizeof(query),
                "SELECT sender, message, timestamp FROM messages WHERE (receiver IN (");

            for (int i = 0; i < state->room_size; i++) {
                strcat(query, "'");
                strcat(query, state->room_partners[i]);
                strcat(query, "'");
                if (i < state->room_size - 1) strcat(query, ",");
            }
            strcat(query, ") AND sender = ?) OR (sender = ? AND receiver IN (");

            for (int i = 0; i < state->room_size; i++) {
                strcat(query, "'");
                strcat(query, state->room_partners[i]);
                strcat(query, "'");
                if (i < state->room_size - 1) strcat(query, ",");
            }
            strcat(query, ")) ORDER BY timestamp ASC;");

            rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
            if (rc != SQLITE_OK) {
                send_to_sock(sock, "ERROR: Failed to prepare DB query.\n");
                pthread_mutex_unlock(&db_lock);
                return;
            }

            sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, username, -1, SQLITE_TRANSIENT);
        }
    }

    // Collect results
    char line[512];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *sender = sqlite3_column_text(stmt, 0);
        const unsigned char *msg = sqlite3_column_text(stmt, 1);
        const unsigned char *ts = sqlite3_column_text(stmt, 2);

        snprintf(line, sizeof(line), "[%s] %s: %s\n",
                 ts ? (const char*)ts : "",
                 sender ? (const char*)sender : "",
                 msg ? (const char*)msg : "");
        strncat(out, line, sizeof(out) - strlen(out) - 1);
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_lock);

    send(sock, out, strlen(out), 0);
}

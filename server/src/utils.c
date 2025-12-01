#include "utils.h"
#include "clients.h"
#include <ctype.h>
#include <string.h>

void trim_whitespace(char *s) {
    char *start = s;
    char *end;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    if (*s == 0) return;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

void send_help(int sock, client_chat_state_t *state) {
    send_to_sock(sock, "Available commands:\n");
    send_to_sock(sock, " - Chat <user> <message>    (open mode)\n");
    send_to_sock(sock, " - getmessages <user>\n");
    send_to_sock(sock, " - deletemessages <user>\n");
    send_to_sock(sock, " - getuserlist\n");
    send_to_sock(sock, " - Menu   (interactive chatrooms)\n");
    send_to_sock(sock, " - select <username>   (enter closed chat)\n");
    send_to_sock(sock, " - open   (go to open mode)\n");
    send_to_sock(sock, " - /help  (in closed mode show this help)\n");
    send_to_sock(sock, " - exit\n");
    if (state && state->mode == CLOSED_CHAT) {
        send_to_sock(sock, "In CLOSED_CHAT: type message directly to send to chat partner.\n");
        send_to_sock(sock, "Use /open or /menu or /exit to leave closed chat.\n");
    }
}

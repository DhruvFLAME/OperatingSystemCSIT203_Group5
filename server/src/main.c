#include "server.h"
#include "logging.h"
#include "database.h"
#include "clients.h"
#include "client_thread.h" /* not ideal to include, but client_thread is compiled separately; here only prototypes used */
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

int server_fd;
int running = 1;
sqlite3 *db = NULL;

pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
client_t clients[MAX_CLIENTS];

extern void *client_thread(void *arg);
extern void broadcast_shutdown_and_close_all();

static void handle_sigint(int sig) {
    (void)sig;
    log_info("Caught SIGINT. Shutting down...");
    running = 0;
    broadcast_shutdown_and_close_all();
    if (server_fd > 0) close(server_fd);

    pthread_mutex_lock(&db_lock);
    if (db) sqlite3_close(db);
    pthread_mutex_unlock(&db_lock);

    exit(0);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <port> <database>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    const char *dbfile = argv[2];

    signal(SIGINT, handle_sigint);

    memset(clients, 0, sizeof(clients));
    init_database(dbfile);

    log_info("Creating socket...");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) die("socket");

    log_server_lan_ip();

    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) die("setsockopt");
#ifdef SO_REUSEPORT
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    log_info("Binding to port %d...", port);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("bind");
    log_info("Listening for connections...");
    if (listen(server_fd, 10) < 0) die("listen");

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        log_info("Waiting for incoming connections...");
        int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        log_info("New connection accepted (sock=%d)", client_sock);

        int *psock = malloc(sizeof(int));
        if (!psock) { close(client_sock); continue; }
        *psock = client_sock;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, psock) != 0) {
            perror("pthread_create"); free(psock); close(client_sock); continue;
        }
        pthread_detach(tid);
    }

    broadcast_shutdown_and_close_all();
    if (server_fd > 0) close(server_fd);
    pthread_mutex_lock(&db_lock); if (db) sqlite3_close(db); pthread_mutex_unlock(&db_lock);
    return 0;
}

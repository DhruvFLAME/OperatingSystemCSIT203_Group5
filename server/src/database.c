#include "database.h"
#include "server.h"
#include "clients.h"
#include "logging.h"  // for log_info()
#include <stdio.h>    // for printf(), fprintf()
#include <stdlib.h>   // for exit()
#include <stddef.h>   // for size_t
#include <sqlite3.h>
#include <string.h>   // for memset(), strcpy(), etc.

void init_database(const char *filename) {
    if (sqlite3_open(filename, &db)) {
        fprintf(stderr, "Cannot open DB: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender TEXT,"
        "receiver TEXT,"
        "content TEXT,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "DB error: %s\n", err);
        sqlite3_free(err);
        exit(1);
    }

    log_info("Database ready.");
}

void store_message(const char *sender, const char *receiver, const char *text) {
    pthread_mutex_lock(&db_lock);

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO messages (sender, receiver, content) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "DB prepare error: %s\n", sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_lock);
        return;
    }

    sqlite3_bind_text(stmt, 1, sender, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, receiver, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, text, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "DB step error: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    pthread_mutex_unlock(&db_lock);
}

void handle_getmessages_db_and_send(const char *requester, int requester_sock, const char *target) {
    pthread_mutex_lock(&db_lock);

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT timestamp, sender, receiver, content FROM messages "
        "WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?) "
        "ORDER BY id ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_lock);
        send_to_sock(requester_sock, "ERROR: DB prepare failed\n");
        return;
    }

    sqlite3_bind_text(stmt, 1, requester, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, target, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, target, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, requester, -1, SQLITE_TRANSIENT);

    char line[BUF_SIZE];
    int row_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *ts = sqlite3_column_text(stmt, 0);
        const unsigned char *sender = sqlite3_column_text(stmt, 1);
        const unsigned char *receiver = sqlite3_column_text(stmt, 2);
        const unsigned char *content = sqlite3_column_text(stmt, 3);

        snprintf(line, sizeof(line), "%s %s->%s: %s\n",
                 ts ? (const char*)ts : "",
                 sender ? (const char*)sender : "",
                 receiver ? (const char*)receiver : "",
                 content ? (const char*)content : "");
        send_to_sock(requester_sock, line);
        row_count++;
    }

    if (row_count == 0) send_to_sock(requester_sock, "(no messages)\n");

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_lock);
}

void handle_deletemessages_db(const char *user_a, const char *user_b, int requester_sock) {
    pthread_mutex_lock(&db_lock);

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "DELETE FROM messages WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_lock);
        send_to_sock(requester_sock, "ERROR: DB prepare failed\n");
        return;
    }

    sqlite3_bind_text(stmt, 1, user_a, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user_b, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user_b, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user_a, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        send_to_sock(requester_sock, "ERROR: delete failed\n");
    } else {
        send_to_sock(requester_sock, "OK: messages deleted\n");
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_lock);
}

void get_messages_for_user(const char *username, char *out, size_t out_size)
{
    out[0] = '\0';

    pthread_mutex_lock(&db_lock);

    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT timestamp, sender, receiver, content "
        "FROM messages WHERE sender = ? OR receiver = ? "
        "ORDER BY id ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_lock);
        snprintf(out, out_size, "ERROR reading DB\n");
        return;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_TRANSIENT);

    char line[256];
    int count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *ts = sqlite3_column_text(stmt, 0);
        const unsigned char *sender = sqlite3_column_text(stmt, 1);
        const unsigned char *receiver = sqlite3_column_text(stmt, 2);
        const unsigned char *content = sqlite3_column_text(stmt, 3);

        snprintf(line, sizeof(line),
            "%s %s->%s: %s\n",
            ts ? (const char*)ts : "",
            sender ? (const char*)sender : "",
            receiver ? (const char*)receiver : "",
            content ? (const char*)content : "");

        strncat(out, line, out_size - strlen(out) - 1);
        count++;
    }

    if (count == 0)
        strncat(out, "(no messages)\n", out_size - strlen(out) - 1);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_lock);
}

CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -g
LDLIBS = -lsqlite3 -lpthread

SRC = src/logging.c src/database.c src/clients.c src/messaging.c src/menu.c src/utils.c src/client_thread.c src/main.c
OBJ = $(SRC:.c=.o)

all: server

server: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) server

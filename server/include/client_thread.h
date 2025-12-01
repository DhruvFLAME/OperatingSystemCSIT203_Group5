// include/client_thread.h
#ifndef CLIENT_THREAD_H
#define CLIENT_THREAD_H

#include <pthread.h>
#include "server.h"

void *client_thread(void *arg);

#endif

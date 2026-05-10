#ifndef SIGINT_HANDLER_H
#define SIGINT_HANDLER_H

#include "server.h"

void sigint_handler(int s);

extern server_t *server;
#endif
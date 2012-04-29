#ifndef _SOCKET_H
#define _SOCKET_H

#include <stdbool.h>

// Headers for unix socket
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <piano.h>

#include "main.h"

int sockfd;
bool isSocketAvailable;

void BarSocketInit(BarApp_t *);
void BarSocketDestory();
void BarSocketReconnect();
void BarSocketSendMessage (char *);

#endif /* _SOCKET_H */

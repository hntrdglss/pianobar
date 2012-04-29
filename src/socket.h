#ifndef _SOCKET_H
#define _SOCKET_H

#include <stdbool.h>

// Headers for unix socket
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <piano.h>

#include "settings.h"
#include "player.h"
#include "main.h"

int sockfd;
bool isSocketAvailable;

void BarSocketInit(BarApp_t *);
void BarSocketDestory();
void BarSocketReconnect();
void BarSocketCreateMessage (const BarSettings_t *, const char *,
		const PianoStation_t *, const PianoSong_t *, const struct audioPlayer *);
void BarSocketSendMessage (char *);

#endif /* _SOCKET_H */

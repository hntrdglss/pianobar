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
struct audioPlayer *socketPlayer;

typedef struct {
	char *url; /* splitted url, unusable */
	const char *host;
	const char *port;
} SocketHostPort_t;

void BarSocketInit(BarApp_t *);
void BarSocketDestory();
void BarSocketReconnect();
void BarSocketCreateMessage (const BarSettings_t *, const char *,
		const PianoStation_t *, const PianoSong_t *, const struct audioPlayer *);
void BarSocketSendMessage (char *);
static bool SocketSplitUrl (const char *, SocketHostPort_t *);

#endif /* _SOCKET_H */

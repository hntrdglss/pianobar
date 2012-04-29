#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>
#include <ctype.h> /* tolower() */

#include "main.h"
#include "ui.h"
#include "socket.h"

void BarSocketInit(BarApp_t * app) {
	//------------------------------------------------------------------------------- //
	// ---------------------- SOCKET SERVER INITIALIZATION -------------------------  //
	//------------------------------------------------------------------------------- //

	// printf("attempting to connect to socket server...\n");

	if(app->settings.socketEnabled) {
		isSocketAvailable = true;
		int n;
		struct sockaddr_in serv_addr;
		char hello[] = "knock knock";

		BarUiMsg (&app->settings, MSG_NONE, "Connecting %s to %s:%i: ", app->settings.socketMyDeviceName, app->settings.socketIP, app->settings.socketPort);

		/* First call to socket() function */
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			BarUiMsg (&app->settings, MSG_NONE, "error opening socket.\n");
			isSocketAvailable = false;
		}

		/* Initialize socket structure */
		bzero((char *) &serv_addr, sizeof(serv_addr));
		// serv_addr.sin_family = AF_INET;
		serv_addr.sin_family = AF_UNSPEC;
		serv_addr.sin_addr.s_addr = inet_addr(app->settings.socketIP);
		serv_addr.sin_port = htons(app->settings.socketPort);

		signal(SIGPIPE, BarSocketReconnect);

		// connecting to socket
		if (isSocketAvailable && connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
			BarUiMsg (&app->settings, MSG_NONE, "error connecting to socket.\n");
			isSocketAvailable = false;
		}

		/* Write a response to the client */
		if(isSocketAvailable) {
			n = send(sockfd, hello, (int)strlen(hello), 0);
		}
		if (isSocketAvailable && n < 0) {
			BarUiMsg (&app->settings, MSG_NONE, "error writing to socket.\n");
			isSocketAvailable = false;
		}

		if(isSocketAvailable) {
			BarUiMsg (&app->settings, MSG_NONE, "connected!\n");
		}

		//------------------------------------------------------------------------------- //
		// ---------------------- END SOCKET SERVER CODE -------------------------------  //
		//------------------------------------------------------------------------------- //
	}
}

void BarSocketDestroy() {
	close(sockfd);
}

void BarSocketReconnect() {
	isSocketAvailable = false;
	printf ("!!! Socket closed unexpectedly.\n");
}

inline void BarSocketSendMessage(char * message) {
	if(isSocketAvailable) {
		int n;

		// Allocate memory for the string we will send to the socket server.
		// length will be the size of the mem allocation for the string
		//int length = snprintf(NULL, 0, message) + 1;
	
		// Character object that will store the string
		//char * data = (char*) malloc((length) * sizeof(char));
	
		// Print string in format of: [id,x,y,x,time]
		//snprintf(data, length, message);
	
		// Send data off to socket server
		n = send( sockfd, message, (int) strlen(message), 0 );
	}
}
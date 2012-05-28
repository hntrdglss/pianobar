#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <json.h>
#include <assert.h>
#include <ctype.h> /* tolower() */

#include "main.h"
#include "ui.h"
#include "socket.h"

void BarSocketInit(BarApp_t * app) {
	//------------------------------------------------------------------------------- //
	// ---------------------- SOCKET SERVER INITIALIZATION -------------------------  //
	//------------------------------------------------------------------------------- //

	isSocketAvailable = false;

	if(app->settings.socketHostIP != NULL) {
		isSocketAvailable = true;
		int n;
		struct sockaddr_in serv_addr;
		char stream[1024];

		socketPlayer = &app->player;

		SocketHostPort_t shp;
		memset (&shp, 0, sizeof (shp));
		if(!SocketSplitUrl(app->settings.socketHostIP, &shp)) {
			isSocketAvailable = false;
			return;
		}

		BarUiMsg (&app->settings, MSG_NONE, "Connecting %s to %s:%i: ", app->settings.socketMyDeviceName, shp.host, atoi(shp.port));

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
		serv_addr.sin_addr.s_addr = inet_addr(shp.host);
		serv_addr.sin_port = htons(atoi(shp.port));

		signal(SIGPIPE, BarSocketReconnect);

		// connecting to socket
		if (isSocketAvailable && connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
			BarUiMsg (&app->settings, MSG_NONE, "error connecting to socket.\n");
			isSocketAvailable = false;
		}

		sprintf(stream, "{\"device\":\"%s\",\"event\":\"connect\"}", app->settings.socketMyDeviceName);

		/* Write a response to the client */
		if(isSocketAvailable) {
			n = send(sockfd, stream, (int)strlen(stream), 0);
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
	printf ("!!! Socket closed unexpectedly. Unpause music to continue listening, without socket connection.\n");
	close(sockfd);

	if (pthread_mutex_trylock (&socketPlayer->pauseMutex) == EBUSY) {
		//pthread_mutex_unlock (&socketPlayer->pauseMutex);
	}
}

void BarSocketCreateMessage(const BarSettings_t *settings, const char *type,
	const PianoStation_t *curStation, const PianoSong_t *curSong,
	const struct audioPlayer *player) {

	if(isSocketAvailable) {
		// char stream[2048];
		json_object *jstream = json_object_new_object ();

			json_object_object_add (jstream, "device", json_object_new_string (settings->socketMyDeviceName));
			json_object_object_add (jstream, "event", json_object_new_string (type));

			json_object *payload = json_object_new_object ();

		if(type == "songstart") {
				if(curSong->musicId != NULL) {
					json_object_object_add (payload, "music_id", json_object_new_string (curSong->musicId));
				} else {
					json_object_object_add (payload, "music_id", json_object_new_string ("(null)"));
				}
				json_object_object_add (payload, "station_name", json_object_new_string (curStation->name));
				json_object_object_add (payload, "track_token", json_object_new_string (curSong->trackToken));
				json_object_object_add (payload, "artist", json_object_new_string (curSong->artist));
				json_object_object_add (payload, "album", json_object_new_string (curSong->album));
				json_object_object_add (payload, "title", json_object_new_string (curSong->title));
				if(curSong->coverArt != NULL) {
					json_object_object_add (payload, "cover_art", json_object_new_string (curSong->coverArt));
				} else {
					json_object_object_add (payload, "cover_art", json_object_new_string ("(null)"));
				}
				json_object_object_add (payload, "audio", json_object_new_string (curSong->audioUrl));
				json_object_object_add (payload, "detail_url", json_object_new_string (curSong->detailUrl));

					json_object *lyrics = json_object_new_object ();
						if(curSong->lyricId != NULL) {
							json_object_object_add (lyrics, "id", json_object_new_string (curSong->lyricId));
						} else {
							json_object_object_add (lyrics, "id", json_object_new_string ("(null)"));
						}
						if(curSong->lyricChecksum != NULL) {
							json_object_object_add (lyrics, "checksum", json_object_new_string (curSong->lyricChecksum));
						} else {
							json_object_object_add (lyrics, "checksum", json_object_new_string ("(null)"));
						}
					json_object_object_add (payload, "lyrics", lyrics);

					// send next song
					PianoSong_t *nextSong = curSong->next;
					if (nextSong != NULL) {
						json_object *next = json_object_new_object ();
						json_object_object_add (next, "artist", json_object_new_string (nextSong->artist));
						json_object_object_add (next, "album", json_object_new_string (nextSong->album));
						json_object_object_add (next, "title", json_object_new_string (nextSong->title));
						if(curSong->coverArt != NULL) {
							json_object_object_add (next, "cover_art", json_object_new_string (nextSong->coverArt));
						} else {
							json_object_object_add (next, "cover_art", json_object_new_string ("(null)"));
						}
						json_object_object_add (next, "audio", json_object_new_string (nextSong->audioUrl));
						json_object_object_add (next, "detail_url", json_object_new_string (nextSong->detailUrl));
						//json_object_object_add (payload, "next", next);
					} else {
						json_object *next = json_object_new_string ("(null)");
						//json_object_object_add (payload, "next", next);
					}
		} else if(type == "songduration") {
			if(curSong->musicId != NULL) {
				json_object_object_add (payload, "music_id", json_object_new_string (curSong->musicId));
			} else {
				json_object_object_add (payload, "music_id", json_object_new_string ("(null)"));
			}
			json_object_object_add (payload, "seconds_elapsed", json_object_new_int (player->songPlayed / BAR_PLAYER_MS_TO_S_FACTOR));
			json_object_object_add (payload, "duration", json_object_new_int (player->songDuration / BAR_PLAYER_MS_TO_S_FACTOR));
		} else if(type == "songfinish" || type == "songbookmark" || type == "songlove") {
			if(curSong->musicId != NULL) {
				json_object_object_add (payload, "music_id", json_object_new_string (curSong->musicId));
			} else {
				json_object_object_add (payload, "music_id", json_object_new_string ("(null)"));
			}
		}

		json_object_object_add (jstream, "payload", payload);
		BarSocketSendMessage(json_object_to_json_string(jstream));
		// BarSocketSendMessage(stream);

		// stream[0] = 0;
		json_object_put (jstream);
	}
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

		isSocketAvailable = false;
		// printf("%s\n", message);

		// Send data off to socket server
		n = send( sockfd, message, (int) strlen(message), 0 );

		if(n >= 0) {
			isSocketAvailable = true;
		}
	}
}

/*	Split http url into host, port and path
 *	@param url
 *	@param returned url struct
 *	@return url is a http url? does not say anything about its validity!
 */
static bool SocketSplitUrl (const char *inurl, SocketHostPort_t *retUrl) {
	assert (inurl != NULL);
	assert (retUrl != NULL);

	enum {FIND_HOST, FIND_PORT, DONE}
			state = FIND_HOST, newState = FIND_HOST;
	char *url, *urlPos, *assignStart;
	const char **assign = NULL;

	url = strdup (inurl);
	retUrl->url = url;

	urlPos = url;
	assignStart = urlPos;

	if (*urlPos == '\0') {
		state = DONE;
	}

	while (state != DONE) {
		const char c = *urlPos;

		switch (state) {

			case FIND_HOST: {
				if (c == ':') {
					assign = &retUrl->host;
					newState = FIND_PORT;
				} else if (c == '\0') {
					assign = &retUrl->host;
					newState = DONE;
				}
				break;
			}

			case FIND_PORT: {
				if (c == '\0') {
					assign = &retUrl->port;
					newState = DONE;
				}
				break;
			}

			case DONE:
				break;
		} /* end switch */

		if (assign != NULL) {
			*assign = assignStart;
			*urlPos = '\0';
			assignStart = urlPos+1;

			state = newState;
			assign = NULL;
		}

		++urlPos;
	} /* end while */

	return true;
}
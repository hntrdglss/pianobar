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
#include "waitress.h"

char *replace_str(char *str, char *orig, char *rep)
{
	static char buffer[4096];
	char *p;

	if(!(p = strstr(str, orig)))  // Is 'orig' even in 'str'?
		return str;

	strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig' st$
	buffer[p-str] = '\0';

	sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));

	return buffer;
}

char *PianoJsonGetMusicId (char *explorerUrl) {
	char *response;

	WaitressSetUrl(&waith, replace_str(explorerUrl, "xml", "json"));

	WaitressFetchBuf (&waith, &response);

	if(response == "{}") {
		return PianoJsonGetMusicId(explorerUrl);
	} else {
		json_object *j = json_tokener_parse (response);

		json_object *songExplorer = json_object_object_get(j, "songExplorer");

		return strdup (json_object_get_string (json_object_object_get (songExplorer, "@musicId")));
	}
}

void BarSocketInit(BarSettings_t *appSettings, struct audioPlayer *appPlayer, WaitressHandle_t *appWaith, bool isReconnect) {
	//------------------------------------------------------------------------------- //
	// ---------------------- SOCKET SERVER INITIALIZATION -------------------------  //
	//------------------------------------------------------------------------------- //

	isSocketAvailable = false;
	resyncPlaylist = false;

	if(appSettings->socketHostIP != NULL) {
		isSocketAvailable = true;

		int n;
		struct sockaddr_in serv_addr;
		char stream[1024];

		socketPlayer = appPlayer;
		socketSettings = appSettings;
		if(appWaith != NULL) {
			waith = appWaith;
		}

		SocketHostPort_t shp;
		memset (&shp, 0, sizeof (shp));
		if(!SocketSplitUrl(appSettings->socketHostIP, &shp)) {
			isSocketAvailable = false;
			return;
		}

		if(!isReconnect) {
			BarUiMsg (appSettings, MSG_NONE, "Connecting %s to %s:%i: ", appSettings->socketMyDeviceName, shp.host, atoi(shp.port));
		}

		/* First call to socket() function */
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			BarUiMsg (appSettings, MSG_NONE, "error opening socket.\n");
			isSocketAvailable = false;
		}

		/* Initialize socket structure */
		bzero((char *) &serv_addr, sizeof(serv_addr));
		// serv_addr.sin_family = AF_INET;
		serv_addr.sin_family = AF_UNSPEC;
		serv_addr.sin_addr.s_addr = inet_addr(shp.host);
		serv_addr.sin_port = htons(atoi(shp.port));

		signal(SIGPIPE, BarSocketDisconnect);

		// connecting to socket
		if (isSocketAvailable && connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
			BarUiMsg (appSettings, MSG_NONE, "error connecting to socket.\n");
			isSocketAvailable = false;
		}

		/* Write a response to the client */
		if(isSocketAvailable) {
			n = send(sockfd, stream, (int)strlen(stream), 0);
		}
		if (isSocketAvailable && n < 0) {
			BarUiMsg (appSettings, MSG_NONE, "error writing to socket.\n");
			isSocketAvailable = false;
		}

		if(isSocketAvailable) {
			BarUiMsg (appSettings, MSG_NONE, "connected!\n");
		}

		json_object *jstream = json_object_new_object ();
			json_object_object_add (jstream, "device", json_object_new_string (appSettings->socketMyDeviceName));
			json_object_object_add (jstream, "event", json_object_new_string("connect"));

		BarSocketSendMessage(json_object_to_json_string(jstream));

		//------------------------------------------------------------------------------- //
		// ---------------------- END SOCKET SERVER CODE -------------------------------  //
		//------------------------------------------------------------------------------- //
	}
}

void BarSocketDestroy() {
	close(sockfd);
}

void BarSocketDisconnect() {
	isSocketAvailable = false;

	if (pthread_mutex_trylock (&socketPlayer->pauseMutex) == EBUSY) {
		//pthread_mutex_unlock (&socketPlayer->pauseMutex);
	}

	BarUiMsg (&socketSettings, MSG_ERR, "!!! Socket closed unexpectedly.\n");
	close(sockfd);

	for(int i = 0; i < 5; i++) {
		BarUiMsg (&socketSettings, MSG_ERR, "Attempt %i of 5: ", i + 1);
		BarSocketReconnect(false);

		if(isSocketAvailable) {
			i = 5;
		} else if(i < 4) {
			sleep(5);
		}
	}

	if(!isSocketAvailable) {
		BarUiMsg (&socketSettings, MSG_ERR, "!!! Unable to auto reconnect. Unpause music or press \"k\" to manually try reconnecting.\n");
	}
}

void BarSocketReconnect(bool isManual) {
	if(isManual) {
		BarUiMsg (socketSettings, MSG_NONE, "Attempting to reconnect: ");
	}

	if(!isSocketAvailable) {
		BarSocketInit(socketSettings, socketPlayer, waith, true);

		if(isSocketAvailable) {
			if (pthread_mutex_trylock (&socketPlayer->pauseMutex) == EBUSY) {
				pthread_mutex_unlock (&socketPlayer->pauseMutex);
			}
		}

		resyncPlaylist = true;
	} else {
		BarUiMsg (socketSettings, MSG_NONE, "already connected!\n");
	}
}

void BarSocketSaveAuthToken(char *token) {
	authToken = token;
}

void BarSocketCreateMessage(const BarSettings_t *settings, const char *type,
	const PianoStation_t *curStation, const PianoSong_t *curSong,
	const struct audioPlayer *player) {

	if(isSocketAvailable) {
		json_object *jstream = json_object_new_object ();

			json_object_object_add (jstream, "device", json_object_new_string (settings->socketMyDeviceName));
			json_object_object_add (jstream, "event", json_object_new_string (type));

			json_object *payload = json_object_new_object ();

		if(curSong != NULL) {
			if(type == "songduration") {
				if(curSong->trackToken != NULL) {
					json_object_object_add (payload, "track_token", json_object_new_string (curSong->trackToken));
				} else {
					json_object_object_add (payload, "track_token", json_object_new_string ("(null)"));
				}
				json_object_object_add (payload, "seconds_elapsed", json_object_new_int (player->songPlayed / BAR_PLAYER_MS_TO_S_FACTOR));
				json_object_object_add (payload, "duration", json_object_new_int (player->songDuration / BAR_PLAYER_MS_TO_S_FACTOR));

				// resync
				if(resyncPlaylist) {
					BarSocketCreateMessage(settings, "songstart", curStation, curSong, player);
				}
			} else if(type == "songstart" || type == "songfinish" || type == "songbookmark" || type == "songlove") {
				if(curSong->trackToken != NULL) {
					json_object_object_add (payload, "track_token", json_object_new_string (curSong->trackToken));
				} else {
					json_object_object_add (payload, "track_token", json_object_new_string ("(null)"));
				}

				// resync
				if(type == "songstart" && resyncPlaylist) {
					BarSocketCreateMessage(settings, "stationfetchplaylist", curStation, curSong, player);
				}
			} else if(type == "stationfetchplaylist") {
				resyncPlaylist = false;
				json_object_object_add (payload, "station_name", json_object_new_string (curStation->name));

				json_object *songs = json_object_new_array();

				while (curSong->next != NULL) {
					json_object_array_add (songs, BarSocketBuildSong(curSong));

					curSong = curSong->next;
				}

				json_object_array_add (songs, BarSocketBuildSong(curSong));

				json_object_object_add (payload, "songs", songs);
			}
		} else if(type == "userlogin") {
			json_object_object_add (payload, "auth_token", json_object_new_string (authToken));
		}

		json_object_object_add (jstream, "payload", payload);
		BarSocketSendMessage(json_object_to_json_string(jstream));

		json_object_put (jstream);
	}
}

void BarSocketSendMessage(char * message) {
	if(isSocketAvailable) {
		int n;

		// Allocate memory for the string we will send to the socket server.
		// length will be the size of the mem allocation for the string
		// int length = snprintf(NULL, 0, message) + 1;
		int length = strlen(message) + 2;
	
		// Character object that will store the string
		char * data = (char*) malloc((length) * sizeof(char));

		isSocketAvailable = false;

		strcpy(data, message);
		strcat(data, "|\0");

		// Send data off to socket server
		n = send( sockfd, data, length, 0 );

		if(n >= 0) {
			isSocketAvailable = true;
		}

		free(data);
	}
}

json_object * BarSocketBuildSong(const PianoSong_t *curSong) {
	json_object *payload = json_object_new_object ();

		if(curSong->trackToken != NULL) {
			json_object_object_add (payload, "track_token", json_object_new_string (curSong->trackToken));
		} else {
			json_object_object_add (payload, "track_token", json_object_new_string ("(null)"));
		}
		json_object_object_add (payload, "artist", json_object_new_string (curSong->artist));
		json_object_object_add (payload, "album", json_object_new_string (curSong->album));
		json_object_object_add (payload, "title", json_object_new_string (curSong->title));
		if(curSong->coverArt != NULL) {
			json_object_object_add (payload, "cover_art", json_object_new_string (curSong->coverArt));
		} else {
			json_object_object_add (payload, "cover_art", json_object_new_string ("(null)"));
		}
		json_object_object_add (payload, "explorer_url", json_object_new_string (curSong->explorerUrl));
		json_object_object_add (payload, "audio", json_object_new_string (curSong->audioUrl));
		//json_object_object_add (payload, "detail_url", json_object_new_string (curSong->detailUrl)); // not needed, part of explorer_url

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

	return payload;
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
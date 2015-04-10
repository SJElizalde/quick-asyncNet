#ifndef AHTTP_GLOBALS_H
#define AHTTP_GLOBALS_H

#define MAX_CONCURRENT_DOWNLOADS 5
#define MAX_CONCURRENT_REQUESTS 10
#define HTTP_READ_TIMEOUT 120
#define BASE_READ_CHUNK 32768
#define LUA_EVENT_NAME "http_request_event"

enum RequestMethod {
	GET,
	POST
};

enum RequestStatus {
	NONE,
	CONNECTING,
	DOWNLOADING,
	COMPLETE,
	ERROR,
	DESTROYED
};

#endif
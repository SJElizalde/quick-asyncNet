//------------------------------------------------------------------------------
// Threaded HTTP Loader for download concurrency lib
//------------------------------------------------------------------------------
#include "s3e.h"
#include "s3eThread.h"
#include "IwHTTP.h"

#define MAX_CONCURRENT_DOWNLOADS 5
#define HTTP_READ_TIMEOUT 120
#define BASE_READ_CHUNK 1024
#define LUA_EVENT_NAME "http_event"

enum LoaderStatus {
	NONE,
	CONNECTING,
	DOWNLOADING,
	COMPLETE,
	ERROR,
};

namespace kidloom_loader {
	
	typedef void(*loaderCallback)(void*);
	//-------------------------------------------------------------------------
	// Loader class encapsulates both the http connection and local file
	// components of a download in progress.
	class Loader {
	private:
		char* local_url;
		char* remote_url;
		
		uint32 error_code;
		uint32 content_length;
		char* content;

		CIwHTTP* net_connection;
		s3eThread* thread_handle;
		s3eFile* file_handle;
		LoaderStatus status;
		loaderCallback callback;

		
	public:
		Loader(char* url, char* filename);
		~Loader();

		bool load();
		bool readHeader();
		bool readContent();
		bool checkRequestStatus();
		void notifyStatus();

		char* getRemoteURL();
		char* getLocalURL();
		char* getStatusString();
		
		LoaderStatus getStatus();
		s3eFile* getFileHandle();
		s3eThread* getThreadHandle();
		CIwHTTP* getConnection();

		void setStatus(LoaderStatus newStatus);
		void setCompleteCallback(loaderCallback);
	};

	
	//-------------------------------------------------------------------------
	// Loaders
	extern Loader* loaders[MAX_CONCURRENT_DOWNLOADS];

	//-------------------------------------------------------------------------
	// Loaders call this method when completed
	void on_load_complete(void* instance);

	//-------------------------------------------------------------------------
	// The C++ API for LUA will call this method to begin a download
	void openRequest(char* url, char* filename);

	//-------------------------------------------------------------------------
	// Static callbacks delegate socket data processing to loader's implementation
	int32 http_dataReceived(void*, void*);
	int32 http_headersReceived(void*, void*);
}
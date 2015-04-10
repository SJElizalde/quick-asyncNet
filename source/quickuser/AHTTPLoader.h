//------------------------------------------------------------------------------
// Threaded HTTP AHTTPLoader for download concurrency lib
//------------------------------------------------------------------------------
#include "s3e.h"
#include "IwHTTP.h"
#include "AHTTPGlobals.h"

namespace ahttp_loader {
	
	using namespace std;
	typedef void(*loaderCallback)(void*);
	//-------------------------------------------------------------------------
	// AHTTPLoader class encapsulates both the http connection and local file
	// components of a download in progress.
	class AHTTPLoader {
	private:
		char* local_url;
		char* remote_url;
		
		uint32 slot;
		uint32 error_code;
		uint32 buffer_size;
		uint32 bytes_read;
		uint32 bytes_written;
		char* buffer;

		CIwHTTP* net_connection;
		s3eFile* file_handle;
		RequestStatus status;
		loaderCallback callback;

		
	public:
		AHTTPLoader(uint32 slot, char* url, char* filename);
		~AHTTPLoader();

		bool load();
		bool readHeader();
		bool readContent();
		bool checkRequestStatus();
		void notifyStatus();

		uint32 getSlot();
		char* getRemoteURL();
		char* getLocalURL();
		char* getStatusString();
		
		RequestStatus getStatus();
		s3eFile* getFileHandle();
		CIwHTTP* getConnection();

		void setStatus(RequestStatus newStatus);
		void setCompleteCallback(loaderCallback);
	};

	
	//-------------------------------------------------------------------------
	// Loaders
	extern AHTTPLoader* loaders[MAX_CONCURRENT_DOWNLOADS];

	//-------------------------------------------------------------------------
	extern uint32 current_downloads;

	//-------------------------------------------------------------------------
	// Loaders call this method when completed
	void on_load_complete(void* instance);

	//-------------------------------------------------------------------------
	// The C++ API for LUA will call this method to begin a download
	bool openRequest(char* url, char* filename);

	//-------------------------------------------------------------------------
	// Static callbacks delegate socket data processing to loader's implementation
	int32 http_dataReceived(void*, void*);
	int32 http_headersReceived(void*, void*);
}
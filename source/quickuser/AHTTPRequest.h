//------------------------------------------------------------------------------
// Threaded HTTP request for request concurrency lib
//------------------------------------------------------------------------------
#include "s3e.h"
#include "IwHTTP.h"
#include "AHTTPGlobals.h"
#include "map"

namespace ahttp_request {
	
	using namespace std;
	typedef void(*loaderCallback)(void*);
	//-------------------------------------------------------------------------
	// AHTTPRequest class encapsulates both the http connection and local file
	// components of a download in progress.
	class AHTTPRequest {
	private:
		char* remote_url;
		uint32 http_method;
		char* request_body;

		uint32 slot;
		uint32 error_code;
		uint32 buffer_size;
		uint32 bytes_read;
		uint32 bytes_written;
		char* buffer;
		char* result_buffer;

		CIwHTTP* net_connection;
		RequestStatus status;
		loaderCallback callback;


	public:
		AHTTPRequest(uint32, char*, uint32, char*);
		~AHTTPRequest();

		bool load();
		bool readHeader();
		bool readContent();
		bool checkRequestStatus();
		void notifyStatus();

		uint32 getSlot();
		char* getRemoteURL();
		char* getStatusString();
		char* getResult();
		
		RequestStatus getStatus();
		CIwHTTP* getConnection();

		void setHeaders(std::map<char*, char*>);
		void setStatus(RequestStatus newStatus);
		void setCompleteCallback(loaderCallback);
	};

	
	//-------------------------------------------------------------------------
	// Loaders
	extern AHTTPRequest* requests[MAX_CONCURRENT_REQUESTS];

	//-------------------------------------------------------------------------
	extern uint32 current_downloads;

	//-------------------------------------------------------------------------
	// Loaders call this method when completed
	void on_load_complete(void* instance);

	//-------------------------------------------------------------------------
	// The C++ API for LUA will call this method to begin a download
	bool openRequest(char* url, uint32 method, char* body);

	//-------------------------------------------------------------------------
	// Request headers are set one by one before requesting any URL. By default, set headers are kept between requests until flushed.
	
	extern map<char*, char*> request_headers;
	void addHeader(char*, char*);
	void remHeader(char*);
	void flushHeaders();

	//-------------------------------------------------------------------------
	// Static callbacks delegate socket data processing to loader's implementation
	int32 http_dataReceived(void*, void*);
	int32 http_headersReceived(void*, void*);
}
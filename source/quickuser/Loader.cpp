/*
* (C) 2014-2015 Kidloom.
*
* This class is designed to funnel a datastream from an HTTP connection into a file
* handle within the local filesystem. Being multi-threading, it also includes a handle
* to the thread in which the operation is being handled.
*
* Author: S.J.Elizalde
*         santa@kidloom.com
*/

#include "Loader.h"
#include "QLuaHelpers.h"

kidloom_loader::Loader* kidloom_loader::loaders[MAX_CONCURRENT_DOWNLOADS];
//-------------------------------------------------------------------------
// Loaders call this method when completed, to be destroyed and removed.
void kidloom_loader::on_load_complete(void* instance) {
	
	Loader* loader = (Loader*)instance;
	IwDebugTraceLinePrintf("AHTTPLoader: Destroying loader [%s]\n", loader->getRemoteURL());
	//loader->~Loader();
	for (int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++) {

		if (loaders[i] == loader) {
			loaders[i] = NULL;
			break;
		}
	}
}

//-------------------------------------------------------------------------
// The C++ API for LUA will call this method to begin a download
void kidloom_loader::openRequest(char* url, char* filename) {
	
	for (int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++) {
		
		if (loaders[i] != NULL) {
			// Check if we need to destroy this loader (memory failsafe)
			if (loaders[i]->getStatus() == COMPLETE || loaders[i]->getStatus() == ERROR) {
				Loader* ldr = loaders[i];
				ldr->~Loader();
				loaders[i] = NULL;
			}
		}

		if (loaders[i] == NULL) {
			IwDebugTraceLinePrintf("AHTTPLoader: Creating new loader\n");
			Loader* loader = new Loader(url, filename);
			IwDebugTraceLinePrintf("AHTTPLoader: Setting loader callback\n");
			
			loaders[i] = loader;

			loader->setCompleteCallback(on_load_complete);
			loader->load();
			return;
		}
	}
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 kidloom_loader::http_dataReceived(void* sysData, void* instance) {
	Loader* loader = (Loader*)instance;
	loader->readContent();
	return 0;
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 kidloom_loader::http_headersReceived(void* sysData, void* instance) {
	Loader* loader = (Loader*)instance;
	loader->readHeader();
	return 0;
}

//-------------------------------------------------------------------------
// LOADER CLASS MEMBERS
//-------------------------------------------------------------------------
kidloom_loader::Loader::Loader(char* url, char* filename) {
	
	remote_url = url;
	local_url = filename;
	error_code = 0;
	status = NONE;
	this->net_connection = new CIwHTTP();
	IwDebugTraceLinePrintf("AHTTPLoader: Loader created\n");
}

//-------------------------------------------------------------------------
// Begins the loading process for the set pair of URLs (downloads remote-to-local)
bool kidloom_loader::Loader::load() {

	if (status != NONE) {
		return false;
	}
	IwDebugTraceLinePrintf("AHTTPLoader: Creating and opening net connection\n");
	if (this->net_connection->Get(remote_url, http_headersReceived, this) != S3E_RESULT_SUCCESS) {
		IwDebugTraceLinePrintf("AHTTPLoader: Request failed -> CONNECTION ERROR (IMMEDIATE)\n");
		setStatus(ERROR);
		error_code = 1;
		notifyStatus();
		return false;
	}
	setStatus(CONNECTING);
	IwDebugTraceLinePrintf("AHTTPLoader: Connecting...\n");
	return true;
}
//-------------------------------------------------------------------------
// Checks for connection or HTTP errors and returns true if all is OK
bool kidloom_loader::Loader::checkRequestStatus() {

	bool res = false;
	if (net_connection->GetStatus() == S3E_RESULT_ERROR) {
		// Socket or Connection Error (fatal)
		setStatus(ERROR);
		error_code = 1;
		res = false;
		IwDebugTraceLinePrintf("AHTTPLaoder: Request failed -> CONNECTION ERROR\n");
	}
	else {
		error_code = net_connection->GetResponseCode();
		switch (error_code) {
		case 200:
		case 203:
			res = true;
			break;
		default:
			setStatus(ERROR);
			res = false;
			IwDebugTraceLinePrintf("AHTTPLaoder: Request failed -> HTTP ERROR %d\n", error_code);
			break;
		}
	}
	return res;
}
//-------------------------------------------------------------------------
// Receives HTTP response headers, sets content length and begins reading.
bool kidloom_loader::Loader::readHeader() {

	IwDebugTraceLinePrintf("AHTTPLoader: Headers received\n");

	if (checkRequestStatus() == false) {
		//Notify the LUA layer if there was an error
		notifyStatus();
		return false;
	}

	content_length = net_connection->ContentExpected();
	if (!content_length) {
		IwDebugTraceLinePrintf(">>>                    Content-length header not present, guessing minimum chunk-size (%d bytes)\n", BASE_READ_CHUNK);
		content_length = BASE_READ_CHUNK;
	}
	char **resultBuffer = &content;
	IwDebugTraceLinePrintf(">>>                    Allocating %d bytes of memory for content data\n", content_length);
	*resultBuffer = (char*)s3eMalloc(content_length + 1);
	(*resultBuffer)[content_length] = 0;
	file_handle = s3eFileOpen(local_url, "w");
	IwDebugTraceLinePrintf(">>>                    Calling ASYNC-READ of first chunk\n");
	net_connection->ReadDataAsync(*resultBuffer, content_length, HTTP_READ_TIMEOUT, http_dataReceived, this);
	setStatus(DOWNLOADING);
	return true;
}

//-------------------------------------------------------------------------
// Receives HTTP chunked content asynchronically and pushes it into the file handle.
bool kidloom_loader::Loader::readContent() {
	IwDebugTraceLinePrintf("AHTTPLoader: Reading content chunk\n");
	
	if (checkRequestStatus() == false) {
		//Notify the LUA layer if there was an error
		notifyStatus();
		return false;
	}
	
	IwDebugTraceLinePrintf(">>>                     Got %d/%d bytes:\n", net_connection->ContentReceived(), net_connection->ContentLength());
	if (net_connection->ContentReceived() != net_connection->ContentLength()) {
		uint32 oldLen = content_length;

		if (content_length < net_connection->ContentExpected()) {
			content_length = net_connection->ContentExpected();
		}
		else if (!net_connection->ContentExpected()) {
			content_length += BASE_READ_CHUNK;
		}

		uint32 realloc_delta = content_length - oldLen;
		if (realloc_delta > 0) {
			IwDebugTraceLinePrintf(">>>                    Reallocating content size (allocating %d extra bytes)\n", realloc_delta);
			content = (char*)s3eRealloc(content, content_length);
		}
		
		IwDebugTraceLinePrintf(">>>                    Calling ASYNC-READ of next chunk\n");
		net_connection->ReadDataAsync(&content[oldLen], content_length - oldLen, HTTP_READ_TIMEOUT, http_dataReceived, this);
	}
	else if(status != COMPLETE) {
		IwDebugTraceLinePrintf(">>>                    COMPLETED DOWNLOAD\n");
		if (file_handle != NULL) {
			IwDebugTraceLinePrintf(">>>                 Writing %d bytes of data to file '%s'\n", content_length, local_url);
			uint32 written = s3eFileWrite(content, net_connection->ContentReceived(), 1, file_handle);
			IwDebugTraceLinePrintf(">>>                 Closing file handle for %s\n", local_url);
			s3eFileClose(file_handle);
		}
		setStatus(COMPLETE);
		callback(this);
	}
	notifyStatus();
	return true;
}

void kidloom_loader::Loader::notifyStatus() {

	IwDebugTraceLinePrintf("AHTTPLoader: sending status event...\n");
	quick::LUA_EVENT_PREPARE("http_event");
	quick::LUA_EVENT_SET_STRING("url", remote_url);
	quick::LUA_EVENT_SET_STRING("filename", local_url);
	IwDebugTraceLinePrintf(">>>          %s: '%s'\n", getStatusString(), remote_url);
	if (net_connection && status != ERROR) {
		float percent = (float)net_connection->ContentReceived() / (float)net_connection->ContentLength();
		quick::LUA_EVENT_SET_NUMBER("percent", percent);
	}
	quick::LUA_EVENT_SET_STRING("status", getStatusString());
	if (status == ERROR) {
		quick::LUA_EVENT_SET_NUMBER("ecode", error_code);
	}
	quick::LUA_EVENT_SEND();
	lua_pop(quick::g_L, 1);
}
//-------------------------------------------------------------------------
// Changes loader status and prints debug state data to log
void kidloom_loader::Loader::setStatus(LoaderStatus newStatus) {
	status = newStatus;
	IwDebugTraceLinePrintf("AHTTPLoader: Change state to [%s]\n", this->getStatusString());
}

//-------------------------------------------------------------------------
// todo
void kidloom_loader::Loader::setCompleteCallback(loaderCallback cb) {
	if (cb) {
		IwDebugTraceLinePrintf("AHTTPLoader: callback set\n");
		callback = cb;
	}
}

//-------------------------------------------------------------------------
// char status, to be passed directly to LUA event handlers (LUA status enum)
LoaderStatus kidloom_loader::Loader::getStatus() {
	return status;
}

//-------------------------------------------------------------------------
// char status, to be passed directly to LUA event handlers (LUA status enum)
char* kidloom_loader::Loader::getStatusString() {
	switch (status)
	{
	case CONNECTING:
		return "begin";
		break;
	case DOWNLOADING:
		return "in_progress";
		break;
	case COMPLETE:
		return "complete";
		break;
	case ERROR:
		return "error";
		break;
	default:
		return "none ";
		break;
	}
}

//-------------------------------------------------------------------------
// Local file URL where remote data is being stored
char* kidloom_loader::Loader::getLocalURL() {
	return local_url;
}

//-------------------------------------------------------------------------
// Remote server URL where the data is hosted
char* kidloom_loader::Loader::getRemoteURL() {
	return remote_url;
}

//-------------------------------------------------------------------------
// Connection object (CIwHTTP socket wrapper)
CIwHTTP* kidloom_loader::Loader::getConnection() {
	return net_connection;
}

//-------------------------------------------------------------------------
// Filesystem file handle (always write mode)
s3eFile* kidloom_loader::Loader::getFileHandle() {
	return file_handle;
}

//-------------------------------------------------------------------------
// Data receiver thread (Thread being used for dumping socket buffer data into file handle)
s3eThread* kidloom_loader::Loader::getThreadHandle() {
	return thread_handle;
}

//-------------------------------------------------------------------------
kidloom_loader::Loader::~Loader() {
	if (status == COMPLETE && content_length > 0) {
		// If a download was performed and completed, free buffer memory
		s3eFree(content);
	}
	if (net_connection) {
		net_connection->Cancel(true);
	}
	net_connection = NULL;
	thread_handle = NULL;
	file_handle = NULL;
	local_url = NULL;
	local_url = NULL;
	callback = NULL;
	content = NULL;
	status = NONE;
	IwDebugTraceLinePrintf("AHTTPLoader: Loader destroyed\n");
}
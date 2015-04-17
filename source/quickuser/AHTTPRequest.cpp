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

#include "AHTTPRequest.h"
#include "QLuaHelpers.h"
#include <iostream>
#include <string>

// List of active loaders
ahttp_request::AHTTPRequest* ahttp_request::requests[MAX_CONCURRENT_REQUESTS];

// Current loader count (for list consistency purposes)
uint32 ahttp_request::current_downloads = 0;

std::map<char*, char*> ahttp_request::request_headers;

//-------------------------------------------------------------------------
void ahttp_request::addHeader(char* header, char* value) {
	request_headers[header] = value;
}

//-------------------------------------------------------------------------
void ahttp_request::remHeader(char* header) {
	request_headers[header] = NULL;
}

//-------------------------------------------------------------------------
void ahttp_request::flushHeaders() {
	request_headers.empty();
}

//-------------------------------------------------------------------------
// Loaders call this method when completed, to be destroyed and removed.
void ahttp_request::on_load_complete(void* instance) {
	
	AHTTPRequest* request = (AHTTPRequest*)instance;
	
	uint slot = request->getSlot();
	requests[slot] = NULL;
	current_downloads--;
	request->notifyStatus();
	request->~AHTTPRequest();
}

//-------------------------------------------------------------------------
// The C++ API for LUA will call this method to begin a download
bool ahttp_request::openRequest(char* url, uint32 method, char* body) {
	
	if (current_downloads >= MAX_CONCURRENT_REQUESTS) {
		return false;
	}

	for (int i = 0; i < MAX_CONCURRENT_REQUESTS; i++) {
		if (requests[i] == NULL) {
			AHTTPRequest* loader = new AHTTPRequest(i, url, method, body);

			requests[i] = loader;

			loader->setCompleteCallback(on_load_complete);
			current_downloads++;
			IwDebugTraceLinePrintf("AHTTP: OPENING SLOT [%d] (current requests = %d)\n", i, current_downloads);
			IwDebugTraceLinePrintf(">>>    REMOTE_URL [%s]\n", url);
			loader->load();
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 ahttp_request::http_dataReceived(void* sysData, void* instance) {
	AHTTPRequest* request = (AHTTPRequest*)instance;
	if (request->getStatus() != DOWNLOADING) {
		IwDebugTraceLinePrintf("AHTTP: REQUEST %d INVALID STATE [%s], expected [in_progress] \n", request->getSlot(), request->getStatusString());
		request->~AHTTPRequest();
		return 1;
	}
	request->readContent();
	return 0;
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to request's implementation
int32 ahttp_request::http_headersReceived(void* sysData, void* instance) {
	AHTTPRequest* request = (AHTTPRequest*)instance;
	if (request->getStatus() != CONNECTING) {
		IwDebugTraceLinePrintf("AHTTP: REQUEST %d INVALID STATE [%s], expected [begin] \n", request->getSlot(), request->getStatusString());
		//request->~AHTTPRequest();
		return 1;
	}
	request->readHeader();
	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
// REQUEST CLASS
ahttp_request::AHTTPRequest::AHTTPRequest(uint32 num, char* url, uint32 method, char* body) {
	
	slot = num;
	remote_url = url;
	http_method = method;
	request_body = body;
	error_code = 0;
	bytes_read = 0;
	bytes_written = 0;
	status = NONE;
	this->net_connection = new CIwHTTP();
	
}

void ahttp_request::AHTTPRequest::setHeaders(std::map<char*, char*> headers) {
	// show content:
	for (std::map<char*, char*>::iterator it = headers.begin(); it != headers.end(); ++it) {
		std::string strvalue = it->second;
		this->net_connection->SetRequestHeader(it->first, strvalue);
	}
}
//-------------------------------------------------------------------------
// Begins the request process for the set pair of URLs
bool ahttp_request::AHTTPRequest::load() {

	if (status != NONE) {
		return false;
	}
	bool success = false;
	
	switch (http_method) {
		case GET:
			success = this->net_connection->Get(remote_url, http_headersReceived, this) != S3E_RESULT_SUCCESS;
			break;
		case POST:
			int32 bodylen = strlen(request_body)*sizeof(request_body);
			success = this->net_connection->Post(remote_url, request_body, bodylen, http_headersReceived, this) != S3E_RESULT_SUCCESS;
			break;
	}

	if (success) {
		setStatus(CONNECTING);
	} else {
		IwDebugTraceLinePrintf("AHTTPRequest[%d]: Request failed -> CONNECTION ERROR (IMMEDIATE)\n", slot);
		setStatus(ERROR);
		error_code = 1;
		notifyStatus();
		return false;
	}
	return success;
}

//-------------------------------------------------------------------------
// Checks for connection or HTTP errors and returns true if all is OK
bool ahttp_request::AHTTPRequest::checkRequestStatus() {

	bool res = false;
	if (net_connection->GetStatus() == S3E_RESULT_ERROR) {
		// Socket or Connection Error (fatal)
		setStatus(ERROR);
		error_code = 1;
		res = false;
		IwDebugTraceLinePrintf("AHTTPRequest[%d]: Request failed -> CONNECTION ERROR\n", slot);
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
			IwDebugTraceLinePrintf("AHTTPRequest[%d]: Request failed -> HTTP ERROR %d\n", slot, error_code);
			break;
		}
	}
	return res;
}

//-------------------------------------------------------------------------
// Read header data from an established HTTP connection
bool ahttp_request::AHTTPRequest::readHeader() {

	if (checkRequestStatus() == false) {
		//Notify the LUA layer if there was an error
		notifyStatus();
		callback(this);
		return false;
	}
	IwDebugTraceLinePrintf("AHTTPRequest[%d]: Headers Received, [%d] bytes of data expected\n", slot, net_connection->ContentExpected());
	
	// Calculate buffer size and allocate buffer memory
buffer_size = net_connection->ContentExpected() - net_connection->ContentReceived();
IwDebugTraceLinePrintf(">>>          Allocating result buffer of [%d] bytes\n", buffer_size);
result_buffer = (char*)s3eMalloc(buffer_size + 1);
buffer_size = (buffer_size > BASE_READ_CHUNK) ? BASE_READ_CHUNK : buffer_size;

IwDebugTraceLinePrintf(">>>          Allocating read buffer of [%d] bytes\n", buffer_size);
buffer = (char*)s3eMalloc(buffer_size + 1);
IwAssertMsg(AHTTP, buffer != NULL, ("AHTTPRequest was unable to allocate [%d bytes] to the socket read buffer.", buffer_size));

net_connection->ReadDataAsync(buffer, buffer_size, HTTP_READ_TIMEOUT, http_dataReceived, this);
setStatus(DOWNLOADING);
return true;
}

//-------------------------------------------------------------------------
// Read content data from an established HTTP connection
bool ahttp_request::AHTTPRequest::readContent() {
	if (checkRequestStatus() == false) {
		//notify the lua layer if there was an error
		notifyStatus();
		return false;
	}
	IwDebugTraceLinePrintf("AHTTPRequest[%d]: Received data, [%d of %d] bytes downloaded\n", slot, net_connection->ContentReceived(), net_connection->ContentExpected());

	uint32 read_len = net_connection->ContentReceived() - bytes_read;
	bytes_read = net_connection->ContentReceived();

	result_buffer = strcat(result_buffer, buffer);

	if (net_connection->ContentFinished()) {
		// free buffer memory, set complete status and notify
		s3eFree(buffer);
		setStatus(COMPLETE);
		callback(this);
		return true;
	}
	else {
		// Calculate next buffer size and begin another read operation
		buffer_size = net_connection->ContentExpected() - net_connection->ContentReceived();
		buffer_size = (buffer_size > BASE_READ_CHUNK) ? BASE_READ_CHUNK : buffer_size;

		/*
		// Reallocate buffer memory and read next chunk
		if (sizeof(buffer) != buffer_size + 1) {
		IwDebugTraceLinePrintf(">>>          Rellocating read buffer of [%d] bytes\n", buffer_size);
		buffer = (char*)s3eRealloc(buffer, buffer_size + 1);
		}
		*/
		net_connection->ReadDataAsync(buffer, buffer_size, HTTP_READ_TIMEOUT, http_dataReceived, this);
	}
	notifyStatus();
	return true;
}

//-------------------------------------------------------------------------
// Sends a status event to the LUA layer
void ahttp_request::AHTTPRequest::notifyStatus() {

	IwDebugTraceLinePrintf("AHTTPRequest[%d]: sending status event...\n", slot);
	quick::LUA_EVENT_PREPARE("http_event");
	quick::LUA_EVENT_SET_STRING("url", remote_url);
	if (net_connection && status != ERROR) {
		float percent = (float)net_connection->ContentReceived() / (float)net_connection->ContentLength();
		quick::LUA_EVENT_SET_NUMBER("percent", percent);
	}
	quick::LUA_EVENT_SET_STRING("status", getStatusString());
	if (status == ERROR) {
		quick::LUA_EVENT_SET_NUMBER("ecode", error_code);
	}
	if (status == COMPLETE) {
		quick::LUA_EVENT_SET_STRING("result", getResult());
	}
	quick::LUA_EVENT_SEND();
	lua_pop(quick::g_L, 1);
}

//-------------------------------------------------------------------------
// Changes loader status and prints debug state data to log
void ahttp_request::AHTTPRequest::setStatus(RequestStatus newStatus) {
	status = newStatus;
	IwDebugTraceLinePrintf("AHTTPRequest[%d]: Changed state to [%s]\n", slot, this->getStatusString());
}

//-------------------------------------------------------------------------
// sets the complete callback to call when the loader finishes its task
void ahttp_request::AHTTPRequest::setCompleteCallback(loaderCallback cb) {
	if (cb) {
		callback = cb;
	}
}

//-------------------------------------------------------------------------
// GETTER: current loader status, as defined by RequestStatus Enum
RequestStatus ahttp_request::AHTTPRequest::getStatus() {
	return status;
}

//-------------------------------------------------------------------------
// GETTER: request result if available, null otherwise
char* ahttp_request::AHTTPRequest::getResult() {
	if (status == COMPLETE || status == ERROR) {
		return result_buffer;
	}
	return NULL;
}

//-------------------------------------------------------------------------
// GETTER: string status, to be passed directly to LUA event handlers (LUA status enum)
char* ahttp_request::AHTTPRequest::getStatusString() {
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
// GETTER: slot number corresponding to the loader's place in the loader array
uint32 ahttp_request::AHTTPRequest::getSlot() {
	return slot;
}

//-------------------------------------------------------------------------
// GETTER: Remote server URL where the data is hosted
char* ahttp_request::AHTTPRequest::getRemoteURL() {
	return remote_url;
}

//-------------------------------------------------------------------------
// GETTER: Connection object (CIwHTTP socket wrapper)
CIwHTTP* ahttp_request::AHTTPRequest::getConnection() {
	return net_connection;
}

//-------------------------------------------------------------------------
ahttp_request::AHTTPRequest::~AHTTPRequest() {
	
	if (status == DESTROYED) {
		IwDebugTraceLinePrintf("AHTTPRequest: Attempted to destroy an already destroyed AHTTPRequest \n");
		return;
	}

	IwDebugTraceLinePrintf("AHTTPRequest[%d]: Destroying AHTTPRequest [FILE:%s][URL:%s] \n", slot, remote_url);
	if (net_connection && !net_connection->ContentFinished()) {
		net_connection->Cancel(true);
	}
	net_connection = NULL;
	
	if (sizeof(result_buffer)) {
		s3eFree(result_buffer);
	}
	result_buffer = NULL;
	remote_url = NULL;
	callback = NULL;
	buffer_size = NULL;
	bytes_read = NULL;
	buffer = NULL;
	slot = NULL;
	status = DESTROYED;
}
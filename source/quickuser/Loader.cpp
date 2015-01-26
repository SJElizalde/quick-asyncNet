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
#include <iostream>
#include <string>

// List of active loaders
kidloom_loader::Loader* kidloom_loader::loaders[MAX_CONCURRENT_DOWNLOADS];

// Current loader count (for list consistency purposes)
uint32 kidloom_loader::current_downlaods = 0;

//-------------------------------------------------------------------------
// Loaders call this method when completed, to be destroyed and removed.
void kidloom_loader::on_load_complete(void* instance) {
	
	Loader* loader = (Loader*)instance;
	
	uint slot = loader->getSlot();
	loaders[slot] = NULL;
	current_downlaods--;
	loader->notifyStatus();
	loader->~Loader();
}

//-------------------------------------------------------------------------
// The C++ API for LUA will call this method to begin a download
void kidloom_loader::openRequest(char* url, char* filename) {
	
	if (current_downlaods >= MAX_CONCURRENT_DOWNLOADS) {
		return;
	}

	for (int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++) {
		if (loaders[i] == NULL) {
			Loader* loader = new Loader(i, url, filename);

			loaders[i] = loader;

			loader->setCompleteCallback(on_load_complete);
			current_downlaods++;
			IwDebugTraceLinePrintf("AHTTP: OPENING SLOT [%d] (current downloads = %d)\n", i, current_downlaods);
			IwDebugTraceLinePrintf(">>>    REMOTE_URL [%s]\n", url);
			IwDebugTraceLinePrintf(">>>    LOCAL_URL [%s]\n", filename);
			loader->load();
			break;
		}
	}
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 kidloom_loader::http_dataReceived(void* sysData, void* instance) {
	Loader* loader = (Loader*)instance;
	if (loader->getStatus() != DOWNLOADING) {
		IwDebugTraceLinePrintf("AHTTP: LOADER %d INVALID STATE [%s], expected [in_progress] \n", loader->getSlot(), loader->getStatusString());
		loader->~Loader();
		return 1;
	}
	loader->readContent();
	return 0;
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 kidloom_loader::http_headersReceived(void* sysData, void* instance) {
	Loader* loader = (Loader*)instance;
	if (loader->getStatus() != CONNECTING) {
		IwDebugTraceLinePrintf("AHTTP: LOADER %d INVALID STATE [%s], expected [begin] \n", loader->getSlot(), loader->getStatusString());
		loader->~Loader();
		return 1;
	}
	loader->readHeader();
	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
// LOADER CLASS
kidloom_loader::Loader::Loader(uint32 num, char* url, char* filename) {
	
	slot = num;
	remote_url = url;
	
	local_url = (char*)s3eMalloc(strlen(filename)*sizeof(filename));
	strcpy(local_url, filename);
	error_code = 0;
	bytes_read = 0;
	status = NONE;
	this->net_connection = new CIwHTTP();
}

//-------------------------------------------------------------------------
// Begins the loading process for the set pair of URLs
bool kidloom_loader::Loader::load() {

	if (status != NONE) {
		return false;
	}
	if (this->net_connection->Get(remote_url, http_headersReceived, this) != S3E_RESULT_SUCCESS) {
		IwDebugTraceLinePrintf("AHTTPLoader[%d]: Request failed -> CONNECTION ERROR (IMMEDIATE)\n", slot);
		setStatus(ERROR);
		error_code = 1;
		notifyStatus();
		return false;
	}
	setStatus(CONNECTING);
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
		IwDebugTraceLinePrintf("AHTTPLoader[%d]: Request failed -> CONNECTION ERROR\n", slot);
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
			IwDebugTraceLinePrintf("AHTTPLoader[%d]: Request failed -> HTTP ERROR %d\n", slot, error_code);
			break;
		}
	}
	return res;
}

//-------------------------------------------------------------------------
// Read header data from an established HTTP connection
bool kidloom_loader::Loader::readHeader() {

	if (checkRequestStatus() == false) {
		//Notify the LUA layer if there was an error
		notifyStatus();
		callback(this);
		return false;
	}
	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Headers Received, [%d] bytes of data expected\n", slot, net_connection->ContentExpected());
	IwDebugTraceLinePrintf(">>>          Opening file for [%s]\n", local_url);
	file_handle = s3eFileOpen(local_url, "w");

	// Calculate buffer size and allocate buffer memory
	buffer_size = net_connection->ContentExpected() - net_connection->ContentReceived();
	buffer_size = (buffer_size > BASE_READ_CHUNK) ? BASE_READ_CHUNK : buffer_size;
	
	IwDebugTraceLinePrintf(">>>          Allocating read buffer of [%d] bytes\n", buffer_size);
	buffer = (char*)s3eMalloc(buffer_size+1);
	IwAssertMsg(AHTTP, buffer != NULL, ("Loader was unable to allocate [%d bytes] to the socket read buffer.", buffer_size));

	net_connection->ReadDataAsync(buffer, buffer_size, HTTP_READ_TIMEOUT, http_dataReceived, this);
	setStatus(DOWNLOADING);
	return true;
}

//-------------------------------------------------------------------------
// Read content data from an established HTTP connection
bool kidloom_loader::Loader::readContent() {
	if (checkRequestStatus() == false) {
		//notify the lua layer if there was an error
		notifyStatus();
		return false;
	}
	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Received data, [%d of %d] bytes downloaded\n", slot, net_connection->ContentReceived(), net_connection->ContentExpected());
	// write buffer contents to file
	if (file_handle == NULL) {
		IwDebugTraceLinePrintf("AHTTPLoader[%d]: Local file handle [%s] failed to open correctly\n", slot, local_url);
		setStatus(ERROR);
		callback(this);
		return false;
	}
	IwDebugTraceLinePrintf(">>>          Writing buffer of [%d] bytes to %s\n", buffer_size, local_url);
	uint32 read_len = net_connection->ContentReceived() - bytes_read;
	uint32 written = s3eFileWrite(buffer, read_len, 1, file_handle);
	bytes_read = net_connection->ContentReceived();
	IwDebugTraceLinePrintf(">>>          Total written to disk is [%d] bytes\n", bytes_read);

	if (net_connection->ContentFinished()) {
		// Close file, free buffer memory, set complete status and notify
		s3eFileClose(file_handle);
		s3eFree(buffer);
		setStatus(COMPLETE);
		callback(this);
		return true;
	}
	else {
		// Calculate next buffer size and begin another read operation
		buffer_size = net_connection->ContentExpected() - net_connection->ContentReceived();
		buffer_size = (buffer_size > BASE_READ_CHUNK) ? BASE_READ_CHUNK : buffer_size;


		// Reallocate buffer memory and read next chunk
		if (sizeof(buffer) != buffer_size + 1) {
			IwDebugTraceLinePrintf(">>>          Rellocating read buffer of [%d] bytes\n", buffer_size);
			buffer = (char*)s3eRealloc(buffer, buffer_size + 1);
		}
		net_connection->ReadDataAsync(buffer, buffer_size, HTTP_READ_TIMEOUT, http_dataReceived, this);
	}
	notifyStatus();
	return true;
}

//-------------------------------------------------------------------------
// Sends a status event to the LUA layer
void kidloom_loader::Loader::notifyStatus() {

	IwDebugTraceLinePrintf("AHTTPLoader[%d]: sending status event...\n", slot);
	quick::LUA_EVENT_PREPARE("http_event");
	quick::LUA_EVENT_SET_STRING("url", remote_url);
	quick::LUA_EVENT_SET_STRING("filename", local_url);
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
	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Changed state to [%s]\n", slot, this->getStatusString());
}

//-------------------------------------------------------------------------
// sets the complete callback to call when the loader finishes its task
void kidloom_loader::Loader::setCompleteCallback(loaderCallback cb) {
	if (cb) {
		callback = cb;
	}
}

//-------------------------------------------------------------------------
// GETTER: current loader status, as defined by LoaderStatus Enum
LoaderStatus kidloom_loader::Loader::getStatus() {
	return status;
}

//-------------------------------------------------------------------------
// GETTER: string status, to be passed directly to LUA event handlers (LUA status enum)
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
// GETTER: slot number corresponding to the loader's place in the loader array
uint32 kidloom_loader::Loader::getSlot() {
	return slot;
}

//-------------------------------------------------------------------------
// GETTER: Local file URL where remote data is being stored
char* kidloom_loader::Loader::getLocalURL() {
	return local_url;
}

//-------------------------------------------------------------------------
// GETTER: Remote server URL where the data is hosted
char* kidloom_loader::Loader::getRemoteURL() {
	return remote_url;
}

//-------------------------------------------------------------------------
// GETTER: Connection object (CIwHTTP socket wrapper)
CIwHTTP* kidloom_loader::Loader::getConnection() {
	return net_connection;
}

//-------------------------------------------------------------------------
// GETTER: Filesystem file handle (always write mode)
s3eFile* kidloom_loader::Loader::getFileHandle() {
	return file_handle;
}

//-------------------------------------------------------------------------
kidloom_loader::Loader::~Loader() {
	
	if (status == DESTROYED) {
		IwDebugTraceLinePrintf("AHTTPLoader: Attempted to destroy an already destroyed Loader \n");
		return;
	}

	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Destroying Loader [FILE:%s][URL:%s] \n", slot, local_url, remote_url);
	if (net_connection && !net_connection->ContentFinished()) {
		net_connection->Cancel(true);
	}
	net_connection = NULL;
	if (status == ERROR && file_handle) {
		//If there was an error and the file was written, remove it.
		s3eFileDelete(local_url);
	}
	file_handle = NULL;
	if (sizeof(local_url)) {
		s3eFree(local_url);
	}
	local_url = NULL;
	remote_url = NULL;
	callback = NULL;
	buffer_size = NULL;
	bytes_read = NULL;
	buffer = NULL;
	slot = NULL;
	status = DESTROYED;
}
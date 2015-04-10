/*
* (C) 2014-2015 Kidloom.
*
* This class is designed to funnel a datastream from an HTTP connection into a file
* handle within the local filesystem. Asynchronous data reading is used with the IWHTTP
* library to enable non-blocking downloads in paralell for the LUA layer.
*
* Author: S.J.Elizalde
*         santa@kidloom.com
*/

#include "AHTTPLoader.h"
#include "QLuaHelpers.h"
#include <iostream>
#include <string>

// List of active loaders
ahttp_loader::AHTTPLoader* ahttp_loader::loaders[MAX_CONCURRENT_DOWNLOADS];

// Current loader count (for list consistency purposes)
uint32 ahttp_loader::current_downloads = 0;

//-------------------------------------------------------------------------
// Loaders call this method when completed, to be destroyed and removed.
void ahttp_loader::on_load_complete(void* instance) {
	
	AHTTPLoader* loader = (AHTTPLoader*)instance;
	
	uint slot = loader->getSlot();
	loaders[slot] = NULL;
	current_downloads--;
	loader->notifyStatus();
	loader->~AHTTPLoader();
}

//-------------------------------------------------------------------------
// The C++ API for LUA will call this method to begin a download
bool ahttp_loader::openRequest(char* url, char* filename) {
	
	if (current_downloads >= MAX_CONCURRENT_DOWNLOADS) {
		return false;
	}

	for (int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++) {
		if (loaders[i] == NULL) {
			AHTTPLoader* loader = new AHTTPLoader(i, url, filename);

			loaders[i] = loader;

			loader->setCompleteCallback(on_load_complete);
			current_downloads++;
			IwDebugTraceLinePrintf("AHTTP: OPENING SLOT [%d] (current downloads = %d)\n", i, current_downloads);
			IwDebugTraceLinePrintf(">>>    REMOTE_URL [%s]\n", url);
			IwDebugTraceLinePrintf(">>>    LOCAL_URL [%s]\n", filename);
			loader->load();
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 ahttp_loader::http_dataReceived(void* sysData, void* instance) {
	AHTTPLoader* loader = (AHTTPLoader*)instance;
	if (loader->getStatus() != RequestStatus::DOWNLOADING) {
		IwDebugTraceLinePrintf("AHTTP: LOADER %d INVALID STATE [%s], expected [in_progress] \n", loader->getSlot(), loader->getStatusString());
		loader->~AHTTPLoader();
		return 1;
	}
	loader->readContent();
	return 0;
}

//-------------------------------------------------------------------------
// Static callbacks delegate socket data processing to loader's implementation
int32 ahttp_loader::http_headersReceived(void* sysData, void* instance) {
	AHTTPLoader* loader = (AHTTPLoader*)instance;
	if (loader->getStatus() != RequestStatus::CONNECTING) {
		IwDebugTraceLinePrintf("AHTTP: LOADER %d INVALID STATE [%s], expected [begin] \n", loader->getSlot(), loader->getStatusString());
		loader->~AHTTPLoader();
		return 1;
	}
	loader->readHeader();
	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------
// LOADER CLASS
ahttp_loader::AHTTPLoader::AHTTPLoader(uint32 num, char* url, char* filename) {
	
	slot = num;
	remote_url = url;
	
	local_url = (char*)s3eMalloc(strlen(filename)*sizeof(filename));
	strcpy(local_url, filename);
	error_code = 0;
	bytes_read = 0;
	bytes_written = 0;
	status = RequestStatus::NONE;
	this->net_connection = new CIwHTTP();
}

//-------------------------------------------------------------------------
// Begins the loading process for the set pair of URLs
bool ahttp_loader::AHTTPLoader::load() {

	if (status != RequestStatus::NONE) {
		return false;
	}
	if (this->net_connection->Get(remote_url, http_headersReceived, this) != S3E_RESULT_SUCCESS) {
		IwDebugTraceLinePrintf("AHTTPLoader[%d]: Request failed -> CONNECTION ERROR (IMMEDIATE)\n", slot);
		setStatus(RequestStatus::ERROR);
		error_code = 1;
		notifyStatus();
		return false;
	}
	setStatus(CONNECTING);
	return true;
}

//-------------------------------------------------------------------------
// Checks for connection or HTTP errors and returns true if all is OK
bool ahttp_loader::AHTTPLoader::checkRequestStatus() {

	bool res = false;
	if (net_connection->GetStatus() == S3E_RESULT_ERROR) {
		// Socket or Connection Error (fatal)
		setStatus(RequestStatus::ERROR);
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
			setStatus(RequestStatus::ERROR);
			res = false;
			IwDebugTraceLinePrintf("AHTTPLoader[%d]: Request failed -> HTTP ERROR %d\n", slot, error_code);
			break;
		}
	}
	return res;
}

//-------------------------------------------------------------------------
// Read header data from an established HTTP connection
bool ahttp_loader::AHTTPLoader::readHeader() {

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
	IwAssertMsg(AHTTP, buffer != NULL, ("AHTTPLoader was unable to allocate [%d bytes] to the socket read buffer.", buffer_size));

	net_connection->ReadDataAsync(buffer, buffer_size, HTTP_READ_TIMEOUT, http_dataReceived, this);
	setStatus(DOWNLOADING);
	return true;
}

//-------------------------------------------------------------------------
// Read content data from an established HTTP connection
bool ahttp_loader::AHTTPLoader::readContent() {
	if (checkRequestStatus() == false) {
		//notify the lua layer if there was an error
		notifyStatus();
		return false;
	}
	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Received data, [%d of %d] bytes downloaded\n", slot, net_connection->ContentReceived(), net_connection->ContentExpected());
	// write buffer contents to file
	if (file_handle == NULL) {
		IwDebugTraceLinePrintf("AHTTPLoader[%d]: Local file handle [%s] failed to open correctly\n", slot, local_url);
		setStatus(RequestStatus::ERROR);
		callback(this);
		return false;
	}
	IwDebugTraceLinePrintf(">>>          Writing buffer of [%d] bytes to %s\n", buffer_size, local_url);
	uint32 read_len = net_connection->ContentReceived() - bytes_read;
	//Don't touch the file if there's nothing to write
	if (read_len > 0) {
		uint32 written = s3eFileWrite(buffer, read_len, 1, file_handle);
		bytes_written += written;
	}
	bytes_read = net_connection->ContentReceived();
	
	
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
void ahttp_loader::AHTTPLoader::notifyStatus() {

	IwDebugTraceLinePrintf("AHTTPLoader[%d]: sending status event...\n", slot);
	quick::LUA_EVENT_PREPARE("http_event");
	quick::LUA_EVENT_SET_STRING("url", remote_url);
	quick::LUA_EVENT_SET_STRING("filename", local_url);
	if (net_connection && status != RequestStatus::ERROR) {
		float percent = (float)net_connection->ContentReceived() / (float)net_connection->ContentLength();
		quick::LUA_EVENT_SET_NUMBER("percent", percent);
	}
	quick::LUA_EVENT_SET_STRING("status", getStatusString());
	if (status == RequestStatus::ERROR) {
		quick::LUA_EVENT_SET_NUMBER("ecode", error_code);
	}
	quick::LUA_EVENT_SEND();
	lua_pop(quick::g_L, 1);
}

//-------------------------------------------------------------------------
// Changes loader status and prints debug state data to log
void ahttp_loader::AHTTPLoader::setStatus(RequestStatus newStatus) {
	status = newStatus;
	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Changed state to [%s]\n", slot, this->getStatusString());
}

//-------------------------------------------------------------------------
// sets the complete callback to call when the loader finishes its task
void ahttp_loader::AHTTPLoader::setCompleteCallback(loaderCallback cb) {
	if (cb) {
		callback = cb;
	}
}

//-------------------------------------------------------------------------
// GETTER: current loader status, as defined by RequestStatus Enum
RequestStatus ahttp_loader::AHTTPLoader::getStatus() {
	return status;
}

//-------------------------------------------------------------------------
// GETTER: string status, to be passed directly to LUA event handlers (LUA status enum)
char* ahttp_loader::AHTTPLoader::getStatusString() {
	switch (status)
	{
	case RequestStatus::CONNECTING:
		return "begin";
		break;
	case RequestStatus::DOWNLOADING:
		return "in_progress";
		break;
	case RequestStatus::COMPLETE:
		return "complete";
		break;
	case RequestStatus::ERROR:
		return "error";
		break;
	default:
		return "none ";
		break;
	}
}

//-------------------------------------------------------------------------
// GETTER: slot number corresponding to the loader's place in the loader array
uint32 ahttp_loader::AHTTPLoader::getSlot() {
	return slot;
}

//-------------------------------------------------------------------------
// GETTER: Local file URL where remote data is being stored
char* ahttp_loader::AHTTPLoader::getLocalURL() {
	return local_url;
}

//-------------------------------------------------------------------------
// GETTER: Remote server URL where the data is hosted
char* ahttp_loader::AHTTPLoader::getRemoteURL() {
	return remote_url;
}

//-------------------------------------------------------------------------
// GETTER: Connection object (CIwHTTP socket wrapper)
CIwHTTP* ahttp_loader::AHTTPLoader::getConnection() {
	return net_connection;
}

//-------------------------------------------------------------------------
// GETTER: Filesystem file handle (always write mode)
s3eFile* ahttp_loader::AHTTPLoader::getFileHandle() {
	return file_handle;
}

//-------------------------------------------------------------------------
ahttp_loader::AHTTPLoader::~AHTTPLoader() {
	
	if (status == RequestStatus::DESTROYED) {
		IwDebugTraceLinePrintf("AHTTPLoader: Attempted to destroy an already destroyed AHTTPLoader \n");
		return;
	}

	IwDebugTraceLinePrintf("AHTTPLoader[%d]: Destroying AHTTPLoader [FILE:%s][URL:%s] \n", slot, local_url, remote_url);
	if (net_connection && !net_connection->ContentFinished()) {
		net_connection->Cancel(true);
	}
	net_connection = NULL;
	if (status == RequestStatus::ERROR && file_handle) {
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
	status = RequestStatus::DESTROYED;
}
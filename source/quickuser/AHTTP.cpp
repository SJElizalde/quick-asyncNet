/*
* (C) 2014-2015 Kidloom.
*
* Author: S.J.Elizalde
*         santa@kidloom.com
*/
#include "QLuaHelpers.h"
#include "AHTTP.h"

#define AHTTP_MAX_CONNECTIONS 5

//------------------------------------------------------------------------------
uint32 ahttp::getMaxConnections() {
	return AHTTP_MAX_CONNECTIONS;
}
//------------------------------------------------------------------------------
uint32 ahttp::getActiveConnections() {
	uint32 current_downloads = ahttp_loader::current_downloads + ahttp_request::current_downloads;
	IwDebugTraceLinePrintf("AHTTP: open sockets -> [%d]\n", current_downloads);
	return current_downloads;
}
//------------------------------------------------------------------------------
uint32 ahttp::getAvailableConnections() {
	return AHTTP_MAX_CONNECTIONS - getActiveConnections();
}
//------------------------------------------------------------------------------
bool ahttp::downloadURL(char* url, char* filename) {
	IwDebugTraceLinePrintf("AHTTP: downloadURL called.\n");
	if (getActiveConnections() >= AHTTP_MAX_CONNECTIONS) {
		return false;
	}
	return ahttp_loader::openRequest(url, filename);
}
//------------------------------------------------------------------------------
bool ahttp::requestURL(char* url, char* method, char* body) {
	IwDebugTraceLinePrintf("AHTTP: requestURL called\n");
	if (getActiveConnections() >= AHTTP_MAX_CONNECTIONS) {
		return false;
	}
	uint32 urlmethod = 0;
	std::string strmethod = method;
	if (strmethod == "post") { urlmethod = 1; }
	if (strmethod == "put") { urlmethod = 2; }
	if (strmethod == "delete") { urlmethod = 3; }
	
	return ahttp_request::openRequest(url, urlmethod, body);
}

//------------------------------------------------------------------------------
void ahttp::addRequestHeader(char* key, char* value) {
	IwDebugTraceLinePrintf("AHTTP: Setting header '%s': %s\n", key, value);
	ahttp_request::addHeader(key, value);
}

//------------------------------------------------------------------------------
void ahttp::removeRequestHeader(char* key) {
	IwDebugTraceLinePrintf("AHTTP: Removing header '%s'\n", key);
	ahttp_request::remHeader(key);
}

//------------------------------------------------------------------------------
void ahttp::flushRequestHeaders() {
	IwDebugTraceLinePrintf("AHTTP: Flushing all headers\n");
	ahttp_request::flushHeaders();
}

// APFSDS

//------------------------------------------------------------------------------
void ahttp::test(char* message) {
	IwDebugTraceLinePrintf("AHTTP: testing LUA binding.\n");
	quick::LUA_EVENT_PREPARE("complete");
	quick::LUA_EVENT_SET_STRING("url", "www.foo.com/test/test_file.txt");
	quick::LUA_EVENT_SET_STRING("filename", "test/test_file.txt");
	quick::LUA_EVENT_SET_STRING("status", message);
	quick::LUA_EVENT_SEND();
	lua_pop(quick::g_L, 1);
}
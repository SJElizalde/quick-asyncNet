/*
* (C) 2014-2015 Kidloom.
*
* Author: S.J.Elizalde
*         santa@kidloom.com
*/
#include "QLuaHelpers.h"
#include "AHTTP.h"

#define AHTTP_MAX_CONNECTIONS 6

//------------------------------------------------------------------------------
uint32 ahttp::getMaxConnections() {
	return AHTTP_MAX_CONNECTIONS;
}
//------------------------------------------------------------------------------
uint32 ahttp::getActiveConnections() {
	return ahttp_loader::current_downloads + ahttp_request::current_downloads;
}
//------------------------------------------------------------------------------
uint32 ahttp::getAvailableConnections() {
	return AHTTP_MAX_CONNECTIONS - getActiveConnections();
}
//------------------------------------------------------------------------------
bool ahttp::downloadURL(char* url, char* filename) {
	printf("AHTTP: downloadURL called.\n");
	if (getActiveConnections() >= AHTTP_MAX_CONNECTIONS) {
		return false;
	}
	return ahttp_loader::openRequest(url, filename);
}
//------------------------------------------------------------------------------
bool ahttp::requestURL(char* url, char* method, char* body) {
	printf("AHTTP: requestURL called\n");
	uint32 current = ahttp_loader::current_downloads + ahttp_request::current_downloads;
	if (getActiveConnections() >= AHTTP_MAX_CONNECTIONS) {
		return false;
	}
	uint32 urlmethod = 0;
	std::string strmethod = method;
	if (strmethod == "post") {
		urlmethod = 1;
	}

	return ahttp_request::openRequest(url, urlmethod, body);
}

//------------------------------------------------------------------------------
void ahttp::test(char* message) {
	printf("AHTTP: testing binding.\n");
	printf(message);
	quick::LUA_EVENT_PREPARE("complete");
	quick::LUA_EVENT_SET_STRING("url", "www.foo.com/test/test_file.txt");
	quick::LUA_EVENT_SET_STRING("filename", "test/test_file.txt");
	quick::LUA_EVENT_SET_STRING("status", message);
	quick::LUA_EVENT_SEND();
	lua_pop(quick::g_L, 1);
}
/*
* (C) 2014-2015 Kidloom.
*
* Author: S.J.Elizalde
*         santa@kidloom.com
*/
#include "QLuaHelpers.h"
#include "AHTTP.h"

#define AHTTP_MAX_CONNECTIONS 3

enum HTTPStatus {
	kNone,
	kDownloading,
	kOK,
	kError,
};

//------------------------------------------------------------------------------
void ahttp::downloadURL(char* url, char* filename) {
	printf("AHTTP: loadURL function\n");
	kidloom_loader::openRequest(url, filename);
}

//------------------------------------------------------------------------------
void ahttp::test(char* message) {
	printf("AHTTP: testing binding\n");
	printf(message);
	quick::LUA_EVENT_PREPARE("complete");
	quick::LUA_EVENT_SET_STRING("url", "www.foo.com/test/test_file.txt");
	quick::LUA_EVENT_SET_STRING("filename", "test/test_file.txt");
	quick::LUA_EVENT_SET_STRING("status", message);
	quick::LUA_EVENT_SEND();
	lua_pop(quick::g_L, 1);
}
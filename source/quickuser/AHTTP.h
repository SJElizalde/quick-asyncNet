//------------------------------------------------------------------------------
// LUA bindings for Kidloom's async-concurrent downloader
//------------------------------------------------------------------------------
#include "AHTTPLoader.h"
#include "AHTTPRequest.h"

#ifndef __AHTTP_H
#define __AHTTP_H

// tolua_begin
namespace ahttp {

	// Public interface
	void test(char*);

	uint32 getMaxConnections();
	uint32 getActiveConnections();
	uint32 getAvailableConnections();

	// Main API functions
	bool downloadURL(char*, char*);
	bool requestURL(char*, char*, char*);

	// Request Headers
	void addRequestHeader(char*, char*);
	void removeRequestHeader(char*);
	void flushRequestHeaders();
}
// tolua_end

#endif // __AHTTP_H
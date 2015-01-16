//------------------------------------------------------------------------------
// LUA bindings for Kidloom's async-concurrent downloader
//------------------------------------------------------------------------------
#include "Loader.h"

#ifndef __AHTTP_H
#define __AHTTP_H

// tolua_begin
namespace ahttp {

	// Public interface
	void test(char*);
	void downloadURL(char*, char*);
}
// tolua_end

#endif // __AHTTP_H
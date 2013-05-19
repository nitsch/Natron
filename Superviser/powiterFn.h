//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com
#ifndef PowiterOsX_powiterFn_h
#define PowiterOsX_powiterFn_h
#include "Superviser/Enums.h"
#include <string>

#ifdef __APPLE__
#define __POWITER_OSX__
#define __POWITER_UNIX__
#elif  defined(_WIN32)
#define __POWITER_WIN32__
#include <windows.h>
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#define __POWITER_UNIX__
#define __POWITER_LINUX__
#endif


#define ROOT "./"
#define CACHE_ROOT_PATH ROOT"Cache/"
#define IMAGES_PATH ROOT"Images/"
#define PLUGINS_PATH ROOT"Plugins"


// debug flag
#define PW_DEBUG


namespace PowiterWindows{
    
#ifdef __POWITER_WIN32__

	/*Converts a std::string to wide string*/
    static std::wstring s2ws(const std::string& s)
    {
        int len;
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
        wchar_t* buf = new wchar_t[len];
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
        std::wstring r(buf);
        delete[] buf;
        return r;
    }

#endif

}
#ifdef __POWITER_WIN32__
#undef strlcpy
#undef strcpy
#undef strlen
/*Windows fix to have safe c string handling functions*/

static size_t strlen(const char* src){
	const char *eos = src;
	while( *eos++ ) ;
	return( eos - src - 1 );
}

static size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t res = strlen(src);
	if (size > 0) {
		size_t length = (res >= size) ? size - 1 : res;
		memcpy(dest, src, length);
		dest[length]='\0';
	}
	return res;
}

static char* strcpy(char* dest,const char* src){
	size_t res = strlen(src);
	if (res > 0) {
		memcpy(dest, src, res);
		dest[res]='\0';
	}
	return dest;
}

#endif
#endif

#pragma once

#ifdef APPLE_PLATFORM_WINDOWS
	#ifdef APPLE_BUILD_DLL
		#define APPLE_API __declspec(dllexport)
	#else
		#define APPLE_API __declspec(dllimport)
    #endif
#else
#error Apple only supports Windows!
#endif
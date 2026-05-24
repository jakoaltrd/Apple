#pragma once

#ifdef APPLE_PLATFORM_WINDOWS
	// For static library, APPLE_API is empty (no dllexport/dllimport needed)
	#define APPLE_API
#else
	#error Apple only supports Windows!
#endif

#ifdef APPLE_DEBUG
	#define APPLE_ENABLE_ASSERTS
#endif

#ifdef APPLE_ENABLE_ASSERTS
	#define APPLE_ASSERT(x, ...) { if(!(x)) { APPLE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define APPLE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)
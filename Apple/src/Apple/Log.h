#pragma once
#include <memory>
#include <spdlog/spdlog.h>
#include "Core.h"
namespace Apple
{
	class APPLE_API Log
	{
	public:
		static void Init();
		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		inline static std::shared_ptr<spdlog::logger> s_CoreLogger;
		inline static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

#define APPLE_CORE_TRACE(...) ::Apple::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define APPLE_CORE_ERROR(...) ::Apple::Log::GetCoreLogger()->error(__VA_ARGS__)
#define APPLE_CORE_WARN(...) ::Apple::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define APPLE_CORE_INFO(...) ::Apple::Log::GetCoreLogger()->info(__VA_ARGS__)

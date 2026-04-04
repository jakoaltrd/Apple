#pragma once

#ifdef APPLE_PLATFORM_WINDOWS
extern Apple::Application* Apple::CreateApplication();

int main()
{ 
	Apple::Log::Init();
	APPLE_CORE_WARN("Initialized Log!");
	APPLE_CORE_INFO("Hello! This is an Apple application!");

	auto app = Apple::CreateApplication();
	app->Run();
	delete app;
}
#endif
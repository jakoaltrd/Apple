#pragma once

#ifdef APPLE_PLATFORM_WINDOWS
extern Apple::Application* Apple::CreateApplication();

int main()
{
	auto app = Apple::CreateApplication();
	app->Run();
	delete app;
}
#endif
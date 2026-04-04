#pragma once

#include "Core.h"

namespace Apple
{
	class APPLE_API Application
	{
		public:
		Application();
		virtual ~Application();
		void Run();
	};
	Application* CreateApplication();
}

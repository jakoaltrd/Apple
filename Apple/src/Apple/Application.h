#pragma once

#include "Core.h"
#include <stdio.h>
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

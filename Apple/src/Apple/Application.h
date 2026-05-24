#pragma once
#include <glad/gl.h>
#include "Core.h"
#include <GLFW/glfw3.h>

namespace Apple
{
	class APPLE_API Application
	{
	public:
		Application();
		virtual ~Application();
		void Run();

	public:
		bool InitializeWindow();
		void ShutdownWindow();
		void Update();

		bool InitializeImGui();
		void ShutdownImGui();
		virtual void OnImGuiRender();

	private:
		void RenderImGui();

		GLFWwindow* m_Window;
		bool m_Running;
	};
	Application* CreateApplication();
}

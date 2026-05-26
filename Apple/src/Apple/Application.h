#pragma once
#include <glad/gl.h>
#include "Core.h"
#include <GLFW/glfw3.h>

namespace Apple
{
	struct MouseButtonArgs
	{
		int Button;
		int Action;
		int Mods;
	};

	struct MouseMoveArgs
	{
		double X;
		double Y;
		double DeltaX;
		double DeltaY;
	};

	struct ScrollArgs
	{
		double XOffset;
		double YOffset;
	};

	struct WindowResizeArgs
	{
		int Width;
		int Height;
	};

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
		virtual void OnRender();
		virtual void OnImGuiRender();

		// Mouse event callbacks
		virtual void OnMouseButton(const MouseButtonArgs& args);
		virtual void OnMouseMove(const MouseMoveArgs& args);
		virtual void OnScroll(const ScrollArgs& args);
		virtual void OnWindowResize(const WindowResizeArgs& args);

		GLFWwindow* GetWindow() { return m_Window; }
		int GetWindowWidth() const { return m_WindowWidth; }
		int GetWindowHeight() const { return m_WindowHeight; }
		bool IsImGuiCapturingMouse() const;

	private:
		void RenderImGui();
		static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
		static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
		static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
		static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

		GLFWwindow* m_Window;
		bool m_Running;
		double m_LastMouseX = 0.0;
		double m_LastMouseY = 0.0;
		int m_WindowWidth = 1280;
		int m_WindowHeight = 720;
	};
	Application* CreateApplication();
}

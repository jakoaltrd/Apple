#include "Application.h"
#include "Log.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <sstream>

namespace Apple
{
	Application::Application()
		: m_Window(nullptr), m_Running(false)
	{
		if (!InitializeWindow())
		{
			APPLE_CORE_ERROR("Failed to initialize window!");
			return;
		}

		if (!InitializeImGui())
		{
			APPLE_CORE_ERROR("Failed to initialize ImGui!");
			return;
		}
	}

	Application::~Application()
	{
		ShutdownImGui();
		ShutdownWindow();
	}

	bool Application::InitializeWindow()
	{
		if (!glfwInit())
		{
			APPLE_CORE_ERROR("GLFW initialization failed!");
			return false;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		m_Window = glfwCreateWindow(1280, 720, "Apple Engine", nullptr, nullptr);
		if (!m_Window)
		{
			APPLE_CORE_ERROR("Failed to create GLFW window!");
			glfwTerminate();
			return false;
		}

		glfwMakeContextCurrent(m_Window);

		if (!gladLoadGL(glfwGetProcAddress))
		{
			APPLE_CORE_ERROR("Failed to initialize GLAD!");
			return false;
		}

		glClearColor(0.2f, 0.3f, 0.8f, 1.0f);

		m_Running = true;
		return true;
	}

	void Application::ShutdownWindow()
	{
		if (m_Window)
		{
			glfwDestroyWindow(m_Window);
			m_Window = nullptr;
		}
		glfwTerminate();
	}

	bool Application::InitializeImGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 5.0f;
		style.FrameRounding = 3.0f;
		style.GrabRounding = 3.0f;

		if (!ImGui_ImplGlfw_InitForOpenGL(m_Window, true))
		{
			APPLE_CORE_ERROR("Failed to initialize ImGui GLFW implementation!");
			return false;
		}

		const char* glsl_version = "#version 130";
		if (!ImGui_ImplOpenGL3_Init(glsl_version))
		{
			APPLE_CORE_ERROR("Failed to initialize ImGui OpenGL3 implementation!");
			return false;
		}

		APPLE_CORE_INFO("ImGui initialized successfully");
		return true;
	}

	void Application::ShutdownImGui()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		APPLE_CORE_INFO("ImGui shutdown completed");
	}

	void Application::OnImGuiRender()
	{
		// Default implementation - can be overridden by derived classes
		static bool show_demo_window = true;
		if (show_demo_window)
		{
			ImGui::ShowDemoWindow(&show_demo_window);
		}
	}

	void Application::RenderImGui()
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Call the virtual function for derived classes to add their UI
		OnImGuiRender();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void Application::Update()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glfwPollEvents();

		RenderImGui();

		glfwSwapBuffers(m_Window);
	}

	void Application::Run()
	{
		while (m_Running && !glfwWindowShouldClose(m_Window))
		{
			Update();
		}
	}
}
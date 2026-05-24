#include <Apple.h>
#include "imgui.h"

class Sandbox : public Apple::Application
{
public:
	Sandbox()
	{
		APPLE_CLIENT_INFO("Sandbox constructor called!");
	}

	~Sandbox()
	{
		APPLE_CLIENT_INFO("Sandbox destructor called!");
	}

	void OnImGuiRender() override
	{
		// Call parent class implementation first to show demo window
		Application::OnImGuiRender();

		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, World!");                          // Create a window called "Hello, World!"
		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

private:
	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
};

Apple::Application* Apple::CreateApplication()
{
	APPLE_CLIENT_INFO("Creating Sandbox Application");
	return new Sandbox();
}
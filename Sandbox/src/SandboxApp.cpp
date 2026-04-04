#include <Apple.h>


class Sandbox : public Apple::Application
{
	public:
	Sandbox()
	{
	}
	~Sandbox()
	{
	}
};

Apple::Application* Apple::CreateApplication()
{
	return new Sandbox();
}


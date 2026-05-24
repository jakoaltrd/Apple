#include <iostream>
#include <GLFW/glfw3.h>

int main()
{
    std::cout << "Simple GLFW Test" << std::endl;

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    std::cout << "GLFW initialized successfully" << std::endl;

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test Window", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "Window created successfully" << std::endl;
    glfwMakeContextCurrent(window);

    std::cout << "Starting main loop..." << std::endl;
    int frameCount = 0;

    while (!glfwWindowShouldClose(window) && frameCount < 100)
    {
        glfwPollEvents();
        glfwSwapBuffers(window);

        if (frameCount % 10 == 0)
        {
            std::cout << "Frame: " << frameCount << std::endl;
        }
        frameCount++;
    }

    std::cout << "Closing window after " << frameCount << " frames" << std::endl;

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
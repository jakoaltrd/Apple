#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <iostream>

int main()
{
    std::cout << "Starting GLFW window test..." << std::endl;

    if (!glfwInit())
    {
        std::cerr << "GLFW initialization failed!" << std::endl;
        return -1;
    }
    std::cout << "GLFW initialized successfully" << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test Window", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "Window created successfully" << std::endl;

    glfwMakeContextCurrent(window);
    glfwSetWindowPos(window, 100, 100);

    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return -1;
    }
    std::cout << "GLAD initialized successfully" << std::endl;

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Starting main loop..." << std::endl;

    glClearColor(0.2f, 0.3f, 0.8f, 1.0f);

    int frameCount = 0;
    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glfwPollEvents();
        glfwSwapBuffers(window);

        if (frameCount % 60 == 0)
        {
            std::cout << "Frame: " << frameCount << std::endl;
        }
        frameCount++;
    }

    std::cout << "Window closing..." << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
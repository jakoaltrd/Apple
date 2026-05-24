workspace "Apple"
    configurations { "Debug", "Release" }
    platforms { "x64" }
    startproject "Sandbox"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "GLFW"
    location "Apple/vendor/glfw"
    kind "StaticLib"
    language "C"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "Apple/vendor/glfw/src/**.c",
        "Apple/vendor/glfw/src/**.m",
        "Apple/vendor/glfw/include/**.h"
    }

    includedirs {
        "Apple/vendor/glfw/include",
        "Apple/vendor/glfw/deps"
    }

    filter "system:windows"
        systemversion "latest"
        defines {
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "system:linux"
        defines {
            "_GLFW_X11"
        }

    filter "system:macosx"
        defines {
            "_GLFW_COCOA"
        }

project "ImGui"
    location "Apple/vendor/imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "Apple/vendor/imgui/*.cpp",
        "Apple/vendor/imgui/*.h",
        "Apple/vendor/imgui/backends/imgui_impl_glfw.cpp",
        "Apple/vendor/imgui/backends/imgui_impl_glfw.h",
        "Apple/vendor/imgui/backends/imgui_impl_opengl3.cpp",
        "Apple/vendor/imgui/backends/imgui_impl_opengl3.h"
    }

    includedirs {
        "Apple/vendor/imgui",
        "Apple/vendor/glfw/include"
    }

    defines {
        "IMGUI_IMPL_OPENGL_LOADER_GLAD"
    }

    filter "system:windows"
        systemversion "latest"
        defines {
            "_CRT_SECURE_NO_WARNINGS"
        }

project "Apple"
    location "Apple"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "Apple/src/**.h",
        "Apple/src/**.cpp",
        "Apple/vendor/spdlog/include/**.h",
        "Apple/vendor/spdlog/include/**.hpp"
    }

    includedirs {
        "Apple/src",
        "Apple/vendor/spdlog/include",
        "Apple/vendor/glfw/include",
        "Apple/vendor/glfw/deps",
        "Apple/vendor/imgui"
    }

    links {
        "GLFW",
        "ImGui"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "APPLE_PLATFORM_WINDOWS", "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Debug"
        defines { "APPLE_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "APPLE_RELEASE" }
        optimize "On"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "Sandbox/src/**.h",
        "Sandbox/src/**.cpp"
    }

    includedirs {
        "Apple/src",
        "Apple/vendor/spdlog/include",
        "Apple/vendor/glfw/include",
        "Apple/vendor/glfw/deps",
        "Apple/vendor/imgui"
    }

    links {
        "Apple",
        "GLFW",
        "ImGui"
    }

    filter "system:windows"
        systemversion "latest"
        defines {"APPLE_PLATFORM_WINDOWS", "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Debug"
        defines { "SANDBOX_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "SANDBOX_RELEASE" }
        optimize "On"

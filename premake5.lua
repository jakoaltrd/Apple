workspace "Apple"
    configurations { "Debug", "Release" }
    platforms { "x64" }
    startproject "Sandbox"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Apple"
    location "Apple"
    kind "SharedLib"
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
        "Apple/vendor/spdlog/include"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "APPLE_PLATFORM_WINDOWS","APPLE_BUILD_DLL","_WINDLL", "_CRT_SECURE_NO_WARNINGS" }

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
        "Apple/vendor/spdlog/include"
    }

    links { "Apple" }

    filter "system:windows"
        systemversion "latest"
        defines {"APPLE_PLATFORM_WINDOWS", "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Debug"
        defines { "SANDBOX_DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "SANDBOX_RELEASE" }
        optimize "On"

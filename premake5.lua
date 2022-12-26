EnableDebugging = true

newaction {
    trigger = "clean",
    description = "Cleans all build products",
    execute = function()
        os.rmdir("./build")
        os.remove("Makefile")
    end
}

workspace "dashcam"
    configurations "base"
    architecture "ARM64"

project "dashcam"
    targetname "dashcam"
    kind "ConsoleApp"

    location "build"
    basedir "../"
    objdir "build/intermediate"
    targetdir "build/bin"

    language "C++"
    cppdialect "C++17"

    flags { "MultiProcessorCompile", "NoPCH" }
    clr "Off"
    rtti "Off"
    characterset "Unicode"
    staticruntime "On"
    warnings "Default"

    if EnableDebugging then
        exceptionhandling "On"
        optimize "Off"
        symbols "On"
        defines { "DEBUG", "_DEBUG" }
        buildoptions "-fsanitize=address"
        linkoptions { "-fsanitize=address", "-static-libasan" }
    else
        exceptionhandling "Off"
        optimize "Speed"
        symbols "Off"
        omitframepointer "On"
        defines { "NDEBUG" }
    end

    files { "src/**.cpp", "src/**.h" }

    if _ACTION ~= "clean" then
        os.execute("sudo ./scripts/buildFFmpeg.sh")
    end

    includedirs { "src", "build/ffmpeg/build/include" }
    libdirs { "build/ffmpeg/build/lib" }

    links { "atomic", "avdevice", "avfilter", "postproc", "avformat", "avcodec", "rt", "dl", "xcb", "z", "swresample", "swscale", "avutil", "m", "x264", "pthread" }

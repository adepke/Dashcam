newoption {
    trigger = "debugging",
    value = "value",
    default = "false",
    description = "Compiles a debug build"
}

newaction {
    trigger = "clean",
    description = "Cleans all build products",
    execute = function()
        os.execute("sudo rm /usr/local/bin/ffmpeg /usr/local/bin/ffprobe")
        os.execute("rm -rf ./build")
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

    filter { "options:debugging=true" }
        exceptionhandling "On"
        optimize "Off"
        symbols "On"
        defines { "DEBUG", "_DEBUG" }
        buildoptions "-fsanitize=address"
        linkoptions { "-fsanitize=address", "-static-libasan" }
    filter {}
    
    filter { "options:debugging=false" }
        exceptionhandling "On"
        optimize "Speed"
        symbols "Off"
        omitframepointer "On"
        defines { "NDEBUG" }
    filter {}

    files { "src/**.cpp", "src/**.h" }

    if _ACTION ~= "clean" then
        os.execute("./scripts/buildFFmpeg.bash")
    end

    includedirs { "src", "thirdparty/*", "build/ffmpeg/build/include" }
    libdirs {
        "build/ffmpeg/build/lib"
    }

    links {
        "asan",
        "atomic",
        "avdevice",
        "avfilter",
        "postproc",
        "avformat",
        "avcodec",
        "rt",
        "dl",
        "z",
        "swresample",
        "swscale",
        "avutil",
        "m",
        "x264",
        "pthread",
        "ssl",
        "crypto"
    }

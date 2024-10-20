newoption {
    trigger = "debug",
    value = "bool",
    default = "false",
    description = "Compiles a debug build"
}

newoption {
    trigger = "profile",
    value = "bool",
    default = "false",
    description = "Compiles with Tracy profiling metrics"
}

newoption {
    trigger = "defer-filtering",
    value = "bool",
    default = "false",
    description = "Defer filtering, primarily pixel format conversions, until the media is ready to be converted and uploaded. This can improve performance since filtering is not happening in hardware, but might not be possible to do in FFmpeg!"
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

    filter { "options:debug=true" }
        exceptionhandling "On"
        optimize "Off"
        symbols "On"
        defines { "DEBUG", "_DEBUG" }
        buildoptions "-fsanitize=address"
        linkoptions { "-fsanitize=address", "-static-libasan" }
    filter {}

    filter { "options:debug=false" }
        exceptionhandling "On"
        optimize "Speed"
        symbols "Off"
        omitframepointer "On"
        defines { "NDEBUG" }
    filter {}

    filter { "options:defer-filtering=true" }
        defines { "DEFERRED_FILTERING=1" }
    filter {}

    filter { "options:defer-filtering=false" }
        defines { "DEFERRED_FILTERING=0" }
    filter {}

    files { "src/**.cpp", "src/**.h" }

    -- Enable Tracy client during profiling builds
    filter { "options:profile=true" }
        defines { "TRACY_ENABLE" }
    filter {}

    files { "thirdparty/tracy/public/TracyClient.cpp" }

    if _ACTION ~= "clean" then
        os.execute("./scripts/buildFFmpeg.bash")
    end

    includedirs { "src", "build/ffmpeg/build/include", "thirdparty/tracy/public" }

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

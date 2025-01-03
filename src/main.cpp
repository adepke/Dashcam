#include "run.h"
#include "video.h"
#include "upload.h"
#include "status.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavcodec/version.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/timestamp.h>
    #include <libavutil/opt.h>
    #include <libavdevice/avdevice.h>
}

#include <tracy/Tracy.hpp>

using namespace std::literals::chrono_literals;

bool operatorConnected() {
    ZoneScoped;

    // Check if we have a DNS server from being connected to an operator.
    bool hasConnection = false;
    struct addrinfo* info;

    if (getaddrinfo("www.google.com", "http", nullptr, &info) == 0) {
        hasConnection = true;
        // getaddrinfo() doesn't seem to allocate on failure.
        freeaddrinfo(info);
    }

    return hasConnection;
}

int main(int argc, char** argv) {
    int frameRate = 30;
    bool debug = false;

    int c;
    while ((c = getopt(argc, argv, "r:d")) != -1) {
        switch (c) {
            case 'r':
                frameRate = std::stoi(optarg);
                break;
            case 'd':
                debug = true;
                break;
            case '?':
                if (optopt == 'r') {
                    std::cerr << "Option '" << optopt << "' requires an argument!\n";
                    return 1;
                } else {
                    std::cerr << "Unrecognized option '" << optopt << "'!\n";
                    return 1;
                }
            default:
                std::cerr << "Argument parsing error! c='" << c << "'\n";
                return 1;
        }
    }

    if (frameRate < 1 || frameRate > 60) {
        std::cerr << "Invalid framerate specified.\n";
        return 1;
    }

    if (!initializeStatus()) {
        std::cerr << "Watchdog disabled!\n";

        shutdownStatus();
    } else {
        setState(DashcamState::STARTING);
    }

    // Skip upload in debug mode.
    if (!debug) {
        // DNS takes some time to resolve, this seems like a decent balance.
        std::cout << "Waiting 5 seconds before testing for operator...\n";
        std::this_thread::sleep_for(5s);

        if (operatorConnected()) {
            std::cout << "Operator connected, starting upload mode.\n";

            return uploadMedia();
        } else {
            std::cout <<"No operator connected, starting record mode.\n";
        }
    } else {
        std::cout << "Debug mode enabled.\n";
    }

    avdevice_register_all();

    AVFormatContext* input;

    if (!setupInput(&input, frameRate)) {
        std::cerr << "Failed to create input.\n";
        return 1;
    }

    // Create the data directory if if doesn't exist.
    mkdir("data", S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    auto error = run(input, frameRate);

    return error;
}

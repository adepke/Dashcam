#include "run.h"
#include "video.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <sys/stat.h>
#include <string>

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

using namespace std::literals::chrono_literals;

int main(int argc, char** argv) {
	int frameRate = 30;

	if (argc == 2) {
		frameRate = std::stoi(argv[1]);
	}

	if (frameRate < 1 || frameRate > 60) {
		std::cerr << "Invalid framerate specified.\n";
		return 1;
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

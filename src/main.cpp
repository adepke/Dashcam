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

#include "run.h"

using namespace std::literals::chrono_literals;

int setupContext(int frameRate, AVFormatContext** input, AVCodecContext** decoder, AVCodecContext** encoder) {
	*input = nullptr;
	*decoder = nullptr;
	*encoder = nullptr;

	avdevice_register_all();

	auto deviceName = "/dev/video0";
	auto* inputFormat = av_find_input_format("v4l2");
	AVDictionary* options = nullptr;
	// Device configurations: $ v4l2-ctl --device=/dev/video0 --list-formats-ext
	av_dict_set(&options, "input_format", "mjpeg", 0);
	av_dict_set(&options, "video_size", "1920x1080", 0);
	av_dict_set(&options, "framerate", std::to_string(frameRate).c_str(), 0);

	AVFormatContext* inputContext = nullptr;
	if (avformat_open_input(&inputContext, deviceName, inputFormat, &options) != 0) {
		std::cout << "Failed to open input device.\n";
		return 1;
	}

	if (avformat_find_stream_info(inputContext, nullptr) < 0) {
		std::cout << "Failed to find stream info for input context.\n";
		return 1;
	}

	// Find the video stream.
	int streamId = -1;
	int streamCount = 0;
	for (int i = 0; i < inputContext->nb_streams; ++i) {
		if (inputContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			if (streamId < 0) {
				streamId = i;
			}
			streamCount++;
		}
	}

	if (streamId < 0) {
		std::cout << "Failed to find video stream in input device.\n";
		return 1;
	} else {
		std::cout << "Found " << streamCount << " suitable streams, choosing stream " << streamId << ".\n";
	}

	// Decoding

	auto decCodec = avcodec_find_decoder(inputContext->streams[streamId]->codecpar->codec_id);
	if (!decCodec) {
		std::cout << "failed to find a suitable decoder.\n";
		return 1;
	}

	std::cout << "Found decoder " << decCodec->long_name << "\n";

	auto decContext = avcodec_alloc_context3(decCodec);
	if (!decContext) {
		std::cout << "Failed to allocate decoder context.\n";
		return 1;
	}

	if (decContext->codec_id == AV_CODEC_ID_RAWVIDEO) {
		decContext->pix_fmt = AV_PIX_FMT_YUYV422;  // $ v4l2-ctl --all
	}

	if (avcodec_open2(decContext, decCodec, nullptr) < 0) {
		std::cout << "Failed to open the decoding codec.";
		return 1;
	}

	std::cout << "Decode HW accel status: " << (decContext->hwaccel ? "ENABLED" : "DISABLED/UNKNOWN") << "\n";

	// Encoding

	// Note: if this changes to MPEG1 or MPEG2, we need to write a special endcode.
	//auto encCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	auto encCodec = avcodec_find_encoder_by_name("h264_v4l2m2m");
	if (!encCodec) {
		std::cout << "failed to find a suitable encoder.\n";
		return 1;
	}

	std::cout << "Found encoder " << encCodec->long_name << "\n";

	auto encContext = avcodec_alloc_context3(encCodec);
	if (!encContext) {
		std::cout << "Failed to allocate encoder context.\n";
		return 1;
	}

	encContext->width = 1920;
	encContext->height = 1080;
	encContext->bit_rate = 250000;
	encContext->time_base = (AVRational){ 1, frameRate };
	encContext->pix_fmt = AV_PIX_FMT_YUV420P;
	av_opt_set(encContext->priv_data, "preset", "superfast", 0);
	av_opt_set(encContext->priv_data, "tune", "fastdecode", 0);

	if (avcodec_open2(encContext, encCodec, nullptr) < 0) {
		std::cout << "Failed to open the encoding codec.";
		return 1;
	}

	std::cout << "Encode HW accel status: " << (encContext->hwaccel ? "ENABLED" : "DISABLED/UNKNOWN") << "\n";

	*input = inputContext;
	*decoder = decContext;
	*encoder = encContext;

	return 0;
}

int main(int argc, char** argv) {
	int frameRate = 30;

	if (argc == 2) {
		frameRate = std::stoi(argv[1]);
	}

	if (frameRate < 1 || frameRate > 60) {
		std::cerr << "Invalid framerate specified.\n";
		return 1;
	}

	AVFormatContext* input;
	AVCodecContext* decoder;
	AVCodecContext* encoder;

	if (setupContext(frameRate, &input, &decoder, &encoder) != 0) {
		std::cout << "Failed to setup context.\n";
		return 1;
	}

	// Create the data directory if if doesn't exist.
	mkdir("data", S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	auto error = run(input, decoder, encoder, frameRate);

	return error;
}

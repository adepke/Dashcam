#include "video.h"

#include <iostream>

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

bool setupInput(AVFormatContext** input, int frameRate) {
	auto deviceName = "/dev/video0";
	auto* inputFormat = av_find_input_format("v4l2");
	AVDictionary* options = nullptr;
	// Device configurations: $ v4l2-ctl --device=/dev/video0 --list-formats-ext
	av_dict_set(&options, "input_format", "mjpeg", 0);
	av_dict_set(&options, "video_size", "1920x1080", 0);
	av_dict_set(&options, "framerate", std::to_string(frameRate).c_str(), 0);

	*input = nullptr;
	if (avformat_open_input(input, deviceName, inputFormat, &options) != 0) {
		std::cerr << "Failed to open input device.\n";
		return false;
	}

	if (avformat_find_stream_info(*input, nullptr) < 0) {
		std::cerr << "Failed to find stream info for input context.\n";
		return false;
	}

	return true;
}

bool setupDecoder(AVCodecContext** decoder, AVFormatContext* inputContext) {
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
		std::cerr << "Failed to find video stream in input device.\n";
		return false;
	} else {
		std::cout << "Found " << streamCount << " suitable streams, choosing stream " << streamId << ".\n";
	}

	auto decCodec = avcodec_find_decoder(inputContext->streams[streamId]->codecpar->codec_id);
	if (!decCodec) {
		std::cerr << "Failed to find a suitable decoder.\n";
		return false;
	}

	std::cout << "Found decoder " << decCodec->long_name << "\n";

	auto dec = avcodec_alloc_context3(decCodec);
	if (!dec) {
		std::cerr << "Failed to allocate decoder context.\n";
		return false;
	}

	if (dec->codec_id == AV_CODEC_ID_RAWVIDEO) {
		dec->pix_fmt = AV_PIX_FMT_YUYV422;  // $ v4l2-ctl --all
	}

	if (avcodec_open2(dec, decCodec, nullptr) < 0) {
		std::cerr << "Failed to open the decoding codec.\n";
		return false;
	}

	std::cout << "Decode HW accel status: " << (dec->hwaccel ? "ENABLED" : "DISABLED/UNKNOWN") << "\n";

	*decoder = dec;

	return true;
}

bool setupEncoder(AVCodecContext** encoder, int frameRate) {
	// Note: if this changes to MPEG1 or MPEG2, we need to write a special endcode.
	//auto encCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	auto encCodec = avcodec_find_encoder_by_name("h264_v4l2m2m");
	if (!encCodec) {
		std::cerr << "Failed to find a suitable encoder.\n";
		return false;
	}

	std::cout << "Found encoder " << encCodec->long_name << "\n";

	auto enc = avcodec_alloc_context3(encCodec);
	if (!enc) {
		std::cerr << "Failed to allocate encoder context.\n";
		return false;
	}

	enc->width = 1920;
	enc->height = 1080;
	enc->bit_rate = 350000;
	enc->time_base = (AVRational){ 1, frameRate };
	enc->framerate = (AVRational){ frameRate, 1 };
	enc->pix_fmt = AV_PIX_FMT_YUV420P;
	enc->gop_size = 10;  // https://github.com/FFmpeg/FFmpeg/blob/3d5edb89e75fe3ab3a6757208ef121fa2b0f54c7/doc/examples/encode_video.c#L119
	enc->max_b_frames = 1;  // See above
	av_opt_set(enc->priv_data, "preset", "veryfast", 0);  // https://trac.ffmpeg.org/wiki/Encode/H.264#Preset
	av_opt_set(enc->priv_data, "tune", "zerolatency", 0);  // https://trac.ffmpeg.org/wiki/Encode/H.264#Tune

	if (avcodec_open2(enc, encCodec, nullptr) < 0) {
		std::cerr << "Failed to open the encoding codec.";
		return false;
	}

	std::cout << "Encode HW accel status: " << (enc->hwaccel ? "ENABLED" : "DISABLED/UNKNOWN") << "\n";

	*encoder = enc;

	return true;
}

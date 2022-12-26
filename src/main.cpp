#include <iostream>
#include <fstream>
#include <chrono>
#include <stdio.h>

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

int frames = 0;
std::chrono::time_point<std::chrono::high_resolution_clock> lastDecodeTime;

size_t sumDecodeTime = 0;
size_t sumEncodeTime = 0;

void decode(AVCodecContext* decCtx, AVCodecContext* encCtx, AVFrame* frame, AVPacket* pkt, FILE* outFile) {
	auto ret = avcodec_send_packet(decCtx, pkt);
	if (ret < 0) {
		std::cout << "Failed to send packet to context.\n";
		//exit(1);
		return;
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(decCtx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return;
		} else if (ret < 0) {
			std::cout << "Decoding error.\n";
			exit(1);
		}

		++frames;

		auto now = std::chrono::high_resolution_clock::now();
		auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now - lastDecodeTime).count();
		//std::cout << "DECODED IN " << micros << "us.\n";
		lastDecodeTime = now;
		sumDecodeTime += micros;

		// BEGIN ENCODING

		auto start = std::chrono::high_resolution_clock::now();

		ret = avcodec_send_frame(encCtx, frame);
		if (ret < 0) {
			std::cout << "Failed to encode frame " << frames-1 << "\n";
			return;
		}

		while (ret >= 0) {
			ret = avcodec_receive_packet(encCtx, pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				std::cout << "Encoding error.\n";
				exit(1);
			}

			fwrite(pkt->data, 1, pkt->size, outFile);
			av_packet_unref(pkt);
		}

		auto end = std::chrono::high_resolution_clock::now();
		micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		sumEncodeTime += micros;
	}
}

int main() {
	avdevice_register_all();

	auto deviceName = "/dev/video0";
	auto* inputFormat = av_find_input_format("v4l2");
	AVDictionary* options = nullptr;
	// Device configurations: $ v4l2-ctl --device=/dev/video0 --list-formats-ext
	av_dict_set(&options, "input_format", "mjpeg", 0);
	av_dict_set(&options, "video_size", "1920x1080", 0);
	av_dict_set(&options, "framerate", "30", 0);

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

	FILE* outFile = fopen("data/encoded.h264", "wb");
	if (!outFile) {
		std::cout << "Failed to create output video.\n";
		return 1;
	}

	// DECODING

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

	// ENCODING

	auto encCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
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
	encContext->time_base = (AVRational){ 1, 30 };
	encContext->pix_fmt = AV_PIX_FMT_YUV420P;
	av_opt_set(encContext->priv_data, "preset", "superfast", 0);
	av_opt_set(encContext->priv_data, "tune", "fastdecode", 0);

	if (avcodec_open2(encContext, encCodec, nullptr) < 0) {
		std::cout << "Failed to open the encoding codec.";
		return 1;
	}

	std::cout << "Encode HW accel status: " << (encContext->hwaccel ? "ENABLED" : "DISABLED/UNKNOWN") << "\n";

	//

	auto frame = av_frame_alloc();
	if (!frame) {
		std::cout << "Failed to allocate video frame.\n";
		return 1;
	}

	AVPacket* packet = av_packet_alloc();
	while (av_read_frame(inputContext, packet) >= 0) {
		lastDecodeTime = std::chrono::high_resolution_clock::now();
		decode(decContext, encContext, frame, packet, outFile);
		av_packet_unref(packet);
	}

	decode(decContext, encContext, frame, nullptr, outFile);  // Flush the decoder.
	avcodec_send_frame(encContext, nullptr);  // Flush the encoder.

	if (encCodec->id == AV_CODEC_ID_MPEG1VIDEO || encCodec->id == AV_CODEC_ID_MPEG2VIDEO) {
		uint8_t endcode[] = { 0, 0, 1, 0xb7 };
		fwrite(endcode, 1, sizeof(endcode), outFile);
	}

	fclose(outFile);
	avformat_close_input(&inputContext);

	avcodec_free_context(&decContext);
	avcodec_free_context(&encContext);
	//av_packet_free(&packet);
	av_frame_free(&frame);

	std::cout << "\nFRAMES: " << frames << "\n";
	std::cout << "AVG DECODE TIME: " << ((double)sumDecodeTime / (double)frames / 1000.0) << "ms\n";
	std::cout << "AVG ENCODE TIME: " << ((double)sumEncodeTime / (double)frames / 1000.0) << "ms\n";

	return 0;
}
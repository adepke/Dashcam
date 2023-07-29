#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <sys/stat.h>

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

#include "storage.h"

void processFrame(AVCodecContext* decCtx, AVCodecContext* encCtx, AVFrame* frame, AVPacket* pkt, FILE* outFile) {
	auto ret = avcodec_send_packet(decCtx, pkt);
	if (ret < 0) {
		std::cerr << "Failed to send packet to context.\n";
		return;  // Maybe not an error?
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(decCtx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			return;
		} else if (ret < 0) {
			std::cerr << "Decoding error.\n";
			exit(1);  // Not recoverable.
		}

		ret = avcodec_send_frame(encCtx, frame);
		if (ret < 0) {
			std::cerr << "Failed to encode frame.\n";
			return;  // Recoverable, will just skip this frame.
		}

		while (ret >= 0) {
			ret = avcodec_receive_packet(encCtx, pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			} else if (ret < 0) {
				std::cerr << "Encoding error.\n";
				exit(1);  // Potentially recoverable?
			}

			fwrite(pkt->data, 1, pkt->size, outFile);
			av_packet_unref(pkt);
		}
	}
}

int run(AVFormatContext* inputContext, AVCodecContext* decContext, AVCodecContext* encContext, int frameRate) {
	size_t spaceRemaining = 0;
	auto* outFile = getStorage(nullptr, spaceRemaining);
	if (!outFile) {
		return 1;
	}

	auto frame = av_frame_alloc();
	if (!frame) {
		std::cerr << "Failed to allocate video frame.\n";
		return 1;
	}

	AVPacket* packet = av_packet_alloc();
	auto lastFrame = std::chrono::high_resolution_clock::now();
	while (av_read_frame(inputContext, packet) >= 0) {
		if (spaceRemaining == 0) {
			outFile = getStorage(outFile, spaceRemaining);
			if (!outFile) {
				return 1;
			}
		}

		processFrame(decContext, encContext, frame, packet, outFile);
		av_packet_unref(packet);

		const auto now = std::chrono::high_resolution_clock::now();
		const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrame).count();
		const auto targetUs = 1.0 * 1000.0 * 1000.0 / (double)frameRate;
		const int waitUs = targetUs - elapsedUs;
		if (waitUs > 0) {
			std::this_thread::sleep_for(std::chrono::microseconds{ waitUs });
		} else {
			// Send to stdout since we don't want to spam logs with this.
			std::cout << "Falling behind! (" << waitUs * -1.0 / 1000.0 << "ms late)\n";
		}
		lastFrame = now;
	}

	processFrame(decContext, encContext, frame, nullptr, outFile);  // Flush the decoder.
	avcodec_send_frame(encContext, nullptr);  // Flush the encoder.

	/*
	if (encCodec->id == AV_CODEC_ID_MPEG1VIDEO || encCodec->id == AV_CODEC_ID_MPEG2VIDEO) {
		const uint8_t endcode[] = { 0, 0, 1, 0xb7 };
		fwrite(endcode, 1, sizeof(endcode), outFile);
	}
	*/

	fclose(outFile);
	avformat_close_input(&inputContext);

	avcodec_free_context(&decContext);
	avcodec_free_context(&encContext);
	//av_packet_free(&packet);
	av_frame_free(&frame);

	return 0;
}

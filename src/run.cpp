#include "run.h"
#include "storage.h"
#include "video.h"

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

size_t processFrame(AVCodecContext* decCtx, AVCodecContext* encCtx, AVFrame* frame, AVPacket* pkt, FILE* outFile) {
    auto ret = avcodec_send_packet(decCtx, pkt);
    if (ret < 0) {
        std::cerr << "Failed to send packet to context.\n";
        return 0;  // Maybe not an error?
    }

    size_t bytesWritten = 0;

    while (ret >= 0) {
        ret = avcodec_receive_frame(decCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return bytesWritten;
        } else if (ret < 0) {
            std::cerr << "Decoding error.\n";
            exit(1);  // Not recoverable.
        }

        ret = avcodec_send_frame(encCtx, frame);
        if (ret < 0) {
            std::cerr << "Failed to encode frame.\n";
            return bytesWritten;  // Recoverable, will just skip this frame.
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
            bytesWritten += pkt->size;
            av_packet_unref(pkt);
        }
    }

    return bytesWritten;
}

int run(AVFormatContext* inputContext, int frameRate) {
    setState(DashcamState::RECORDING);

    AVCodecContext* decContext;
    AVCodecContext* encContext;

    if (!setupDecoder(&decContext, inputContext)) {
        std::cerr << "Failed to setup decoder.\n";
        return 1;
    }

    if (!setupEncoder(&encContext, frameRate)) {
        std::cerr << "Failed to setup encoder.\n";
        return 1;
    }

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

            // Draining and flushing doesn't seem to work, just build a new encoder.
            //avcodec_send_frame(encContext, nullptr);
            //avcodec_flush_buffers(encContext);

            avcodec_send_frame(encContext, nullptr);  // Flush the encoder.
            avcodec_free_context(&encContext);

            if (!setupEncoder(&encContext, frameRate)) {
                std::cerr << "Failed to re-setup encoder.\n";
                return 1;
            }
        }

        auto bytes = processFrame(decContext, encContext, frame, packet, outFile);
        spaceRemaining -= std::min(bytes, spaceRemaining);
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

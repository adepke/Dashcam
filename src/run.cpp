#include "run.h"
#include "storage.h"
#include "video.h"
#include "status.h"
#include "channel.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <list>
#include <atomic>
#include <thread>
#include <cassert>
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
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
}

#include <tracy/Tracy.hpp>

uint32_t zoneColors[] = {
    0xE81416,
    0xFFA500,
    0xFAEB36,
    0x79C314,
    0x487DE7,
    0x4B369D,
    0x70369D
};

void inputWorker(const VideoContext& context, std::atomic<bool>& flag, Channel<AVPacket*>& output) {
    size_t job = 0;  // Debug variable for tracking pipelining.

    const auto targetUs = 1.0 * 1000.0 * 1000.0 / (double)context.frameRate;
    auto lastFrame = std::chrono::high_resolution_clock::now();

    while (flag.load(std::memory_order_relaxed)) {
        ZoneScopedN("input_job");
        ZoneColor(zoneColors[job++ % (sizeof(zoneColors) / sizeof(*zoneColors))]);

        auto* packet = av_packet_alloc();

        {
            ZoneScopedN("input_drain");

            if (auto ret = av_read_frame(context.inputCtx, packet); ret < 0) {
                std::cerr << "Failed to read packet from input source.\n";
                exit(1);  // #TODO: proper error handling and cleanup.
            }
        }

        output.push(packet);

        const auto now = std::chrono::high_resolution_clock::now();
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrame).count();

        const int waitUs = targetUs - elapsedUs;
        if (waitUs > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds{ waitUs });
        } else {
            // Send to stdout since we don't want to spam logs with this.
            std::cout << "Falling behind! (" << waitUs * -1.0 / 1000.0 << "ms late)\n";
        }
        lastFrame = now;
    }

    // Drain the pipeline.
    output.push(nullptr);
}

void decodeWorker(const VideoContext& context, Channel<AVPacket*>& input, Channel<AVFrame*>& output) {
    size_t job = 0;  // Debug variable for tracking pipelining.

    while (true) {
        ZoneScopedN("decode_job");
        ZoneColor(zoneColors[job++ % (sizeof(zoneColors) / sizeof(*zoneColors))]);

        auto* packet = input.pop();
        if (!packet) {
            break;
        }

        int ret;
        {
            ZoneScopedN("decoder_fill");

            if (ret = avcodec_send_packet(context.decodeCtx, packet); ret < 0) {
                std::cerr << "Failed to send packet to context.\n";
                exit(1);  // Maybe not an error? #TODO: proper error handling and cleanup.
            }
        }

        // Cleanup
        // #TEMP TESTING
        //av_packet_free(&packet);

        while (ret >= 0) {
            auto* frame = av_frame_alloc();

            {
                ZoneScopedN("decoder_drain");
                ret = avcodec_receive_frame(context.decodeCtx, frame);
            }

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // Finished the job, return to the parent loop.
                break;
            } else if (ret < 0) {
                std::cerr << "Decoding error.\n";
                exit(1);  // Not recoverable. #TODO: proper error handling and cleanup.
            }

            output.push(frame);
        }
    }

    // Drain the pipeline.
    output.push(nullptr);
}

void filterWorker(const VideoContext& context, Channel<AVFrame*>& input, Channel<AVFrame*>& output) {
    size_t job = 0;  // Debug variable for tracking pipelining.

    while (true) {
        ZoneScopedN("filter_job");
        ZoneColor(zoneColors[job++ % (sizeof(zoneColors) / sizeof(*zoneColors))]);

        auto* preFilter = input.pop();
        if (!preFilter) {
            break;
        }

#if DEFERRED_FILTERING
        // Do no work, just pass the frame through.
        output.push(preFilter);
#else
        int ret;
        {
            ZoneScopedN("filter_graph_fill");

            if (ret = av_buffersrc_add_frame_flags(context.filterSourceCtx, preFilter, AV_BUFFERSRC_FLAG_KEEP_REF); ret < 0) {
                std::cerr << "Failed to feed frame into filter graph.\n";
                exit(1);  // #TODO: proper error handling and cleanup.
            }
        }

        // Cleanup
        av_frame_unref(preFilter);

        // Retrieve the frame from the filter graph output and push it through the encoder.
        while (ret >= 0) {
            auto* postFilter = av_frame_alloc();

            {
                ZoneScopedN("filter_graph_drain");
                ret = av_buffersink_get_frame(context.filterSinkCtx, postFilter);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // Finished the job, return to the parent loop.
                break;
            } else if (ret < 0) {
                std::cerr << "Buffer sink error.\n";
                exit(1);  // Not recoverable. #TODO: proper error handling and cleanup.
            }

            output.push(postFilter);
        }
#endif
    }

    // Drain the pipeline.
    output.push(nullptr);
}

void encodeWorker(const VideoContext& context, Channel<AVFrame*>& input, Channel<AVPacket*>& output) {
    size_t job = 0;  // Debug variable for tracking pipelining.

    while (true) {
        ZoneScopedN("encode_job");
        ZoneColor(zoneColors[job++ % (sizeof(zoneColors) / sizeof(*zoneColors))]);

        auto* frame = input.pop();
        if (!frame) {
            break;
        }

        int ret;
        {
            ZoneScopedN("encoder_fill");
            ret = avcodec_send_frame(context.encodeCtx, frame);
        }
        if (ret == AVERROR(EAGAIN)) {
            // Encoder is not ready to accept new frames, this is not an ideal situation. Consider reducing the pipelining.
            // We need to try and process this frame again

            while (avcodec_send_frame(context.encodeCtx, frame) == AVERROR(EAGAIN)) {
                std::cerr << "Retried encoder fill, this could be dangerous.\n";
            }
        } else if ( ret < 0) {
            char buffer[256];
            std::cerr << "Failed to encode frame: error: " << av_make_error_string(buffer, sizeof(buffer), ret) << "\n";
            continue;  // Recoverable, will just skip this frame. #TODO: look at this again
        }

        // Cleanup
        av_frame_unref(frame);

        while (ret >= 0) {
            auto* packet = av_packet_alloc();

            {
                ZoneScopedN("encoder_drain");
                ret = avcodec_receive_packet(context.encodeCtx, packet);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // Finished the job, return to the parent loop.
                break;
            } else if (ret < 0) {
                std::cerr << "Encoding error.\n";
                exit(1);  // Potentially recoverable? #TODO: proper error handling and cleanup.
            }

            output.push(packet);
        }
    }

    // Drain the pipeline.
    output.push(nullptr);
}

void outputWorker(const VideoContext& context, Channel<AVPacket*>& input, Channel<Storage>& reset) {
    size_t job = 0;  // Debug variable for tracking pipelining.

    Storage storage;
    size_t spaceRemaining = 0;
    bool firstJob = false;
    bool draining = false;

    // Try and retrieve old storage from a reset. If there's nothing here, then this must be the first job.
    auto lastStorage = reset.tryPop();
    if (lastStorage) {
        storage = *lastStorage;
        spaceRemaining = storage.space;  // We know that the old storage was not used yet, so the full space is usable.
    } else {
        firstJob = true;
    }

    while (true) {
        ZoneScopedN("output_job");
        ZoneColor(zoneColors[job++ % (sizeof(zoneColors) / sizeof(*zoneColors))]);

        auto* packet = input.pop();
        if (!packet) {
            break;
        }

        // When draining the pipeline, just free the memory and continue. Consider saving these packets in the future?
        if (draining) {
            av_packet_free(&packet);
            continue;
        }

        // Check if the current storage medium has enough space for this packet.
        if (packet->size > spaceRemaining) {
            if (storage = getStorage(storage); !storage.file) {
                exit(1);  // #TODO: proper error handling and cleanup.
            }

            spaceRemaining = storage.space;

            // If this is the first output packet, then we don't have to reset state.
            if (firstJob) {
                firstJob = false;
            } else {
                // The storage medium is changing, so notify the main thread we need to reboot.
                reset.push(storage);
                draining = true;
                av_packet_free(&packet);

                continue;
            }
        }

        // Sanity check to ensure the packet can now fit on the storage.
        assert(spaceRemaining >= packet->size);

        {
            ZoneScopedN("write_to_disk");
            fwrite(packet->data, 1, packet->size, storage.file);
        }

        spaceRemaining -= packet->size;

        // Cleanup
        av_packet_free(&packet);
    }
}

int run(AVFormatContext* inputContext, int frameRate) {
    ZoneScoped;

    setState(DashcamState::RECORDING);

    AVCodecContext* decContext;
    AVCodecContext* encContext;
    AVFilterGraph* filterGraph;
    AVFilterContext* bufferSourceContext;
    AVFilterContext* bufferSinkContext;

    if (!setupDecoder(&decContext, inputContext)) {
        std::cerr << "Failed to setup decoder.\n";
        return 1;
    }

    if (!setupEncoder(&encContext, frameRate)) {
        std::cerr << "Failed to setup encoder.\n";
        return 1;
    }

    if (!setupFilterGraph(&filterGraph, &bufferSourceContext, &bufferSinkContext, decContext, encContext)) {
        std::cerr << "Failed to setup filter graph.\n";
        return 1;
    }

    VideoContext videoContext{
        .frameRate = frameRate,
        .inputCtx = inputContext,
        .decodeCtx = decContext,
        .filterSourceCtx = bufferSourceContext,
        .filterSinkCtx = bufferSinkContext,
        .encodeCtx = encContext
    };

    while (true) {
        std::atomic<bool> flag = true;
        std::list<std::thread> workers;

        // Determines how pipelined a single frame can become. A low number can restrict parallelism, but a high number introduces latency.
        constexpr size_t maxPipelining = 2;

        Channel<AVPacket*> inputChannel{ maxPipelining };
        Channel<AVFrame*> decodeChannel{ maxPipelining };
        Channel<AVFrame*> filterChannel{ maxPipelining };
        Channel<AVPacket*> encodeChannel{ maxPipelining };
        Channel<Storage> resetCommunicationChannel{ 1 };  // Channel used to communicate storage resets back to the main thread.

        workers.push_back(std::thread{ inputWorker, std::ref(videoContext), std::ref(flag), std::ref(inputChannel) });
        workers.push_back(std::thread{ decodeWorker, std::ref(videoContext), std::ref(inputChannel), std::ref(decodeChannel) });
        workers.push_back(std::thread{ filterWorker, std::ref(videoContext), std::ref(decodeChannel), std::ref(filterChannel) });
        workers.push_back(std::thread{ encodeWorker, std::ref(videoContext), std::ref(filterChannel), std::ref(encodeChannel) });
        workers.push_back(std::thread{ outputWorker, std::ref(videoContext), std::ref(encodeChannel), std::ref(resetCommunicationChannel) });

        // Wait for the output worker to request a reset. The newly allocated storage is pulled from the channel.
        auto newStorage = resetCommunicationChannel.pop();

        // Notify the input worker to start draining the pipeline. We already lost our storage space, so consider rethinking this solution
        // in the future, as this method will add a short time jump in the media for the lost video.
        flag.store(false);

        // Sync all workers.
        for (auto iter = workers.begin(); iter != workers.end(); ++iter) {
            iter->join();
        }

        // Push the new storage back on the reset channel so that the output worker can retrieve it when it starts up again.
        resetCommunicationChannel.push(newStorage);

        // Draining and flushing doesn't seem to work, just build a new encoder.
        //avcodec_send_frame(encContext, nullptr);
        //avcodec_flush_buffers(encContext);

        avcodec_send_frame(videoContext.encodeCtx, nullptr);  // Flush the encoder.
        avcodec_free_context(&videoContext.encodeCtx);

        if (!setupEncoder(&videoContext.encodeCtx, videoContext.frameRate)) {
            std::cerr << "Failed to re-setup encoder.\n";
            exit(1);  // #TODO: proper error handling and cleanup.
        }
    }

    //processFrame(videoContext, frame, nullptr, outFile);  // Flush the decoder.
    avcodec_send_frame(encContext, nullptr);  // Flush the encoder.

    /*
    if (encCodec->id == AV_CODEC_ID_MPEG1VIDEO || encCodec->id == AV_CODEC_ID_MPEG2VIDEO) {
        const uint8_t endcode[] = { 0, 0, 1, 0xb7 };
        fwrite(endcode, 1, sizeof(endcode), outFile);
    }
    */

    avformat_close_input(&inputContext);
    avfilter_graph_free(&filterGraph);
    avcodec_free_context(&encContext);
    avcodec_free_context(&decContext);

    return 0;
}

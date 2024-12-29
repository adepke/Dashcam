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
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
}

#include <tracy/Tracy.hpp>

bool setupInput(AVFormatContext** input, int frameRate) {
    ZoneScoped;

    auto deviceName = "/dev/video0";

    // Configure the device to be in the correct format. FFmpeg doesn't always configure it properly without this.
    auto v4l2CommandBase = std::string{ "v4l2-ctl --device=" } + deviceName;
    auto v4l2Set = v4l2CommandBase + " --set-fmt-video=width=1920,height=1080,pixelformat=MJPG";  // Use MJPG compression for high framerate and high resolution.
    auto v4l2Get = v4l2CommandBase + " --get-fmt-video";

    system(v4l2Set.c_str());
    system(v4l2Get.c_str());

    auto* inputFormat = av_find_input_format("v4l2");  // Capturing from a v4l2 device
    AVDictionary* options = nullptr;
    // Device configurations: $ v4l2-ctl --device=/dev/video0 --list-formats-ext
    av_dict_set(&options, "input_format", "mjpeg", 0);  // "rawvideo" can be used with this camera instead, but it's far slower as it's uncompressed. 6 FPS max @ 1080p
    //av_dict_set(&options, "pixel_format", "yuyv422", 0);  // Don't need to set the format, let FFMPEG decide.
    av_dict_set(&options, "video_size", "1920x1080", 0);
    // #TEMP: testing, this doesn't impact it
    //av_dict_set(&options, "framerate", std::to_string(frameRate).c_str(), 0);

    *input = nullptr;
    if (auto ret = avformat_open_input(input, deviceName, inputFormat, &options); ret != 0) {
        char buffer[256];
        std::cerr << "Failed to open input device: error: " << av_make_error_string(buffer, sizeof(buffer), ret) << "\n";
        return false;
    }

    if (avformat_find_stream_info(*input, nullptr) < 0) {
        std::cerr << "Failed to find stream info for input context.\n";
        return false;
    }

    return true;
}

bool setupDecoder(AVCodecContext** decoder, AVFormatContext* inputContext) {
    ZoneScoped;

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

    // Codec is either AV_CODEC_ID_RAWVIDEO or AV_CODEC_ID_MJPEG
    // $ v4l2-ctl --all
    if (dec->codec_id == AV_CODEC_ID_RAWVIDEO) {
        dec->pix_fmt = AV_PIX_FMT_YUYV422;  // This may not be correct, need to test.
    } else if (dec->codec_id == AV_CODEC_ID_MJPEG) {
        dec->pix_fmt = AV_PIX_FMT_YUVJ422P;
    } else {
        std::cerr << "Unexpected decoder codec! Codec ID is: " << dec->codec_id << "\n";
        return false;
    }

    std::cout << "Decoder pixel format is: " << dec->pix_fmt << "\n";

    // Try to enable multithreading, if supported by the codec.
    dec->thread_count = 0;

    if (decCodec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
        std::cout << "Decoder multithreading with frame threads.\n";
        dec->thread_type = FF_THREAD_FRAME;
    } else if (decCodec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
        std::cout << "Decoder multithreading with slice threads.\n";
        dec->thread_type = FF_THREAD_SLICE;
    } else {
        std::cout << "Decoder multithreading is unsupported, using a single thread.\n";
        dec->thread_count = 1;  // Force single thread.
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
    ZoneScoped;

    // Note: if this changes to MPEG1 or MPEG2, we need to write a special endcode.
    auto encCodec = avcodec_find_encoder_by_name("h264_v4l2m2m");  // Can use AV_CODEC_ID_H264 for the SW encoder.
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
    enc->bit_rate = 200000000;  // 200kb/s
    enc->compression_level = 0;
    enc->time_base = (AVRational){ 1, frameRate };
    enc->framerate = (AVRational){ frameRate, 1 };
    enc->pix_fmt = AV_PIX_FMT_YUV420P;  // v4l2m2m encoder requires this pixel format, it cannot encode with YUYV422.
    enc->gop_size = 10;  // https://github.com/FFmpeg/FFmpeg/blob/3d5edb89e75fe3ab3a6757208ef121fa2b0f54c7/doc/examples/encode_video.c#L119
    enc->max_b_frames = 1;  // See above
    // TODO: CRF might be unused by v4l2m2m encoder!
    av_opt_set(enc, "crf", "17", 0);  // https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue
    av_opt_set(enc, "preset", "veryfast", 0);  // https://trac.ffmpeg.org/wiki/Encode/H.264#Preset
    av_opt_set(enc, "tune", "zerolatency", 0);  // https://trac.ffmpeg.org/wiki/Encode/H.264#Tune
    av_opt_set(enc, "bufsize", "1000000", 0);

    // #TEMP: testing with pipelined execution
    av_opt_set(enc, "num_capture_buffers", "64", 0);

    if (avcodec_open2(enc, encCodec, nullptr) < 0) {
        std::cerr << "Failed to open the encoding codec.";
        return false;
    }

    std::cout << "Encode HW accel status: " << (enc->hwaccel ? "ENABLED" : "DISABLED/UNKNOWN") << "\n";

    *encoder = enc;

    return true;
}

bool setupFilterGraph(AVFilterGraph** graph, AVFilterContext** filterSource, AVFilterContext** filterSink, AVCodecContext* decoder, AVCodecContext* encoder) {

    ZoneScoped;

#if DEFERRED_FILTERING
    // When deferring filtering, just set the AV points to null and early out.
    *graph = nullptr;
    *filterSource = nullptr;
    *filterSink = nullptr;
#else
    const AVFilter* bufferSource = avfilter_get_by_name("buffer");
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    AVFilterContext* bufferSourceContext;
    AVFilterContext* bufferSinkContext;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = encoder->time_base;

    AVFilterGraph* grph = avfilter_graph_alloc();

    if (!outputs || !inputs || !grph) {
        std::cerr << "Failed to allocate filter graph essential components.\n";
        return false;
    }

    // Source filter: do nothing
    char sourceArgs[512];
    snprintf(sourceArgs, sizeof(sourceArgs), "video_size=1920x1080:pix_fmt=%d:time_base=1/30", decoder->pix_fmt);
    if (avfilter_graph_create_filter(&bufferSourceContext, bufferSource, "in", sourceArgs, nullptr, grph) < 0) {
        std::cerr << "Failed to make source filter.\n";
        return false;
    }

    // Sink filter: change the pixel format
    if (avfilter_graph_create_filter(&bufferSinkContext, bufferSink, "out", nullptr, nullptr, grph) < 0) {
        std::cerr << "Failed to make sink filter\n";
        return false;
    }

    // I'm not sure what the purpose of this code is, maybe to set the pixel format of all node in the graph? It doesn't seem like I need this, maybe because
    // I only have 2 nodes in my graph.
    /*enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    if (av_opt_set_int_list(bufferSinkContext, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
        std::cerr << "Failed setting pix_fmts option in the buffer sink context.\n";
        return false;
    }*/

    outputs->name = av_strdup("in");
    outputs->filter_ctx = bufferSourceContext;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = bufferSinkContext;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    // Modify the format to be of the encoder's expected format.
    char graphDesc[64];
    snprintf(graphDesc, sizeof(graphDesc), "format=%d", encoder->pix_fmt);

    if (avfilter_graph_parse_ptr(grph, graphDesc, &inputs, &outputs, nullptr) < 0) {
        std::cerr << "Failed to parse filter graph string.\n";
        return false;
    }

    if (avfilter_graph_config(grph, nullptr) < 0) {
        std::cerr << "Failed to configure filter graph.\n";
        return false;
    }

    *graph = grph;
    *filterSource = bufferSourceContext;
    *filterSink = bufferSinkContext;
#endif

    return true;
}

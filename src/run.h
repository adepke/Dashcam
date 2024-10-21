#pragma once

struct AVFormatContext;
struct AVCodecContext;
struct AVFilterContext;

struct VideoContext
{
    int frameRate;
    AVFormatContext* inputCtx;
    AVCodecContext* decodeCtx;
    AVFilterContext* filterSourceCtx;
    AVFilterContext* filterSinkCtx;
    AVCodecContext* encodeCtx;
};

int run(AVFormatContext* inputContext, int frameRate);

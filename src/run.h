#pragma once

struct AVFormatContext;
struct AVCodecContext;
struct AVFilterContext;

struct VideoContext
{
    AVCodecContext* decodeCtx;
    AVCodecContext* encodeCtx;
    AVFilterContext* filterSourceCtx;
    AVFilterContext* filterSinkCtx;
};

int run(AVFormatContext* inputContext, int frameRate);

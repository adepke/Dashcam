#pragma once

struct AVFormatContext;
struct AVCodecContext;

int run(AVFormatContext* inputContext, AVCodecContext* decContext, AVCodecContext* encContext, int frameRate);

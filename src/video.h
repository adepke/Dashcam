#pragma once

struct AVFormatContext;
struct AVCodecContext;

bool setupInput(AVFormatContext** input, int frameRate);
bool setupDecoder(AVCodecContext** decoder, AVFormatContext* inputContext);
bool setupEncoder(AVCodecContext** encoder, int frameRate);

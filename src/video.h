#pragma once

struct AVFormatContext;
struct AVCodecContext;
struct AVFilterGraph;
struct AVFilterContext;

bool setupInput(AVFormatContext** input, int frameRate);
bool setupDecoder(AVCodecContext** decoder, AVFormatContext* inputContext);
bool setupEncoder(AVCodecContext** encoder, int frameRate);
bool setupFilterGraph(AVFilterGraph** graph, AVFilterContext** filterSource, AVFilterContext** filterSink, AVCodecContext* decoder, AVCodecContext* encoder);

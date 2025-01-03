#!/usr/bin/python3

import argparse
import os
import sys
import subprocess

def main():
    parser = argparse.ArgumentParser(description="Tool to covert video files to mp4")
    parser.add_argument("-f", "--file", type=str, required=True)
    parser.add_argument("-d", "--destination", type=str, required=False)
    parser.add_argument("-s", "--no-hardware-accel", required=False, action="store_true")
    parsedArgs = parser.parse_args()

    dest = os.path.splitext(os.path.basename(parsedArgs.file))[0] + ".mp4"
    if parsedArgs.destination is not None:
        dest = os.path.join(parsedArgs.destination, dest)

    # Default to the hardware accelerated mem2mem encoder/decoder
    decoder = "h264_v4l2m2m"
    encoder = "h264_v4l2m2m"

    if parsedArgs.no_hardware_accel:
        decoder = "h264"
        encoder = "libx264"

    # Unfortunately, the h264 decoder does not support user-defined pixel format, so we cannot tell it to parse the media as yuvj422p (deferred filtering). Similarly, the v4l2m2m H264 encoder
    # does not support any output pixel format besides yuv420p, so conversion there does not work either. I'm not sure if there's any way to pull this off.
    #ffmpegCmd = f"ffmpeg -y -hide_banner -loglevel error -r 30 -c:v {decoder} -vtag YV12 -pix_fmt yuvj422p -i {parsedArgs.file} -pix_fmt yuv420p -b:v 8M -c:a copy -c:v {encoder} {dest}"
    ffmpegCmd = f"ffmpeg -y -hide_banner -loglevel error -r 30 -c:v {decoder} -vtag YV12 -i {parsedArgs.file} -b:v 8M -c:a copy -c:v {encoder} {dest}"

    cmd = subprocess.Popen(ffmpegCmd.split(" "), stderr=subprocess.PIPE)
    _, stderr = cmd.communicate()

    print(stderr.decode(), end="", file=sys.stderr)

    # Prints the filename output on success.
    if cmd.returncode == 0:
        print(dest)

    return cmd.returncode

if __name__ == "__main__":
    exit(code=main())

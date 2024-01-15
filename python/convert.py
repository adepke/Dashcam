#!/bin/env python3

import argparse
import os
import sys
import subprocess

def main():
    parser = argparse.ArgumentParser(description="Tool to covert video files to mp4")
    parser.add_argument("-f", "--file", type=str, required=True)
    parser.add_argument("-d", "--destination", type=str, required=False)
    parsedArgs = parser.parse_args()

    dest = os.path.splitext(os.path.basename(parsedArgs.file))[0] + ".mp4"
    if parsedArgs.destination is not None:
        dest = os.path.join(parsedArgs.destination, dest)

    ffmpegCmd = f"ffmpeg -y -hide_banner -loglevel error -r 30 -c:v h264_v4l2m2m -i {parsedArgs.file} -b:v 4M -c:a copy -c:v h264_v4l2m2m {dest}"

    cmd = subprocess.Popen(ffmpegCmd.split(" "), stderr=subprocess.PIPE)
    _, stderr = cmd.communicate()

    print(stderr.decode(), end="", file=sys.stderr)

    # Prints the filename output on success.
    if cmd.returncode == 0:
        print(dest)

    return cmd.returncode

if __name__ == "__main__":
    exit(code=main())

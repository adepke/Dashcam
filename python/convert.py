#!/bin/env python3

import argparse
import os
import subprocess

def main():
    parser = argparse.ArgumentParser(description="Tool to covert video files to mp4")
    parser.add_argument("--file", dest="file", required=True)
    parsedArgs = parser.parse_args()

    dest = os.path.splitext(os.path.basename(parsedArgs.file))[0] + ".mp4"
    ffmpegCmd = f"ffmpeg -y -hide_banner -loglevel error -r 30 -i {parsedArgs.file} -c:a copy {dest}"

    cmd = subprocess.Popen(ffmpegCmd.split(" "), stderr=subprocess.PIPE)
    _, stderr = cmd.communicate()

    print(stderr.decode(), end="")

    return cmd.returncode

if __name__ == "__main__":
    exit(code=main())

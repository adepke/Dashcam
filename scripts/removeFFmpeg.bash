#!/usr/bin/bash

set -e
set -u

baseDir=$(dirname $(realpath "$0"))
installDir=$baseDir/../build/ffmpeg

sudo rm -f /usr/local/bin/ffmpeg /usr/local/bin/ffprobe
sudo rm -f /etc/ld.so.conf.d/ffmpeg.conf
sudo ldconfig
sudo rm -rf $installDir

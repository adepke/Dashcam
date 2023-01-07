#!/bin/bash

baseDir=$(dirname $(realpath "$0"))
installDir=$baseDir/../build/ffmpeg
libDir=$installDir/build/lib
numCores=$(lscpu | grep -i "cpu(s):" | tr -dc '0-9')

if [ -d $installDir ]
then
    echo "FFmpeg installed."
    exit 0
fi

# Decoder/encoder options:
# Decoding: libx264 (SW), MMAL (HW)
# Encoding: OpenMAX (HW)
# See: https://trac.ffmpeg.org/wiki/HWAccelIntro

mkdir -p $installDir/sources $installdir/bin && \
chmod a+rw build && \
cd $installDir/sources && \
curl https://www.ffmpeg.org/releases/ffmpeg-5.1.2.tar.bz2 --output ffmpeg.tar.bz2 && \
tar xjf ffmpeg.tar.bz2 && \
cd ffmpeg-5.1.2 && \
PATH="$installDir/bin:$PATH" PKG_CONFIG_PATH="$installDir/build/lib/pkgconfig" ./configure \
  --prefix="$installDir/build" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$installDir/build/include -Ofast -faggressive-loop-optimizations -fomit-frame-pointer" \
  --extra-ldflags="-L$installDir/build/lib" \
  --extra-libs="-lpthread -lm" \
  --ld="g++" \
  --bindir="$installDir/bin" \
  --arch=arm \
  --cpu=arm1176jzf-s \
  --target-os=linux \
  --enable-gpl \
  --enable-nonfree \
  --disable-doc \
  --enable-libx264 \
  --enable-omx \
  --enable-omx-rpi \
  --enable-mmal \
  --enable-neon \
  --enable-hardcoded-tables && \
PATH="$installDir/bin:$PATH" make -j$numCores && \
make -j$numCores install && \
echo "$libDir" | tee -a /etc/ld.so.conf.d/ffmpeg.conf && \
ldconfig

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

mkdir -p $installDir/sources $installdir/bin && \
chmod a+rw build && \
cd $installDir/sources && \
wget -O ffmpeg-5.1.2.tar.bz2 https://ffmpeg.org/releases/ffmpeg-5.1.2.tar.bz2 && \
tar xjf ffmpeg-5.1.2.tar.bz2 && \
cd ffmpeg-5.1.2 && \
PATH="$installDir/bin:$PATH" PKG_CONFIG_PATH="$installDir/build/lib/pkgconfig" ./configure \
  --prefix="$installDir/build" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$installDir/build/include" \
  --extra-ldflags="-L$installDir/build/lib" \
  --extra-libs="-lpthread -lm" \
  --ld="g++" \
  --bindir="$installDir/bin" \
  --enable-gpl \
  --enable-libx264 \
  --disable-hwaccels \
  --disable-vaapi \
  --disable-vdpau && \
PATH="$installDir/bin:$PATH" make -j$numCores && \
make -j$numCores install && \
echo "$libDir" | tee -a /etc/ld.so.conf.d/ffmpeg.conf && \
ldconfig

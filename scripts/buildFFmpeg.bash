#!/bin/bash

set -e

baseDir=$(dirname $(realpath "$0"))
installDir=$baseDir/../build/ffmpeg
libDir=$installDir/build/lib
binDir=$installDir/bin
numCores=$(nproc)

if [[ $(type -P "ffmpeg") ]]
then
	echo "FFmpeg installed."
	exit 0
fi

echo "Building FFmpeg."

# Decoder/encoder options:
# libx264 (SW), v4l2m2m (HW)
# See: https://trac.ffmpeg.org/wiki/HWAccelIntro

mkdir -p $installDir/sources $installdir/bin
cd $installDir/sources
curl https://www.ffmpeg.org/releases/ffmpeg-5.1.2.tar.bz2 --output ffmpeg.tar.bz2
tar -xjf ffmpeg.tar.bz2
cd ffmpeg-5.1.2
PATH="$installDir/bin:$PATH" PKG_CONFIG_PATH="$installDir/build/lib/pkgconfig" ./configure \
	--prefix="$installDir/build" \
	--pkg-config-flags="--static" \
	--extra-cflags="-I$installDir/build/include -Ofast -faggressive-loop-optimizations -fomit-frame-pointer" \
	--extra-ldflags="-L$installDir/build/lib" \
	--extra-libs="-lpthread -lm" \
	--ld="g++" \
	--bindir="$installDir/bin" \
	--arch=aarch64 \
	--target-os=linux \
	--enable-gpl \
	--enable-nonfree \
	--disable-doc \
	--enable-libx264 \
	--enable-neon \
	--enable-hardcoded-tables
PATH="$installDir/bin:$PATH" make -j$numCores
sudo make install
echo "$libDir" | sudo tee /etc/ld.so.conf.d/ffmpeg.conf
sudo ldconfig
sudo cp $binDir/* /usr/local/bin/

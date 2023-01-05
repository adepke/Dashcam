#!/bin/bash

baseDir=$(dirname $(realpath "$0"))

cd $baseDir/../ && \
apt-get install -y libx264-dev libomxil-bellagio-dev && \
./premake5_RPI gmake2 && \
make && \
cp dashcam.service /etc/systemd/system && \
systemctl enable dashcam

#!/bin/bash

set -e

baseDir=$(dirname $(realpath "$0"))

cd $baseDir/../
sudo apt install -y g++ libx264-dev
./premake5 gmake2
make
sudo cp dashcam.service /etc/systemd/system
sudo systemctl enable dashcam

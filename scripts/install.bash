#!/bin/bash

set -e

baseDir=$(dirname $(realpath "$0"))

cd $baseDir/../
sudo apt install -y g++ libx264-dev
./premake5 gmake2
make

sudo cp utils/dashcam.service.template /etc/systemd/system/dashcam.service
echo "WorkingDirectory=$(pwd)" | sudo tee -a /etc/systemd/system/dashcam.service
echo "ExecStart=$(pwd)/build/bin/dashcam" | sudo tee -a /etc/systemd/system/dashcam.service
sudo systemctl enable dashcam

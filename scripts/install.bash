#!/usr/bin/bash

set -e
set -u

baseDir=$(dirname $(realpath "$0"))

cd $baseDir/../
sudo apt install -y g++ libx264-dev xz-utils openssl libssl-dev python3 python3-distutils
./premake5 gmake2
make

./scripts/setupPython.bash

sudo cp utils/dashcam.service.template /etc/systemd/system/dashcam.service
echo "User=$(whoami)" | sudo tee -a /etc/systemd/system/dashcam.service
echo "WorkingDirectory=$(pwd)" | sudo tee -a /etc/systemd/system/dashcam.service
echo "ExecStart=$(pwd)/build/bin/dashcam" | sudo tee -a /etc/systemd/system/dashcam.service
sudo systemctl enable dashcam

#!/bin/env bash

set -e

curl https://bootstrap.pypa.io/get-pip.py --output get-pip.py
trap "rm ./get-pip.py" EXIT
python3 ./get-pip.py
python3 -m pip install google-auth google-api-python-client PyDrive2 RPi.GPIO

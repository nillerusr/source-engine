#!/bin/sh

sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt install -y -f libibus-1.0-dev:i386 ibpulse-dev:i386 g++-multilib gcc-multilib libpng-dev:i386 libjpeg-dev:i386 libfreetype6-dev:i386 libfontconfig1-dev:i386 libcurl4-gnutls-dev:i386 libsdl2-dev:i386 zlib1g-dev:i386 libbz2-dev:i386

./waf configure -T debug


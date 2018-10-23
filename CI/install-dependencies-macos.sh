#!/bin/sh
set -ex

# OBS Studio deps
brew update
brew install ffmpeg
brew install libav

# qtwebsockets deps
# qt latest
brew install ./CI/macos/qt.rb

# Packages app
brew install wget || brew upgrade wget
wget --quiet --retry-connrefused --waitretry=1 https://s3-us-west-2.amazonaws.com/obs-nightly/Packages.pkg
sudo installer -pkg ./Packages.pkg -target /

# TcxViewer
A tool for visualizing running tracks.
E.g. the Fitbit app exports tracks as TCX.

If you want to see both heartbeat, pace and speed in one graph to compare them, the App can not help you. But this tool can!

![A Screenshot of TcxViewer](/Screenshot.png?raw=true "Plotting Heartrate and Pace")

## License
TcxViewer is governed by the GNU GPL v2.0 license.

## Requirements
 - [CMake](https://cmake.org/)
 - [Qt 6](https://www.qt.io/)

## Required packages on Debian/Ubuntu
 - libgl1-mesa-dev
 - libglx-dev
 - cmake
 - g++
 - qt6-base-dev
 - libqt6charts6-dev

So, e.g. `sudo apt install libgl1-mesa-dev libglx-dev cmake g++ qt6-base-dev libqt6charts6-dev`

#!/bin/bash

# Configuration
DELAY_SECONDS=20
FRAMERATE=30
TMP_FILE=/tmp/delayed.h264

# Remove old file
rm -f $TMP_FILE

# Capture video stream with fixed framrate to tmp file
rpicam-vid -n -t 0 --width 1920 --height 1080 --framerate $FRAMERATE -o $TMP_FILE &

# Wait for the specified amount of seconds
sleep $DELAY_SECONDS

# Play h264 data with fixed framerate
ffplay -fs -f h264 -i $TMP_FILE -vf "setpts=N/$FRAMERATE/TB"

# Stop recording when script or ffplay was stopped
pkill rpicam-vid

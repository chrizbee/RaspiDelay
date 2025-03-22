#!/bin/bash

# Configuration
DELAY_SECONDS=20
FRAMERATE=30

# Capture video stream with fixed framrate to tmp file
rpicam-vid -n -t 0 --width 1920 --height 1080 --framerate $FRAMERATE -o /tmp/delayed.h264 &

# Wait for the specified amount of seconds
sleep $DELAY_SECONDS

# Play h264 data with fixed framerate
ffplay -fs -f h264 -i /tmp/delayed.h264 -vf "setpts=N/$FRAMERATE/TB"

# Stop recording when script or ffplay was stopped
pkill rpicam-vid

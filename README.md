# RaspiDelay
Delay Raspberry Pi Cam Stream

## Install Raspberry Pi OS

1. Download Raspberry Pi Imager from here: https://www.raspberrypi.com/software/
2. Choose Raspberry Pi Model and Raspberry Pi OS 64bit and flash to SD card
3. Set WiFi credentials, enable SSH, ...

## Install required packages

Update system first.

```bash
sudo apt update
sudo apt upgrade -y
```

Install git, ffplay and (optionally) locate commands.

```bash
sudo apt install -y git ffmpeg mlocate
```

## Clone repository or copy script

Either clone this repository

```bash
git clone https://github.com/chrizbee/RaspiDelay
```

or copy the [`rpicam-delay.sh`](rpicam-delay.sh) script

```bash
mkdir ~/RaspiDelay
nano ~/RaspiDelay/rpicam-delay.sh
```

## Launch script on startup

Create the desktop entry in the autostart directory

```bash
mkdir -p ~/.config/autostart
nano ~/.config/autostart/rpicam-delay.desktop
```

```ini
[Desktop Entry]
Type=Application
Exec=bash /home/pi/RaspiDelay/rpicam-delay.sh
```


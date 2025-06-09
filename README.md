# RaspiDelay
Delay Raspberry Pi Cam Stream

## Hardware requirements

- **Raspberry Pi 5**: The more RAM, the longer the possible delay
- Raspberry Pi Camera Module 3
- Any pushbutton between any GPIO and GND
- For a complete list see [hardware.md](hardware.md)

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

Install build tools and libcamera.

```bash
sudo apt install -y cmake git build-essential libcamera-dev
```

Install Qt6.

```bash
sudo apt install -y qtcreator qt6-base-dev qt6-base-dev-tools qtchooser qt6-5compat-dev qt6-multimedia-dev qt6-tools-dev qt6-tools-dev-tools qt6-wayland*
```

Install WiringPi: https://github.com/WiringPi/WiringPi.

```bash
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build debian
mv debian-template/wiringpi-*.deb .
sudo apt install ./wiringpi-*.deb
```

## Install DelayCam

Clone this repository and install the DelayCam application.

```bash
git clone https://github.com/chrizbee/RaspiDelay
cd RaspiDelay/DelayCam
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
sudo make install
```

Create a config file.

```bash
mkdir -p ~/.config && cat > ~/.config/delaycam.cfg << EOF
framerate=30.0
delay=30.0
buttonpin=17
autofocus=false
EOF
```

## Launch script on startup

Create the desktop entry in the autostart directory.

```bash
mkdir -p ~/.config/autostart
nano ~/.config/autostart/delaycam.desktop
```

```ini
[Desktop Entry]
Type=Application
Exec=DelayCam -f 30.0 -d 30 -b 17
# 30s delay @30fps, button on GPIO17
```


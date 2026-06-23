#!/bin/bash
# Setup script for NVIDIA Jetson AGX Orin Edge AI System

set -e

echo "🚀 Setting up NVIDIA Jetson AGX Orin Edge AI System"
echo "===================================================="

# Check if running on Jetson
if ! grep -q "tegra" /proc/device-tree/model 2>/dev/null; then
    echo "⚠️  Warning: Not running on a Jetson device"
    echo "   Some features may not work correctly"
fi

# Create directories
echo "📁 Creating directories..."
sudo mkdir -p /etc/edge_ai
sudo mkdir -p /var/edge_ai/data
sudo mkdir -p /var/edge_ai/logs
sudo mkdir -p /var/backups/edge_ai
sudo mkdir -p /tmp/edge_ai

# Install system dependencies
echo "📦 Installing system dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    python3-pip \
    python3-dev \
    libopencv-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-tools \
    libmosquitto-dev \
    mosquitto \
    mosquitto-clients \
    libcurl4-openssl-dev \
    libssl-dev \
    libsqlite3-dev \
    libusb-1.0-0-dev \
    libudev-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libtinyxml2-dev \
    alsa-utils \
    libasound2-dev \
    v4l-utils

# Install Python dependencies
echo "🐍 Installing Python dependencies..."
pip3 install --upgrade pip
pip3 install -r requirements.txt

# Setup GPIO permissions
echo "🔌 Setting up GPIO permissions..."
sudo groupadd -f gpio
sudo usermod -a -G gpio $USER
sudo usermod -a -G dialout $USER  # For serial

# Setup udev rules
echo "🔧 Setting up udev rules..."
cat << EOF | sudo tee /etc/udev/rules.d/99-gpio.rules
SUBSYSTEM=="gpio", ACTION=="add", PROGRAM="/bin/sh -c 'chown root:gpio /sys/class/gpio/export /sys/class/gpio/unexport ; chmod 220 /sys/class/gpio/export /sys/class/gpio/unexport'"
SUBSYSTEM=="gpio*, ACTION=="add", PROGRAM="/bin/sh -c 'chown root:gpio /sys/class/gpio/gpio* /sys/class/gpio/gpio*/direction ; chmod 660 /sys/class/gpio/gpio* /sys/class/gpio/gpio*/direction'"
EOF

# Setup serial permissions
cat << EOF | sudo tee /etc/udev/rules.d/99-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", MODE="0666", GROUP="dialout"
KERNEL=="ttyTHS*", MODE="0666", GROUP="dialout"
EOF

# Reload udev
sudo udevadm control --reload-rules
sudo udevadm trigger

# Build the project
echo "🔨 Building project..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install

# Setup service
echo "⚙️  Setting up system service..."
sudo cp scripts/edge_ai.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable edge_ai.service

echo ""
echo "✅ Setup complete!"
echo ""
echo "To start the system:"
echo "  sudo systemctl start edge_ai.service"
echo ""
echo "To run manually:"
echo "  ./build/edge_ai_app"
echo ""
echo "To view logs:"
echo "  tail -f /var/log/edge_ai/system.log"
echo ""
echo "📝 Configuration file: /etc/edge_ai/config.yaml"
echo "📁 Data directory: /var/edge_ai/data"
echo ""
echo "Reboot your system to apply group permissions:"
echo "  sudo reboot"

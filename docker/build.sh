#!/bin/bash

# Build script for Portenta MQTT Controller

set -e

echo "Building Portenta MQTT Controller..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    print_error "Docker is not installed. Please install Docker first."
    exit 1
fi

# Check if docker-compose is installed
if ! command -v docker-compose &> /dev/null; then
    print_error "docker-compose is not installed. Please install docker-compose first."
    exit 1
fi

# Build Docker image
print_status "Building Docker image..."
docker-compose build

# Start container
print_status "Starting container..."
docker-compose up -d platformio

# Wait for container to be ready
sleep 2

# Run PlatformIO build
print_status "Running PlatformIO build..."
docker-compose exec platformio platformio run

# Check if build was successful
if [ $? -eq 0 ]; then
    print_status "Build completed successfully!"
    print_status "Firmware binary available at: .pio/build/portenta_h7_m7/firmware.bin"
else
    print_error "Build failed!"
    exit 1
fi

# Optional: Upload to device (requires device to be connected)
if [ "$1" == "--upload" ]; then
    print_status "Uploading firmware to device..."
    docker-compose exec platformio platformio run --target upload
fi

# Optional: Clean build
if [ "$1" == "--clean" ]; then
    print_status "Cleaning build artifacts..."
    docker-compose exec platformio platformio run --target clean
fi

print_status "Done!"
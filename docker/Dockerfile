FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    git \
    curl \
    build-essential \
    udev \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO
RUN pip3 install -U platformio

# Create non-root user for PlatformIO
RUN useradd -m -s /bin/bash platformio && \
    usermod -aG dialout platformio && \
    usermod -aG plugdev platformio

# Setup working directory
WORKDIR /workspace

# Create udev rules for Arduino boards
RUN echo 'SUBSYSTEMS=="usb", ATTRS{idVendor}=="2341", MODE="0666"' > /etc/udev/rules.d/99-arduino.rules

# Switch to non-root user
USER platformio

# Pre-install Arduino framework and Portenta platform
RUN platformio platform install atmelsam && \
    platformio platform install ststm32

# Default command
CMD ["/bin/bash"]
FROM nvcr.io/nvidia/l4t-jetpack:r35.2.1

LABEL maintainer="Edge AI Team"
LABEL description="NVIDIA Jetson AGX Orin Edge AI System"

# Set working directory
WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y \
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
    libmosquitto-dev \
    mosquitto \
    libcurl4-openssl-dev \
    libssl-dev \
    libsqlite3-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libtinyxml2-dev \
    libasound2-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements
COPY requirements.txt .

# Install Python dependencies
RUN pip3 install --upgrade pip && \
    pip3 install -r requirements.txt

# Copy source code
COPY . .

# Build project
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install

# Create directories
RUN mkdir -p /var/edge_ai/data /var/edge_ai/logs /tmp/edge_ai

# Copy entrypoint
COPY scripts/docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# Expose ports
EXPOSE 8000 8080 1883

# Set entrypoint
ENTRYPOINT ["docker-entrypoint.sh"]

# Default command
CMD ["edge_ai_app", "--config", "/etc/edge_ai/config.yaml"]

FROM ubuntu:22.04

# Avoid interactive prompts during apt installs
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Configure and build
RUN cmake --preset linux-release && \
    cmake --build --preset linux-release-build

# Make gitter wrapper executable
RUN chmod +x gitter

# Setup welcome script (already copied by COPY . . above)
RUN chmod +x /app/docker-demo/welcome.sh && \
    cp /app/docker-demo/welcome.sh /welcome.sh && \
    chmod +x /welcome.sh

# Add to PATH
ENV PATH="/app:/app/build/linux-release:${PATH}"

# Set welcome script as entrypoint
ENTRYPOINT ["/welcome.sh"]


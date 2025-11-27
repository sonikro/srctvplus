FROM debian:bullseye

# Install system dependencies for building
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-multilib \
    g++-multilib \
    git \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# The project will be mounted here, submodules initialized via postCreateCommand
CMD ["/bin/bash"]

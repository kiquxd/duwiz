FROM debian:stable-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gdb \
    clang \
    git \
    ca-certificates \
    neovim \
    clangd \
    libftxui-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]

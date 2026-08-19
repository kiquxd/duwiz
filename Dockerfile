# syntax=docker/dockerfile:1

# PDFium is built for Linux x86-64. On Apple Silicon, pass
# --platform=linux/amd64 to Docker and let Docker Desktop use emulation.
FROM --platform=linux/amd64 debian:stable-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    libfontconfig1 \
    python3 \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN test -f third_party/preview_lib/CMakeLists.txt || \
    (echo 'preview_lib submodule is missing; run git submodule update --init --recursive' >&2; exit 1)
RUN ./scripts/bootstrap_dependencies.sh
RUN ./scripts/configure.sh \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure \
    && cmake --install build --prefix /usr/local

FROM --platform=linux/amd64 debian:stable-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    fontconfig \
    libfontconfig1 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local /usr/local

WORKDIR /data
ENTRYPOINT ["ya-ncdu"]
CMD ["--path", "/data"]

FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN echo "deb [trusted=yes] https://community-packages.deepin.com/beige/ crimson main commercial community" \
    > /etc/apt/sources.list && \
    rm -f /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources

RUN apt-get update && apt-get install -y --allow-downgrades \
    curl wget gnupg \
    build-essential cmake git pkg-config \
    qt6-base-dev \
    qt6-base-private-dev \
    libdtk6core-dev \
    libdtk6widget-dev \
    dde-file-manager-dev \
    libmpv-dev \
    libxcb-ewmh-dev \
    libdde-shell-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

CMD ["bash", "-c", "mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)"]

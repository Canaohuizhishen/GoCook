# =============================================================================
# GoCook 服务端 — 多阶段 Docker 构建
# =============================================================================
# 构建阶段
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

# 编译工具链 & 系统依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    ninja-build \
    libpq-dev \
    libssl-dev \
    libgtest-dev \
    libgmock-dev \
    pkg-config \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

# CMake 4.x（Ubuntu 24.04 自带 CMake 3.28，不满足 >= 4.1.1）
RUN pip3 install --break-system-packages cmake

# libpqxx 8.x（Ubuntu 24.04 仓库为 7.x，编译最新版）
RUN git clone --depth 1 --branch 8.0.1 https://github.com/jtv/libpqxx.git /tmp/libpqxx \
    && cmake -B /tmp/libpqxx/build -G Ninja -S /tmp/libpqxx \
    && cmake --build /tmp/libpqxx/build \
    && cmake --install /tmp/libpqxx/build \
    && rm -rf /tmp/libpqxx

WORKDIR /app

COPY contracts/ contracts/
COPY third_party/ third_party/
COPY server/ server/

RUN cmake -B /app/server/build -G Ninja -S /app/server -DBUILD_TESTING=OFF \
    && cmake --build /app/server/build --target server \
    && strip /app/server/build/server

# =============================================================================
# 运行阶段
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# libpqxx 运行时库（从构建阶段拷贝）
COPY --from=build /usr/local/lib/libpqxx* /usr/local/lib/
RUN ldconfig

# 种子图片（菜谱封面等静态资源）
COPY server/uploads/ /app/server/uploads/

COPY --from=build /app/server/build/server /app/server/

WORKDIR /app
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD curl -f http://127.0.0.1:8080/ || exit 1

ENTRYPOINT []
CMD ["./server/server"]

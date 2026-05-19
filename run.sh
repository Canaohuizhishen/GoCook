#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== 检查 PostgreSQL ==="
if ! docker ps --format '{{.Names}}' 2>/dev/null | grep -q my_postgres; then
    echo "启动 PostgreSQL 容器..."
    docker compose -f server/pg-docker/docker-compose.yml up -d
    sleep 2
fi

echo "=== 构建 ==="
cmake -B build -G Ninja -S . 2>&1 | tail -3
cmake --build build 2>&1 | tail -3

echo "=== 启动服务端 (后台) ==="
./build/server/server &
SERVER_PID=$!
echo "服务端 PID: $SERVER_PID"
sleep 1

echo "=== 启动客户端 ==="
./build/client/appclient

echo "=== 服务端停止 ==="
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
echo "已退出"

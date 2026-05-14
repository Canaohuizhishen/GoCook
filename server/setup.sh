#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

ENV_FILE=".env"
EXAMPLE_FILE="server/.env.example"

if [ -f "$ENV_FILE" ]; then
    echo ".env 已存在，跳过创建（若需重置请手动删除）"
else
    if [ ! -f "$EXAMPLE_FILE" ]; then
        echo "错误：找不到 $EXAMPLE_FILE" >&2
        exit 1
    fi
    cp "$EXAMPLE_FILE" "$ENV_FILE"

    RANDOM_SECRET=$(openssl rand -base64 32 2>/dev/null || echo "change-me-$(date +%s)-$$-$(hostname)")
    # macOS: sed -i ''; Linux: sed -i
    if [[ "$OSTYPE" == "darwin"* ]]; then
        sed -i '' "s|GoCook-Project-Secret-Key-Change-Me-In-Production|$RANDOM_SECRET|" "$ENV_FILE"
    else
        sed -i "s|GoCook-Project-Secret-Key-Change-Me-In-Production|$RANDOM_SECRET|" "$ENV_FILE"
    fi
    echo "已创建 $ENV_FILE 并写入随机 JWT 密钥"
fi

echo "GOCOOK_JWT_SECRET=$(grep '^GOCOOK_JWT_SECRET=' "$ENV_FILE" | cut -d= -f2-)"
echo "GOCOOK_DB_CONN_STRING=$(grep '^GOCOOK_DB_CONN_STRING=' "$ENV_FILE" | cut -d= -f2-)"
echo ""
echo "直接运行: ./build_server/server"

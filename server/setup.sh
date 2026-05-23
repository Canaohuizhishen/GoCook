#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

ENV_FILE=".env"
EXAMPLE_FILE="server/.env.example"

echo "============================================"
echo "  GoCook 服务端一键初始化"
echo "============================================"
echo ""

# ── 1. .env 文件 ──────────────────────────────────
if [ -f "$ENV_FILE" ]; then
    echo "✓ .env 已存在（若需重置请手动删除后重跑）"
else
    if [ ! -f "$EXAMPLE_FILE" ]; then
        echo "✗ 错误：找不到 $EXAMPLE_FILE" >&2
        exit 1
    fi
    cp "$EXAMPLE_FILE" "$ENV_FILE"

    RANDOM_SECRET=$(openssl rand -base64 32 2>/dev/null || echo "change-me-$(date +%s)-$$-$(hostname)")
    if [[ "$OSTYPE" == "darwin"* ]]; then
        sed -i '' "s|GoCook-Project-Secret-Key-Change-Me-In-Production|$RANDOM_SECRET|" "$ENV_FILE"
    else
        sed -i "s|GoCook-Project-Secret-Key-Change-Me-In-Production|$RANDOM_SECRET|" "$ENV_FILE"
    fi
    echo "✓ 已创建 $ENV_FILE 并写入随机 JWT 密钥"
fi

# ── 2. SMTP 配置引导（仅当 .env 中尚无 SMTP 配置时） ──
if ! grep -q "^GOCOOK_SMTP_HOST=" "$ENV_FILE" 2>/dev/null; then
    echo ""
    echo "── SMTP 邮件配置（用于密码重置） ──"
    echo "留空则使用开发模式（令牌将通过 API 响应返回）"
    echo ""
    read -r -p "SMTP 服务器 [留空跳过]: " SMTP_HOST
    if [ -n "$SMTP_HOST" ]; then
        read -r -p "端口 [465]: " SMTP_PORT
        SMTP_PORT=${SMTP_PORT:-465}
        read -r -p "邮箱账号: " SMTP_USER
        read -r -s -p "邮箱密码/应用专用密码: " SMTP_PASS
        echo ""
        read -r -p "发件人地址 [=$SMTP_USER]: " SMTP_FROM
        SMTP_FROM=${SMTP_FROM:-$SMTP_USER}

        cat >> "$ENV_FILE" <<EOF

# SMTP 邮件发送（用于密码重置）
GOCOOK_SMTP_HOST=$SMTP_HOST
GOCOOK_SMTP_PORT=$SMTP_PORT
GOCOOK_SMTP_USER=$SMTP_USER
GOCOOK_SMTP_PASS=$SMTP_PASS
GOCOOK_SMTP_FROM=$SMTP_FROM
EOF
        echo "✓ SMTP 配置已写入 .env"
    else
        cat >> "$ENV_FILE" <<EOF

# SMTP 邮件发送（用于密码重置，未配置时将使用开发模式）
# GOCOOK_SMTP_HOST=smtp.gmail.com
# GOCOOK_SMTP_PORT=465
# GOCOOK_SMTP_USER=
# GOCOOK_SMTP_PASS=
# GOCOOK_SMTP_FROM=
EOF
        echo "ℹ 跳过 SMTP 配置，将使用开发模式"
    fi
else
    echo "✓ SMTP 配置已存在"
fi

# ── 3. 验证关键配置 ──────────────────────────────
echo ""
echo "── 当前配置摘要 ──"
echo "  GOCOOK_DB_CONN_STRING = $(grep '^GOCOOK_DB_CONN_STRING=' "$ENV_FILE" | cut -d= -f2-)"
echo "  GOCOOK_JWT_SECRET     = $(grep '^GOCOOK_JWT_SECRET=' "$ENV_FILE" | cut -d= -f2-)"
if grep -q "^GOCOOK_SMTP_HOST=" "$ENV_FILE" 2>/dev/null; then
    echo "  SMTP                  = $(grep '^GOCOOK_SMTP_HOST=' "$ENV_FILE" | cut -d= -f2-):$(grep '^GOCOOK_SMTP_PORT=' "$ENV_FILE" | cut -d= -f2-)"
fi

# ── 4. 构建并启动 ────────────────────────────────
echo ""
echo "── 编译 ──"
cmake -B server/build -G Ninja -S server/ -DBUILD_TESTING=ON
cmake --build server/build --target server
echo "✓ 编译完成"

echo ""
read -r -p "是否启动服务端？(Y/n): " START_NOW
START_NOW=${START_NOW:-Y}
if [[ "$START_NOW" =~ ^[Yy] ]]; then
    echo ""
    echo "启动 GoCook 服务端..."
    echo "  地址: http://$(grep '^GOCOOK_HOST=' "$ENV_FILE" | cut -d= -f2-):$(grep '^GOCOOK_PORT=' "$ENV_FILE" | cut -d= -f2-)"
    echo ""
    exec server/build/server
else
    echo ""
    echo "手动启动:"
    echo "  server/build/server"
    echo ""
fi

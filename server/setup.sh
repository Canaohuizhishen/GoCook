#!/usr/bin/env bash
#
# GoCook 服务端一键初始化：
#   1) 由 server/.env.example 生成仓库根目录 .env（随机 JWT 密钥 + 注释整理）
#   2) 可选配置 SMTP 邮件（留空 = 开发模式，验证码/重置令牌打印到服务端日志）
#   3) 编译服务端（server/build 离线调试树），可选直接启动
#
# 用法：
#   ./server/setup.sh              正常流程（交互式）
#   ./server/setup.sh --env-only   只生成/校验 .env，不编译不启动（供自动化）
#
# 非交互环境（stdin 非终端）自动跳过所有提问：SMTP 跳过、不自动启动服务端。
set -euo pipefail

cd "$(dirname "$0")/.."

ENV_FILE=".env"
EXAMPLE_FILE="server/.env.example"
ENV_ONLY=0
if [ "${1:-}" = "--env-only" ]; then ENV_ONLY=1; fi

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

    # 随机 JWT 密钥：openssl 优先，/dev/urandom 兜底；两者都不可用则直接报错，
    # 绝不静默降级为可预测的弱密钥
    RANDOM_SECRET=""
    if command -v openssl > /dev/null 2>&1; then
        RANDOM_SECRET=$(openssl rand -base64 32 2>/dev/null | tr -d '\n' || true)
    fi
    if [ -z "$RANDOM_SECRET" ] && [ -r /dev/urandom ]; then
        RANDOM_SECRET=$(head -c 32 /dev/urandom | base64 | tr -d '\n')
    fi
    if [ -z "$RANDOM_SECRET" ]; then
        echo "✗ 错误：无法生成随机密钥（openssl 与 /dev/urandom 均不可用）" >&2
        exit 1
    fi

    # 单遍转换 .env.example → .env（写临时文件后 mv，回避 GNU/BSD sed -i 差异）：
    #   · 头部的"使用方法"注释换成"自动生成"说明（该行文本是示例与生成的约定标记）
    #   · JWT 密钥按 KEY 整行替换——不依赖占位符文本，占位符再改也不会静默失效
    # awk -v 会对值做反斜杠转义解释；base64 字符集（A-Za-z0-9+/=）不含反斜杠，安全
    awk -v secret="$RANDOM_SECRET" -v gen_date="$(date '+%Y-%m-%d %H:%M')" '
        /^# 使用方法：/ { print "# 本文件由 server/setup.sh 于 " gen_date " 自动生成（随机 JWT 密钥已写入）"; next }
        /^GOCOOK_JWT_SECRET=/ { print "GOCOOK_JWT_SECRET=" secret; next }
        { print }
    ' "$EXAMPLE_FILE" > "$ENV_FILE.tmp"
    mv "$ENV_FILE.tmp" "$ENV_FILE"

    # 生成后硬校验（防"静默失效"复燃）：密钥必须确已写入、且不得残留任何"请替换"字样
    if ! grep -Fq "GOCOOK_JWT_SECRET=$RANDOM_SECRET" "$ENV_FILE"; then
        echo "✗ 错误：JWT 密钥写入失败（$EXAMPLE_FILE 缺少 GOCOOK_JWT_SECRET 行？）" >&2
        exit 1
    fi
    if grep -q "请替换" "$ENV_FILE"; then
        echo "✗ 错误：.env 中仍残留占位符，请检查 $EXAMPLE_FILE 与 setup.sh 的转换规则" >&2
        exit 1
    fi
    echo "✓ 已由 $EXAMPLE_FILE 生成 $ENV_FILE（随机 JWT 密钥已写入）"
fi

# ── 2. SMTP 配置引导（仅当 .env 中尚无 SMTP 配置时） ──
if ! grep -q "^GOCOOK_SMTP_HOST=" "$ENV_FILE" 2>/dev/null; then
    if [ ! -t 0 ]; then
        echo "ℹ 非交互环境：跳过 SMTP 配置（开发模式：重置令牌/验证码打印到服务端日志）"
    else
        echo ""
        echo "── SMTP 邮件配置（用于密码重置） ──"
        echo "留空则使用开发模式（令牌将通过 [DEV MAIL] 打印到服务端日志）"
        echo ""
        SMTP_HOST=""
        read -r -p "SMTP 服务器 [留空跳过]: " SMTP_HOST || SMTP_HOST=""
        if [ -n "$SMTP_HOST" ]; then
            SMTP_PORT=""
            read -r -p "端口 [465]: " SMTP_PORT || SMTP_PORT=""
            SMTP_PORT=${SMTP_PORT:-465}
            SMTP_USER=""
            read -r -p "邮箱账号: " SMTP_USER || SMTP_USER=""
            SMTP_PASS=""
            read -r -s -p "邮箱密码/应用专用密码: " SMTP_PASS || SMTP_PASS=""
            echo ""
            SMTP_FROM=""
            read -r -p "发件人地址 [=$SMTP_USER]: " SMTP_FROM || SMTP_FROM=""
            SMTP_FROM=${SMTP_FROM:-$SMTP_USER}

            # 将示例中的注释占位行原位替换为有效配置（值经 ENVIRON 传入，防特殊字符被 awk 解释）；
            # 若示例字段结构变化导致替换落空，兜底追加完整配置块，保证配置一定生效
            SMTP_HOST="$SMTP_HOST" SMTP_PORT="$SMTP_PORT" SMTP_USER="$SMTP_USER" \
            SMTP_PASS="$SMTP_PASS" SMTP_FROM="$SMTP_FROM" awk '
                /^# GOCOOK_SMTP_HOST=/ { print "GOCOOK_SMTP_HOST=" ENVIRON["SMTP_HOST"]; next }
                /^# GOCOOK_SMTP_PORT=/ { print "GOCOOK_SMTP_PORT=" ENVIRON["SMTP_PORT"]; next }
                /^# GOCOOK_SMTP_USER=/ { print "GOCOOK_SMTP_USER=" ENVIRON["SMTP_USER"]; next }
                /^# GOCOOK_SMTP_PASS=/ { print "GOCOOK_SMTP_PASS=" ENVIRON["SMTP_PASS"]; next }
                /^# GOCOOK_SMTP_FROM=/ { print "GOCOOK_SMTP_FROM=" ENVIRON["SMTP_FROM"]; next }
                { print }
            ' "$ENV_FILE" > "$ENV_FILE.tmp"
            mv "$ENV_FILE.tmp" "$ENV_FILE"

            # 逐字段核验（防示例注释占位结构漂移导致部分字段静默缺失）；缺失才补写——
            # 整块重写会产生重复键，值经 printf %s 原样写入（不经 awk/sed 解释）
            missing_fields=0
            for key in HOST PORT USER PASS FROM; do
                grep -q "^GOCOOK_SMTP_${key}=" "$ENV_FILE" || missing_fields=1
            done
            if [ "$missing_fields" = "1" ]; then
                printf '\n# SMTP 邮件发送（由 setup.sh 补写缺失字段）\n' >> "$ENV_FILE"
                grep -q "^GOCOOK_SMTP_HOST=" "$ENV_FILE" || printf 'GOCOOK_SMTP_HOST=%s\n' "$SMTP_HOST" >> "$ENV_FILE"
                grep -q "^GOCOOK_SMTP_PORT=" "$ENV_FILE" || printf 'GOCOOK_SMTP_PORT=%s\n' "$SMTP_PORT" >> "$ENV_FILE"
                grep -q "^GOCOOK_SMTP_USER=" "$ENV_FILE" || printf 'GOCOOK_SMTP_USER=%s\n' "$SMTP_USER" >> "$ENV_FILE"
                grep -q "^GOCOOK_SMTP_PASS=" "$ENV_FILE" || printf 'GOCOOK_SMTP_PASS=%s\n' "$SMTP_PASS" >> "$ENV_FILE"
                grep -q "^GOCOOK_SMTP_FROM=" "$ENV_FILE" || printf 'GOCOOK_SMTP_FROM=%s\n' "$SMTP_FROM" >> "$ENV_FILE"
            fi
            echo "✓ SMTP 配置已写入 .env"
        else
            echo "ℹ 跳过 SMTP 配置，将使用开发模式（示例注释块保留在 .env 中备查）"
        fi
    fi
else
    echo "✓ SMTP 配置已存在"
fi

# ── 3. 验证关键配置 ──────────────────────────────
echo ""
echo "── 当前配置摘要 ──"
# 摘要打码：JWT 密钥与 DB 连接串 password 段不回显明文（防终端翻屏/CI 日志泄露；
# 完整值可自行查看 $ENV_FILE）。`|| true` 兜住 set -e/pipefail：行缺失时按"未设置"呈现。
JWT_RAW=$(grep '^GOCOOK_JWT_SECRET=' "$ENV_FILE" | cut -d= -f2- || true)
DB_RAW=$(grep '^GOCOOK_DB_CONN_STRING=' "$ENV_FILE" | cut -d= -f2- || true)
echo "  GOCOOK_DB_CONN_STRING = $(printf '%s' "$DB_RAW" | sed -E 's/(password=)[^ ]+/\1***/')"
if [ -n "$JWT_RAW" ]; then
    echo "  GOCOOK_JWT_SECRET     = ${JWT_RAW:0:8}…（已打码）"
else
    echo "  GOCOOK_JWT_SECRET     = （未设置！）"
fi
if grep -q "^GOCOOK_SMTP_HOST=" "$ENV_FILE" 2>/dev/null; then
    echo "  SMTP                  = $(grep '^GOCOOK_SMTP_HOST=' "$ENV_FILE" | cut -d= -f2-):$(grep '^GOCOOK_SMTP_PORT=' "$ENV_FILE" | cut -d= -f2-)"
fi

if [ "$ENV_ONLY" = "1" ]; then
    echo ""
    echo "✓ --env-only：.env 已就绪，跳过编译与启动"
    exit 0
fi

# ── 4. 构建并启动 ────────────────────────────────
echo ""
echo "── 编译 ──"
cmake -B server/build -G Ninja -S server/ -DBUILD_TESTING=ON
cmake --build server/build --target server
echo "✓ 编译完成"

echo ""
START_NOW=""
if [ -t 0 ]; then
    read -r -p "是否启动服务端？(Y/n): " START_NOW || START_NOW=""
    START_NOW=${START_NOW:-Y}
else
    echo "ℹ 非交互环境：不自动启动服务端"
    START_NOW="N"
fi
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

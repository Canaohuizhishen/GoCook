#!/bin/bash
# GoCook 服务端启动脚本
# 解决从文件管理器双击启动的问题：
#   1. 切换到项目根目录使 .env 可被找到
#   2. 检查必要文件
#   3. 启动 server/build/server

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP="$SCRIPT_DIR/build/server"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 切换到项目根目录（Config::load 在此查找 .env）
cd "$PROJECT_ROOT"

# 检查 .env
if [ ! -f ".env" ]; then
    echo "错误：找不到 .env 文件" >&2
    echo "请先运行 server/setup.sh 生成配置" >&2
    read -r -p "按回车键退出..."
    exit 1
fi

# 检查二进制
if [ ! -x "$APP" ]; then
    echo "错误：找不到 $APP" >&2
    echo "请先编译：cmake --build server/build --target server" >&2
    read -r -p "按回车键退出..."
    exit 1
fi

# 从 .env 提取端口用于日志显示
PORT=$(grep '^GOCOOK_PORT=' .env 2>/dev/null | cut -d= -f2-)
PORT=${PORT:-8080}

echo "============================================"
echo "  GoCook 服务端启动"
echo "  地址: http://127.0.0.1:$PORT/"
echo "============================================"

exec "$APP"

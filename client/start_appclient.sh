#!/bin/bash
# GoCook 客户端启动脚本
# 确保 Qt 平台插件可被找到，解决文件管理器双击无响应的问题

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP="$SCRIPT_DIR/build/appclient"

# 明确指定 Qt 平台插件路径（系统 Qt6 或自安装 Qt6）
export QT_QPA_PLATFORM_PLUGIN_PATH="/usr/lib/qt6/plugins:/opt/Qt/6.11.1/gcc_64/plugins"

# 如果从文件管理器启动，确保 DISPLAY 被继承
if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    export DISPLAY=:0
fi

exec "$APP"

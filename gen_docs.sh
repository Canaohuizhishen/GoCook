#!/usr/bin/env bash
# ============================================================
# GoCook API 文档一键生成（Doxygen）
#
#   1. 检查 doxygen 可用（一次性探测——系统包版本曾因库符号错配无法启动）
#   2. doxygen Doxyfile → docs/doxygen/html/（已 gitignore，不入库）
#   3. 零 warning 门禁：出现 warning 时以非零退出，清单落盘 docs/doxygen/warnings.txt
#
# 用法：./gen_docs.sh（任意目录执行均可，脚本自动切到仓库根）
# 等价命令：cmake --build build --target docs（同一份 Doxyfile）
# ============================================================
set -e
# pipefail：管道退出码取最后一个命令——不加则 doxygen ... | tail 恒返回 0，失败被静默吞掉
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=============================================="
echo "  GoCook API 文档生成（Doxygen）"
echo "=============================================="

# ── 1. 检查 doxygen ──
if [ ! -f Doxyfile ]; then
    echo "❌ 未找到 Doxyfile——请确认 gen_docs.sh 位于仓库根目录"
    exit 1
fi
if ! command -v doxygen > /dev/null 2>&1; then
    echo "❌ 未找到 doxygen。推荐官方静态二进制（见 README「API 文档生成」）；"
    echo "   Arch/Manjaro 亦可 pacman -S doxygen（系统包版本可能无法启动）"
    exit 1
fi
if ! doxygen --version > /dev/null 2>&1; then
    echo "❌ doxygen 无法运行（$(command -v doxygen)）——若为系统包管理器版本，"
    echo "   可能与系统库版本错配；请改用官方静态二进制（见 README「API 文档生成」）"
    exit 1
fi
echo "ℹ️  doxygen：$(command -v doxygen)"

# ── 2. 生成（零 warning 门禁） ──
echo ""
echo "=== 生成 API 文档 ==="
if ! doxygen Doxyfile | tail -6; then
    echo ""
    echo "❌ 生成失败：存在 warning（零 warning 门禁，见 Doxyfile WARN_AS_ERROR）或配置错误。"
    echo "   警告清单：docs/doxygen/warnings.txt"
    exit 1
fi

# ── 3. 完成 ──
echo ""
echo "✅ 生成完成（零 warning）"
echo "   入口：docs/doxygen/html/index.html（浏览器直接打开）"

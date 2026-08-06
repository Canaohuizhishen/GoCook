#!/usr/bin/env bash
# ============================================================
# GoCook 一键启动脚本（Docker 部署形态）
#
#   1. 启动数据库 + 服务端（Docker 容器，首次自动建表 + 种子数据）
#   2. 等待服务端就绪并验证数据库链路
#   3. 构建客户端（有改动才编译，走 CMake 缓存）
#   4. 启动客户端
#
# 说明：
#   - 服务端跑在容器里（gocook-server，监听 127.0.0.1:8080），
#     客户端直接连接即可，不需要在宿主机再起一份服务端进程。
#   - 默认不重建镜像（镜像已在本地时秒级启动、不依赖网络）。
#     修改了服务端代码需要重建：docker compose up -d --build（需能访问 Docker Hub）
#   - 停止服务端：docker compose stop；恢复：docker compose start
#   - 故障自救：见 README.md「API 不能用？按症状自救」
# ============================================================
set -e
# pipefail：管道退出码取最后一个命令——不加则 cmake ... | tail -2 恒返回 0，编译失败被静默吞掉
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=============================================="
echo "  GoCook 一键启动"
echo "=============================================="

# ── 1. 启动数据库 + 服务端（容器） ──
# 防呆：比较服务端源码与镜像的更新时间——源码晚于镜像则尝试重建；
# 重建失败（如无法访问 Docker Hub）则用现有镜像启动，并在末尾打印提示。
# 源码不晚于镜像则直接启动，不碰构建、不依赖网络。
#
# 注意：检查范围 = 真正影响镜像产物的文件（编译进二进制的源码 + Dockerfile）。
# server/sql/ 是 bind mount 挂给 postgres 的（docker-entrypoint-initdb.d），
# 与镜像无关，改动不需要重建镜像，必须排除——否则改个种子脚本也会误触发重建。
echo ""
echo "=== [1/4] 启动数据库 + 服务端（Docker） ==="

IMAGE_CREATED=$(docker inspect gocook-gocook-server --format '{{.Created}}' 2>/dev/null || echo "")
NEED_BUILD=0
if [ -z "$IMAGE_CREATED" ]; then
    # 镜像不存在（首次部署）→ 必须构建
    NEED_BUILD=1
elif find --version > /dev/null 2>&1; then
    # GNU findutils 支持 -newermt；BSD/macOS 的 find 不支持（直接报错），退化为跳过检测
    if find server contracts third_party Dockerfile -type f \
            -not -path "*/build/*" -not -path "*/uploads/*" \
            -not -path "*/test_data/*" -not -path "*/.qtcreator/*" \
            -not -path "*/sql/*" -not -path "*/tests/*" \
            -not -path "*/.env" -not -path "*.env.*" \
            -newermt "$IMAGE_CREATED" 2>/dev/null | grep -q .; then
        # 有源码文件晚于镜像构建时间 → 需要重建（sql/ 与 tests/ 不影响镜像产物；.env 由 compose 运行时注入也不影响）
        NEED_BUILD=1
    fi
else
    # BSD/macOS：-newermt 不可用，无法精确比较镜像构建时间，跳过自动重建检测（需手动 docker compose up -d --build）
    :
fi

BUILD_FAILED=0
if [ "$NEED_BUILD" = "1" ]; then
    if [ -z "$IMAGE_CREATED" ]; then
        echo "ℹ️  未找到镜像（首次部署），开始构建..."
    else
        echo "ℹ️  检测到服务端源码比镜像新，尝试重建镜像..."
    fi
    if docker compose up -d --build; then
        echo "✅ 镜像重建成功，已用新镜像启动"
    else
        echo "⚠️  镜像重建失败（常见原因：无法访问 Docker Hub），改用现有镜像启动"
        BUILD_FAILED=1
        docker compose up -d
    fi
else
    echo "✅ 服务端源码无更新，直接使用现有镜像启动（不联网）"
    docker compose up -d
fi

# ── 2. 等待服务端就绪 + 验证数据库链路 ──
echo ""
echo "=== [2/4] 等待服务端就绪 ==="
READY=0
for i in $(seq 1 60); do
    if curl -sf http://127.0.0.1:8080/ > /dev/null 2>&1; then
        READY=1
        break
    fi
    printf "."
    sleep 1
done
echo ""
if [ "$READY" != "1" ]; then
    echo "❌ 服务端 60 秒内未就绪，请检查：docker logs gocook-server --tail 50"
    exit 1
fi
echo "✅ 服务端已就绪：http://127.0.0.1:8080"

# 顺带验证数据库链路：公告接口能返回数据 = 服务端到数据库全通
ANN=$(curl -s http://127.0.0.1:8080/api/announcements)
if echo "$ANN" | grep -q '"error"'; then
    echo "⚠️  服务端已启动但数据库链路异常（API 返回 error）"
    echo "    常见原因与自救：见 README.md「API 不能用？按症状自救」"
    echo "    最常见：数据库容器丢了 Docker 网络 → docker network connect --alias postgres gocook_default gocook-postgres"
else
    echo "✅ 数据库链路正常（公告接口返回数据）"
fi

# ── 3. 构建客户端 ──
echo ""
echo "=== [3/4] 构建客户端 ==="
if ! command -v cmake > /dev/null 2>&1 || ! command -v ninja > /dev/null 2>&1; then
    echo "⚠️  未找到 cmake/ninja，跳过构建（请用 Qt Creator 构建客户端）"
    if [ ! -x build/client/appclient ]; then
        echo "❌ 且未找到已构建的 build/client/appclient，无法启动客户端"
        exit 1
    fi
else
    if ! cmake -B build -G Ninja -S . 2>&1 | tail -2; then
        echo "❌ CMake 配置失败，请检查上方错误输出"
        exit 1
    fi
    if ! cmake --build build --target appclient 2>&1 | tail -2; then
        echo "❌ 客户端编译失败，请检查上方错误输出（不会静默启动旧版客户端）"
        exit 1
    fi
fi

# ── 4. 启动客户端 ──
echo ""
echo "=== [4/4] 启动客户端 ==="

# 构建失败但用旧版启动时的醒目提示（脚本末尾，客户端启动前）
if [ "$BUILD_FAILED" = "1" ]; then
    echo ""
    echo "⚠️⚠️⚠️  重要提示 ⚠️⚠️⚠️"
    echo "  服务端源码有改动，但镜像重建失败，当前启动的是【旧版服务端】！"
    echo "  你的服务端改动【未生效】。"
    echo "  解决方案："
    echo "    1) 网络恢复后执行：docker compose up -d --build"
    echo "    2) 或断网本地调试：docker compose stop && cmake -B server/build -G Ninja -S server/ && cmake --build server/build --target server && ./server/build/server"
    echo ""
fi

if [ ! -x build/client/appclient ]; then
    echo "❌ 找不到 build/client/appclient，请先构建客户端（本脚本第 3 步或 Qt Creator）"
    exit 1
fi
echo "（服务端继续在容器中运行，关闭客户端不影响服务端）"
./build/client/appclient

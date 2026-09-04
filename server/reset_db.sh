#!/usr/bin/env bash
# =====================================================================
# GoCook 开发库一键重置脚本（⛔ 仅供开发/测试库！会清空全部数据）
#
# 用法：
#   ./reset_db.sh                         # 交互确认后重置（连接串自动解析）
#   ./reset_db.sh -y                      # 跳过确认直接重置
#   ./reset_db.sh -y "dbname=gocookdb user=gocook password=xxx host=127.0.0.1 port=5432"
#   ./reset_db.sh -y --docker             # 经 docker exec gocook-postgres 执行
#
# 执行内容（按依赖顺序，任一步失败立即中止、返回非零）：
#   1) create_all_tables.sql         删全部表并重建（含 inventory 三维唯一约束）
#   2) seed_ingredient_nutrition.sql 食材营养种子（200 行，幂等）
#   3) seed_test_data.sql            测试账号/菜谱/库存/购物清单等（幂等）
#
# 注意：upgrade_*.sql 是存量库只增迁移，重置后无需执行（约束已由第 1 步建好）。
# 连接串解析顺序：命令行参数 → $GOCOOK_DB_CONN_STRING → 仓库根 .env。
# =====================================================================
set -euo pipefail

cd "$(dirname "$0")/sql"   # 脚本位于 server/ 根，SQL 文件在 server/sql/ 下

FORCE=0
DOCKER=0
CONN=""
for arg in "$@"; do
    case "$arg" in
        -y|--yes|--force) FORCE=1 ;;
        --docker) DOCKER=1 ;;
        -*) echo "未知参数: $arg" >&2; exit 2 ;;
        *) CONN="$arg" ;;
    esac
done

# ── 1. 连接串解析 ─────────────────────────────────────────────
if [ -z "$CONN" ] && [ -n "${GOCOOK_DB_CONN_STRING:-}" ]; then
    CONN="$GOCOOK_DB_CONN_STRING"
fi
if [ -z "$CONN" ]; then
    # server/sql → server/.env → 仓库根 .env
    for ENV in "../.env" "../../.env"; do
        if [ -f "$ENV" ]; then
            CONN="$(grep '^GOCOOK_DB_CONN_STRING=' "$ENV" | head -1 | cut -d= -f2-)"
            [ -n "$CONN" ] && break
        fi
    done
fi
if [ -z "$CONN" ]; then
    echo "✗ 找不到数据库连接串。请任选其一：" >&2
    echo "  1) 命令行参数传入（见脚本头用法）" >&2
    echo "  2) 设置环境变量 GOCOOK_DB_CONN_STRING" >&2
    echo "  3) 在仓库根目录 .env 提供 GOCOOK_DB_CONN_STRING=..." >&2
    exit 1
fi

if [ "$DOCKER" = "1" ]; then
    if ! docker exec gocook-postgres pg_isready -U gocook -d gocookdb >/dev/null 2>&1; then
        echo "✗ 容器 gocook-postgres 不可用（docker ps 检查）" >&2
        exit 1
    fi
else
    if ! command -v psql >/dev/null 2>&1; then
        echo "✗ 未找到 psql，请安装 postgresql-client 或使用 --docker 模式" >&2
        exit 1
    fi
    if ! psql "$CONN" -c "SELECT 1" >/dev/null 2>&1; then
        echo "✗ 无法连接数据库，请检查连接串" >&2
        exit 1
    fi
fi

# ── 2. 红线确认 ───────────────────────────────────────────────
echo "============================================================"
echo "  GoCook 数据库一键重置"
echo "  ⛔ 将删除全部表并重建，再灌入测试种子数据"
echo "  ⛔ 仅供开发/测试库使用，任何现有数据都会丢失！"
echo "============================================================"
if [ "$FORCE" = "0" ]; then
    read -r -p "确认重置？(输入 yes 继续): " ANSWER
    [ "$ANSWER" = "yes" ] || { echo "已取消"; exit 0; }
fi

# ── 3. 按序执行三个 SQL（各自文件内已 \set ON_ERROR_STOP on） ──
run_sql() {
    local file="$1"
    echo ""
    echo "── 执行 $file ..."
    if [ "$DOCKER" = "1" ]; then
        docker exec -i gocook-postgres psql -U gocook -d gocookdb -v ON_ERROR_STOP=1 < "$file"
    else
        psql "$CONN" -v ON_ERROR_STOP=1 -f "$file"
    fi
}

run_sql create_all_tables.sql
run_sql seed_ingredient_nutrition.sql
run_sql seed_test_data.sql

# ── 4. 统计验证 ───────────────────────────────────────────────
if [ "$DOCKER" = "1" ]; then
    stat_q() { docker exec gocook-postgres psql -U gocook -d gocookdb -t -A -c "$1"; }
else
    stat_q() { psql "$CONN" -t -A -c "$1"; }
fi

echo ""
echo "── 重置完成，统计验证 ──"
echo "  users                = $(stat_q 'SELECT COUNT(*) FROM users')"
echo "  recipes              = $(stat_q 'SELECT COUNT(*) FROM recipes')"
echo "  ingredient_nutrition = $(stat_q 'SELECT COUNT(*) FROM ingredient_nutrition')"
echo "  inventory            = $(stat_q 'SELECT COUNT(*) FROM inventory')"
echo "  shopping_lists       = $(stat_q 'SELECT COUNT(*) FROM shopping_lists')"
echo "  三维唯一约束          = $(stat_q "SELECT COUNT(*) FROM pg_constraint WHERE conname = 'inventory_user_id_ingredient_name_unit_key'")"

echo ""
echo "✅ 重置成功（开发库已回到与脚本一致的干净种子态）"

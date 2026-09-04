# GoCook server/sql 目录说明

开发/测试数据库的全部初始化与维护脚本。**均为开发阶段专用；生产库禁止执行清空类脚本。**

## 脚本一览

| 文件 | 作用 | 可否重复执行 | 何时执行 |
| --- | --- | --- | --- |
| `create_all_tables.sql` | ⛔ DROP 全部表后重建（含 inventory 三维唯一约束 `(user_id, ingredient_name, unit)`） | 可重复，但每次都会**清空全部数据** | 全新库 / 一键重置（server/reset_db.sh 第 1 步） |
| `seed_ingredient_nutrition.sql` | 食材营养种子（200 行，每 100g 含量），幂等全量同步（DELETE + INSERT） | ✅ 可重复 | 建表之后（server/reset_db.sh 第 2 步）；营养库修正后也可单独重跑 |
| `seed_test_data.sql` | 测试种子：testuser 账号、23 道菜谱、库存、购物清单、通知、评论等；ON CONFLICT/动态 id，幂等 | ✅ 可重复 | 建表之后（server/reset_db.sh 第 3 步） |

> 曾短暂存在过 `upgrade_20260904_inventory_unit.sql`（存量库约束升级脚本，未纳入版本库），
> 已移除——开发阶段统一走 reset_db.sh 重建，不维护增量迁移文件。若未来出现仍保留旧约束
> （`UNIQUE (user_id, ingredient_name)`）的存量库，需要"只升级结构、保留数据"时，手工执行：
> ```sql
> ALTER TABLE inventory DROP CONSTRAINT IF EXISTS inventory_user_id_ingredient_name_key;
> ALTER TABLE inventory ADD CONSTRAINT inventory_user_id_ingredient_name_unit_key
>     UNIQUE (user_id, ingredient_name, unit);
> ```
> ⚠️ 本目录**没有** `upgrade_*.sql` 文件——任何指向该文件的指引都是过时的，按上面内联 SQL 执行。
>
> 与新建库 `create_all_tables.sql` 对齐的收尾（**可选**）：三维唯一约束对 NULL 单位行不生效，
> 历史 NULL 单位行（应用层校验自始禁止，只可能来自早期手工写库）建议归一后收紧列约束。
> '克' 为确定性兜底值，不精确的单位可在客户端编辑入口改回：
> ```sql
> UPDATE inventory SET unit = '克' WHERE unit IS NULL;
> ALTER TABLE inventory ALTER COLUMN unit SET DEFAULT '克';
> ALTER TABLE inventory ALTER COLUMN unit SET NOT NULL;
> ```


## 一键重置（推荐入口）

> 脚本位于 **server/ 根目录**（`server/reset_db.sh`），刻意不放本目录——本目录被 docker-compose
> bind mount 到 postgres 容器的 `/docker-entrypoint-initdb.d`，首次建库时容器会自动执行其中
> 所有 `*.sql` / `*.sh`，放 .sh 会被当成初始化脚本误执行。本目录只放 SQL。

```bash
# 在 server/ 目录下
cd ../ && ./reset_db.sh     # 交互确认后执行（红线提示会清空数据）
./reset_db.sh -y           # 跳过确认（日常开发/CI）
./reset_db.sh -y "dbname=... user=... password=... host=... port=..."   # 显式连接串
./reset_db.sh -y --docker  # 经 docker exec gocook-postgres 执行（容器部署）
```

重置流程 = `create_all_tables.sql`（清空重建）→ `seed_ingredient_nutrition.sql` → `seed_test_data.sql`，
任一步失败立即中止并返回非零退出码，不会静默产出残缺数据。
连接串解析顺序：命令行参数 → 环境变量 `GOCOOK_DB_CONN_STRING` → 仓库根 `.env`。

> 适用场景：需要按新 schema 重灌数据、或库被脏数据污染时，把库拉回与脚本一致的干净种子态。

## 什么时候不要用 reset

- **存量库只想升级结构、保留数据**：执行上方"曾短暂存在过"块内联的 ALTER 语句（本目录已无 `upgrade_*.sql`），不要跑 create_all_tables。
- **生产/演示数据**：任何清空类脚本都不应执行。

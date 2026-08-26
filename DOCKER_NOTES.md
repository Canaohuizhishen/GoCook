# GoCook Docker / PostgreSQL 操作笔记

> 由旧 `server/pg-docker/note.txt` 改写适配（2026-08）。当时项目有两套数据库（`my_postgres` + pgAdmin），**已合并为一套 `gocook-postgres`**，本文全部命令以现役部署为准。

---

## 1. Docker Compose 是什么

一个用于定义和运行多容器 Docker 应用的工具。只需在一个 `.yml` 文件中配置好所有服务（容器），一条命令就能启动、停止、管理整个应用栈。

GoCook 的 compose 文件在**项目根目录** `GoCook/docker-compose.yml`，定义了两个服务：

- `postgres` → 容器 `gocook-postgres`（PostgreSQL 15，端口 5432，首次启动自动建表 + 灌种子）
- `gocook-server` → 容器 `gocook-server`（C++ 服务端，端口 8080）

## 2. 常用 Docker Compose 命令

以下命令都在项目根目录执行：

```bash
cd /root/project/mainProject/GoCook/GoCook

docker compose up -d              # 启动所有服务（-d = 后台运行）
./run.sh                          # 一键全流程：起服务端 + 构建并启动客户端（日常推荐）
docker compose ps                 # 查看运行状态
docker compose logs -f            # 实时跟踪所有容器日志
docker compose logs -f gocook-server   # 只看服务端日志（排错首选）
docker compose stop               # 停止（保留容器和卷）
docker compose start              # 启动已停止的容器
docker compose restart            # 重启
docker compose up -d --build      # 改过服务端代码后重建镜像并重启
docker compose down               # 停止并删除容器、网络（⚠️ 保留数据卷）
docker compose down -v            # 同上，但连数据卷一起删（⚠️⚠️ 会清空数据库，慎用！）
```

## 3. 通过 PostgreSQL 容器执行 SQL

进入 psql 交互终端：

```bash
docker exec -it gocook-postgres psql -U gocook -d gocookdb
# 输入 \q 退出
```

执行 SQL 文件（例如手动重建表 / 重新灌种子）：

```bash
docker exec -i gocook-postgres psql -U gocook -d gocookdb < server/sql/create_all_tables.sql
docker exec -i gocook-postgres psql -U gocook -d gocookdb < server/sql/seed_test_data.sql
# 食材营养库（投稿营养自动计算用；幂等可重复执行，旧库升级只需跑这一条）
docker exec -i gocook-postgres psql -U gocook -d gocookdb < server/sql/seed_ingredient_nutrition.sql
```

常用 psql 命令速查：

```sql
\dt                 -- 列出所有表
\d recipes          -- 查看表结构
SELECT count(*) FROM recipes;
SELECT id, name, status FROM recipes ORDER BY id;
```

## 4. 备份与恢复（重要）

备份整个数据库到文件：

```bash
# 逻辑备份（在宿主机执行，输出到当前目录）
docker exec gocook-postgres pg_dump -U gocook -d gocookdb > gocook_backup.sql
# 只备份数据（不含表结构）
docker exec gocook-postgres pg_dump -U gocook -d gocookdb --data-only > gocook_data.sql
```

恢复：

```bash
docker exec -i gocook-postgres psql -U gocook -d gocookdb < gocook_backup.sql
```

> 数据实际存放在 Docker 卷 `gocook_postgres_data` 里，`docker compose down` 不会删它。定期 `pg_dump` 一份到宿主机文件是更稳妥的习惯。

## 5. 测试账号与数据库配置

| 项 | 值 |
|---|---|
| 数据库 | gocookdb |
| 用户名 / 密码 | gocook / gocook123 |
| 测试账号 | testuser / **test123** |
| 服务端地址 | http://127.0.0.1:8080 |

## 6. 历史说明（为什么只有一套数据库）

早期开发用的是 `server/pg-docker/docker-compose.yml`（容器 `my_postgres` + Web 管理工具 pgAdmin `my_pgadmin`），后来部署形态改为根目录 compose（`gocook-postgres` + `gocook-server`），两套 PostgreSQL 都映射宿主 5432 端口，只能同时跑一个。

**2026-08 已合并**：旧库数据（含用户操作记录）已通过 `pg_dump --on-conflict-do-nothing` 导入现役库并验证，旧容器、旧网络、旧数据卷已整体删除。旧方案目录 `server/pg-docker/` 也已移除，相关文件仅存在于 Git 历史中。

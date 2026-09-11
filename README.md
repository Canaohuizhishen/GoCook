# GoCook — 菜谱管理与智能烹饪推荐系统

Qt QML 桌面客户端 + C++ httplib 服务端 + PostgreSQL。C/S 架构，智能推荐（库存匹配 + 健康过滤）、库存与购物清单闭环、评分评论、营养报告。

```
GoCook/
├── client/            # Qt QML 客户端（23 页面 / 5 ViewModel / SQLite 离线缓存）
├── server/            # C++ 服务端（Router → Handler → Service → Repository）
├── contracts/         # 接口层（IServices / IGoCookApi / I*Repository / DataModels）
├── docker-compose.yml # 一键起数据库 + 服务端
├── Dockerfile         # 服务端多阶段镜像
├── DOCKER_NOTES.md    # Docker / PostgreSQL 操作笔记（备份恢复、常用命令）
├── run.sh             # 一键启动脚本（服务端+客户端）
└── docs/              # GoCook_Design.md（设计说明书）+ api-spec.md（现行契约）+ archive/（历史文档存档）
```

---

## 一、第一次启动项目（新手必看）

### 第 0 步：确认依赖

```bash
docker --version          # 需要 Docker
docker compose version    # 需要 Compose v2
```

### 第 1 步：一键启动（数据库 + 服务端）

> 🚀 **懒人模式**：`./run.sh` 一条命令搞定全部——起服务端（自动建表种子）→ 等就绪 → 构建客户端 → 拉起客户端。下面 2~4 步是它的手动版，方便你理解每一步在干嘛。

```bash
cd /root/project/mainProject/GoCook/GoCook
./run.sh
```

### 手动版（等价于 run.sh 的分解步骤）

```bash
cd /root/project/mainProject/GoCook/GoCook
docker compose up -d
```

- 首次启动会自动执行 `server/sql/` 下的建表脚本和种子数据（PostgreSQL 数据卷为空时自动初始化，无需手动导 SQL）
- ⚠️ **旧库补表**：`docker-entrypoint-initdb.d` 只在首次初始化时生效——旧版本初始化的数据卷不会自动执行新脚本，如需新增表结构请用**只增迁移脚本**（可安全重复执行）
- ⛔ **绝不要对已有数据的库执行 `create_all_tables.sql`**：它开头会对全部业务表执行 `DROP TABLE IF EXISTS ... CASCADE`，是"清空重建"脚本，只在全新初始化（空数据卷）时由容器自动运行。对旧库手跑一遍 = 清库（菜谱/用户/库存全部丢失），需要 `server/sql/seed_test_data.sql` 重新播种。
- ⏳ **首次执行会构建服务端镜像**（多阶段 Dockerfile，需要下载依赖编译，约几分钟，属正常现象；之后启动是秒级）
- 启动完成后两个容器应该都是 healthy：`docker ps` 应看到 `gocook-server`（8080）和 `gocook-postgres`（5432）

### 第 2 步：验证 API 是否可用

```bash
curl http://127.0.0.1:8080/api/announcements
```

**预期**：返回 JSON 公告数组（不是 error）。
再验证一个列表接口：

```bash
curl "http://127.0.0.1:8080/api/recipes/public?page=1&size=2"
```

**预期**：返回 `{"data":[...],"pagination":{...}}`，data 非空（种子有 23 道菜谱）。

### 第 3 步：登录测试账号

| 用户名 | 密码 |
|---|---|
| `testuser` | **`test123`**（⚠️ 不是 123456，这是种子数据的真实密码） |

```bash
curl -X POST http://127.0.0.1:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"test123"}'
```

**预期**：返回 `{"token":"eyJ...","user_id":1,"username":"testuser"}`。

### 第 4 步：启动客户端

客户端**直接连接 Docker 里的服务端**即可（客户端默认连 `http://127.0.0.1:8080`，容器已映射该端口），**不需要**再单独启动一个服务端进程。

- **方式 A（推荐）**：Qt Creator 打开 `client/CMakeLists.txt`，构建运行 `appclient`
- **方式 B（命令行）**：

```bash
cd /root/project/mainProject/GoCook/GoCook
cmake -B build -G Ninja -S .          # 顶层构建（server + client）
cmake --build build --target appclient
./build/client/appclient
```

### 日常启动 / 停止

```bash
./run.sh                  # 一键：起服务端 + 客户端（推荐日常用这个）
docker compose up -d      # 只起服务端（客户端已在跑、只想重启服务端时）
docker compose stop       # 停止（保留容器和卷）
docker compose down       # 移除容器（数据卷保留，数据不丢）
docker compose down -v    # ⚠️ 连数据卷一起删（数据全丢，慎用！）
```

---

## 二、API 不能用？按症状自救

> 通用排查三板斧（先做这三个，80% 的问题能定位）：
> ```bash
> docker ps                                    # ① 容器还活着吗？
> curl http://127.0.0.1:8080/                  # ② 服务进程活着吗？（200 = 活着）
> docker logs gocook-server --tail 50          # ③ 真实错误永远在日志里
> ```

### 症状 A：`curl` 直接拒绝连接 / 页面打不开

**原因**：服务端没在跑。
**修复**：

```bash
docker compose up -d
```

### 症状 B：`/` 能开（200），但所有 API 返回 `{"error":"服务器内部错误，请稍后重试"}`

**原因**：服务端进程活着，但**连不上数据库**。最常见的是数据库容器丢了 Docker 网络（系统重启后易发）。

**确认**：看日志，出现下面这行就是它：

```bash
docker logs gocook-server --tail 50
# 输出: could not translate host name "postgres" to address: No address associated with hostname
```

**修复**（把数据库容器接回 compose 网络，别名必须是 `postgres`）：

```bash
docker network connect --alias postgres gocook_default gocook-postgres
```

然后重新验证：

```bash
curl http://127.0.0.1:8080/api/announcements
```

> 💡 这个故障可能会复发（Docker 网络子系统的已知毛病），症状 B + 上面这行日志 = 直接执行 connect 命令，不用动别的。

### 症状 C：登录返回"用户名或密码错误"，但密码明明是对的

**两个可能**：

1. **密码真的不对**：测试账号密码是 `test123`，不是 `123456`。
2. **数据库又挂了**：登录接口把所有服务端异常都统一映射成 401"用户名或密码错误"，**数据库连不上时也会报这个**——别被误导，先看日志：

```bash
docker logs gocook-server --tail 20    # 有 Database error / could not translate → 按症状 B 处理
```

### 症状 D：在 Qt Creator 里跑了服务端，API 还是不能用

**原因**：本地编译的服务端想监听 8080，但 8080 已被 Docker 容器占用——**bind 失败后进程不会退出，会"假启动"挂着**（没有日志、没有端口监听，看起来像活着）。

**确认**：

```bash
ss -tlnp | grep 8080    # 只有 docker-proxy 在监听 = 本地 server 是假启动
```

**修复**：本地调试服务端代码前，先停掉容器：

```bash
docker compose stop     # 释放 8080
# 然后在 QtCreator 里正常跑本地 server
# 调试完再恢复：
docker compose up -d
```

> 💡 平时用客户端连 Docker 服务端就够，**不需要**在本地跑 server。

### 症状 E：`docker compose up` 报 `port is already allocated`

**原因**：端口被其他进程/容器占着。

**修复**：

```bash
docker ps -a          # 看谁占着 5432 / 8080
# 找到占用者后停掉它再重新 up
```

> 💡 历史背景：项目早期存在两套 PostgreSQL（`my_postgres` 与 `gocook-postgres` 共用 5432），**已于 2026-08 合并为一套**（旧库数据已导入现役库，旧容器与卷已删除）。现在只有一个数据库，此症状基本只会来自外部进程占端口。

### 症状 F：数据好像丢了？

**不会丢**：数据在 Docker 卷 `gocook_postgres_data` 里（compose 卷名 `postgres_data` 会被项目名前缀为 `gocook_`），`docker compose down`（不带 `-v`）不会删数据，重建容器数据仍在。

万一卷真的没了，可以手动重建表 + 灌种子：

```bash
docker exec -i gocook-postgres psql -U gocook -d gocookdb < server/sql/create_all_tables.sql
docker exec -i gocook-postgres psql -U gocook -d gocookdb < server/sql/seed_test_data.sql
```

> 📌 **营养自动计算**（2026-08 新增）：投稿/编辑菜谱时服务端按食材清单自动计算营养，依赖 `ingredient_nutrition` 食材营养表。
> - **全新初始化**：`create_all_tables.sql` 已含建表，直接再跑 `server/sql/seed_ingredient_nutrition.sql` 灌入 200 种常见食材数据（幂等，可重复执行）：
>   ```bash
>   docker exec -i gocook-postgres psql -U gocook -d gocookdb < server/sql/seed_ingredient_nutrition.sql
>   ```
> - **已有库升级**：只需执行上面这条种子脚本（建表语句在脚本内会先执行）。种子对已存在行按规范名同步修订（`ON CONFLICT DO UPDATE`），营养库修正/扩充随重跑自动生效，无需手工 DELETE。
> - 未收录/无法换算的食材不计入营养（营养报告页会列出"未计入营养的食材"）；全部无法计算时该菜谱营养接口返回 `has_data:false`，客户端展示"暂无营养报告"空态（不再显示全 0 假数据）。若投稿时填写了 `nutrition` 手填值，则自动计算失败时回退使用手填值。
> - **降级说明**：营养表缺失或查询异常时，投稿/编辑不会失败，服务端记日志降级处理——投稿按"手填值或空"入库；**编辑若无手填值则保留库中已有的营养数据，不因计算不可用而清空已保存的营养**。但营养功能需建表+灌种子后才可用，请按上述步骤先初始化。
> - 单位换算约定：质量单位（克/千克/斤/两/磅，含 g/G/kg/KG、lb/LB）与体积单位（毫升/升按 1ml≈1g，含 ml/mL/ML、L/l）直接换算；容器单位按固定容量近似（杯≈240ml、碗≈200ml、汤匙≈15ml、勺≈10ml、茶匙≈5ml）；计数单位（个/只/根…）依赖食材表 `default_portion_g`，无单重则跳过；单位首尾空白自动忽略。

---

## 三、脚本使用说明书

项目一共 5 个脚本 + 1 个废弃备份，速查表：

| 脚本 | 一句话用途 | 什么时候用 |
|---|---|---|
| `./run.sh` | **一键启动**（服务端+客户端） | ⭐ 日常主入口 |
| `./server/setup.sh` | 本地开发环境初始化（生成 .env + 编译） | 第一次本地跑服务端 / .env 丢了 |
| `./server/start_server.sh` | 启动本地服务端（宿主机进程） | 本地调试服务端代码 |
| `./server/run_tests.sh` | 编译并运行 90+ 单元测试 | 每次改服务端代码后回归 |
| `./client/start_appclient.sh` | 启动客户端（修 Qt 插件路径） | 客户端已构建好，想直接跑 |

---

### 1. `./run.sh` — 一键启动（⭐ 日常主入口）

**用途**：一条命令完成全部：起数据库+服务端（Docker 容器）→ 等就绪并验证 → 构建客户端 → 拉起客户端。

**用法**：

```bash
cd /root/project/mainProject/GoCook/GoCook
./run.sh
```

**脚本内部做了什么**（4 步）：

1. **防呆检测**：比较服务端源码（`server/` `contracts/` `third_party/` `Dockerfile`，排除 build/uploads 等产物目录）与镜像构建时间——源码晚于镜像则尝试 `docker compose up -d --build` 重建；**重建失败（如断网）改用现有镜像启动，并在末尾打印醒目提示**；源码无更新则直接用现有镜像启动（不联网）
2. 轮询等待服务端就绪（最多 60 秒），然后 curl 公告接口验证数据库链路——不通会直接打印自救提示
3. `cmake` 构建客户端（未找到 cmake/ninja 时跳过，要求已有构建产物）
4. 启动客户端

**注意事项**：

- 首次执行要构建服务端镜像（约几分钟），之后秒级
- 客户端关闭后服务端**继续在容器里运行**（正常现象，`docker compose stop` 才停）
- 镜像重建需要能访问 Docker Hub；断网时若检测到源码有更新，脚本会用旧镜像启动并明确提示"改动未生效"，**不会静默跑旧版**
- 想跳过客户端构建、只重启服务端：`docker compose up -d`

---

### 2. `./server/setup.sh` — 本地开发环境初始化

**用途**：生成 `.env`（随机 JWT 密钥）、可选配置 SMTP 邮件、编译服务端。**本地跑服务端（宿主机进程）前必跑一次**。

**用法**：

```bash
cd /root/project/mainProject/GoCook/GoCook
./server/setup.sh
```

**注意事项**：

- `.env` 已存在时会跳过生成；想重置密钥就删掉 `.env` 再跑
- SMTP 配置**留空 = 开发模式**：密码重置接口会把令牌直接返回在响应里；注册验证码等邮件内容打印到服务端日志（方便调试），配置了 SMTP 才真正发邮件
- 脚本最后会问是否启动服务端（`server/build/server`），日常用容器的话选 N 即可

---

### 3. `./server/start_server.sh` — 启动本地服务端（宿主机进程）

**用途**：从文件管理器双击也能正确启动本地编译的服务端（自动切到项目根目录找 `.env`，避免"找不到配置"问题）。

**用法**：

```bash
./server/start_server.sh
```

**⚠️ 使用前必须先停容器**（8080 被容器占着会导致本地 server 假启动）：

```bash
docker compose stop      # 释放 8080
./server/start_server.sh
# 调试完恢复：
docker compose up -d
```

---

### 4. `./server/run_tests.sh` — 跑单元测试

**用途**：一键编译并运行 90+ 个单元测试（GoogleTest + Mock，不依赖数据库，毫秒级）。

**用法**：

```bash
./server/run_tests.sh
```

**注意事项**：

- 需要系统已装 gtest/gmock 开发包（Debian/Arch：`libgtest-dev`）；首次运行会自动 cmake 配置
- **每次改完服务端代码都跑一遍**，这是项目的回归底线

---

### 5. `./client/start_appclient.sh` — 启动客户端

**用途**：修复 Qt 平台插件路径问题，让客户端在文件管理器里双击也能正常启动（普通终端直接 `./build/client/appclient` 也行）。

**用法**：

```bash
./client/start_appclient.sh
```

**注意事项**：要求客户端已构建（`client/build/appclient` 存在，Qt Creator 构建或 `cmake -B client/build -S client/` 均可）。

---

## 四、开发相关（选读）

### 服务端单独构建 + 测试

```bash
cmake -B server/build -G Ninja -S server/ -DBUILD_TESTING=ON
cmake --build server/build --target server
./server/run_tests.sh          # 跑测试（详见脚本说明书第 4 条）
```

### 本地调试服务端代码（完整流程）

```bash
./server/setup.sh              # ① 首次：生成 .env（详见脚本说明书第 2 条）
docker compose stop            # ② 停容器释放 8080（否则本地 server 假启动）
./server/start_server.sh       # ③ 启动本地 server（详见脚本说明书第 3 条）
# ④ 在 Qt Creator 里打断点调试，或改代码后重新编译
# ⑤ 调试完恢复容器部署：
docker compose up -d
```

### 数据库直连（调试 SQL）

```bash
docker exec -it gocook-postgres psql -U gocook -d gocookdb
```

---

## 五、常见坑速查

| 坑 | 说明 |
|---|---|
| 测试密码 | `testuser` / `test123`（不是 123456） |
| 数据库 | **只有一套**：`gocook-postgres`（2026-08 已合并旧库 `my_postgres`，旧容器/卷已删） |
| 本地 server 假启动 | 8080 被容器占时本地 server bind 失败但不退出，看起来活着实际没服务 |
| 网络丢端点 | 症状 B 的 `docker network connect --alias postgres gocook_default gocook-postgres` 命令 |
| 登录 401 迷惑性 | "用户名或密码错误"也可能是数据库故障（异常被统一映射成 401），先看日志 |
| 数据安全 | `compose down` 不删卷；只有 `down -v` 才删，别加 `-v` |

## 六、离线行为（开发者必读）

### 设计决策

**断网时不做写队列、不自动重放**（v2 决策，曾实现过离线写队列 + 服务端幂等重放，体验差且复杂度高，已整体拆除）。离线体验按主流 App（拼多多/淘宝）模式分层：

- **读 · 有缓存的页面（菜谱详情）**：断网时静默显示本地缓存（最近看过的 20 个菜谱），**零提示、与联网状态无异**；网络恢复后自动回到最新数据
- **读 · 无缓存的页面（收藏等）**：断网时内存有旧数据则静默显示旧数据（与联网无异）；无数据则页面中心显示「网络连接失败，请检查网络」+ 重试按钮
- **写**：断网时**明确报错**，不假装成功、不后台排队：全局底部黑色 toast「网络连接失败，请检查网络」（1.5 秒自动消失）；页面自行呈现的写操作（收藏/评分/投稿/设置等）由页面内提示负责，不再重复弹全局提示。用户恢复网络后手动重试即可（写操作都是一键点击，重试成本为零）
- **全局 toast 定位**：兜底通道——只弹未被页面自行呈现的请求失败（我的投稿/我的评分/购物清单加载、评分写操作等）；详情/收藏的加载类失败已在 API 层抑制（页面显示缓存或居中离线视图），不弹全局
- 服务端无幂等缓存表：`idempotency_keys` 已随功能移除，`pending_operations` 表在客户端启动时自动清理

### 故障排查提示

| 现象 | 说明 |
|---|---|
| 断网写操作无反应 | 应为底部黑色 toast + 操作失败回调；若页面无提示说明该请求被抑制且页面未自呈现，按收藏页模式补接线 |
| 断网进详情页 | 有缓存 → 静默显示缓存（零提示，与联网状态无异）；无缓存 → 页面中心「网络连接失败」+ 重试 |
| 断网进收藏页 | 有旧数据 → 静默显示旧数据；无数据 → 页面中心「网络连接失败」+ 重试 |
| 新页面要实现离线视图 | 复用 `NetworkOfflineView` 组件 + ViewModel 加 `xxxLoadFailed` 属性（参考收藏页模式） |

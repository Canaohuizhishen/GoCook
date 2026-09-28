# GoCook 待办清单（TODO）

> **用途**：记录「已发现、暂缓处理」的问题与改进项，供日后直接动手——不必重新调查。
> **初版**：2026-09-28（调查来源：营养快照语义 / CMake 头文件登记 / 认证与 token 安全）。
> **维护约定**：新条目追加到对应分节，格式统一为「现状（file:line 证据）→ 影响 → 建议方案」；
> **完成后直接删除条目**（修复落点与证据在 git 提交历史里；编号保持稳定、不复用、不重排）；
> 「已评估·保持 / 记录」类条目不是待办，是「勿重复调查」的档案，保留。
> 优先级：**P1** 尽快修 / **P2** 有窗口就做 / **P3** 可选·需决策（默认不做）。

| # | 事项 | 优先级 | 状态 |
|---|------|--------|------|
| T-4 | 食材库修正 → 旧菜谱营养回填（产品决策） | P3 | 待决策 |
| T-5 | refresh token（可选增强） | P3 | 待决策 |
| T-6 | token 明文存储 → 系统安全存储（暂不做） | P3 | 记录 |
| T-7 | 客户端「乐观登录」 | — | 已评估·保持 |
| T-8 | api-spec §9 版本历史表滞后 | P3 | 待做 |

## 一、建议修复

### T-8 api-spec §9「版本历史」表滞后于顶部修订记录 — P3（低成本）

**现状**：`docs/api-spec.md` 末尾 §9 版本历史表缺 v2.19–v2.22、v2.25、v2.26 等行——最近更新（v2.25 / v2.26 / 本次 v2.27）只维护顶部「修订记录」bullet 列表，两处口径并存。
**建议**：二选一——① 从顶部修订记录回填 §9 缺行；② 声明 §9 为历史归档、以顶部为唯一事实源（加注或移除）。倾向 ②（维护单点，符合「唯一事实源」原则）。
**备注**：本次 v2.27 沿用近两次实践，只更新顶部修订记录。

## 二、可选增强（需产品 / 技术决策，默认不做）

### T-4 食材库修正 → 旧菜谱营养回填工具 — P3（产品决策）

**现状**：营养为投稿 / 编辑时快照（快照语义见 Design D-06）；食材库没有运行时写接口（Router 无任何 ingredient / nutrition 增删改路由），修改的唯一途径是重跑 `server/sql/seed_ingredient_nutrition.sql`（`:42-44` 全量同步：先 DELETE 再 INSERT，只动 `ingredient_nutrition`，不碰 `recipes`）。因此改食材库后旧菜谱显示旧值——这是快照语义的必然结果，不是缺陷。

**若要传导修正**：需显式回填工具（SQL 脚本或管理端）：对目标菜谱调用 `buildNutritionInfo`（`RecipeServiceImpl.cpp:678`）重算并 UPDATE `nutrition_info`。是否希望历史快照被改写属产品语义决策，默认不做。

**备注**：**不要**实现「读取时实时计算」——违背 D-06 快照决策与 2.5 R1 契约（报告接口只读取）。

### T-5 引入 refresh token（长期自动登录 + 短 access token） — P3（可选增强）

**现状**：单 token 7 天（`UserServiceImpl.cpp:104`）；过期 → 服务端 401 → 客户端统一登出重登（`client/api/HttpGoCookApi.cpp:296-299` 发出 unauthorized 信号；`client/viewmodels/AuthViewModel.cpp:17-21` 已登录即 `logout()`）；无 `/api/refresh` 路由。

**评估**：属体验 / 安全增强而非缺陷修复；引入服务端状态（refresh 存储 + 轮换 + 重放检测）与客户端改造，复杂度高。当前「7 天单 token」是有意简化。

**备注**：若做，与既有 `token_version` 吊销机制（Design D-15 / api-spec v2.27）配套设计更顺（短 access + 服务端 refresh 存储天然支持吊销）。

### T-6 客户端 token 明文存 SQLite → 系统安全存储（QtKeychain） — P3（记录，暂不做）

**现状**：`user(id, username, token)` 表 token 为明文 `TEXT`（`client/database/LocalDatabase.cpp:129-132`），写入 `saveUser`（`:161`，INSERT 在 `:176`），读取 `getUser`（`:210`）；DB 位于 `QStandardPaths::AppDataLocation/gocook.db`。

**评估**：桌面应用依赖 OS 用户隔离属常规 trade-off（威胁模型 = 本机其他用户 / 恶意进程）；升级需引入 QtKeychain（Linux libsecret / Windows Credential Manager / macOS Keychain）及跨平台构建、CI、测试迁移成本。暂不做；若做：token 移出 SQLite，本地库仅保留非敏感数据。

## 三、已评估 · 保持现状（勿重复调查）

### T-7 客户端「乐观登录」：断网启动未经验证即进主界面 — 保持

`client/viewmodels/AuthViewModel.cpp:133-145`：网络 / 5xx 故障时**保留 token 乐观登录**（离线可读本地快照），仅服务端 401（明确拒绝）才清凭证（`:126-132`）；服务端各接口仍强制校验，联网后首个 401 经 unauthorizedHandler（`:17-21`）自动登出。属有意设计（离线体验），风险有限，保持现状；若未来收紧（如仅允许只读缓存态）再评估。

---

**已核、无需动作**：`.env`（`GOCOOK_JWT_SECRET` 来源）已被 `.gitignore:94` 覆盖、未入库（`git check-ignore -v .env` / `git ls-files .env` 验证，2026-09-28）。

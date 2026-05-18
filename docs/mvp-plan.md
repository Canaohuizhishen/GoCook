# GoCook MVP 开发计划

## 一、项目架构速览

四层结构，从上到下：

```
QML 页面 → ViewModel → HttpGoCookApi → [HTTP] → Handler → ServiceImpl → PgRepository → PostgreSQL
```

**组员只需填这两层**（其他层已完工）：

```
ServiceImpl.cpp  ——  业务逻辑（校验、数据转换、调用仓库）
PgRepository.cpp  ——  SQL 语句（pqxx 连接池查数据库）
```

**服务端每一条路由的调用链**：

```
Router.cpp  →  Handler  →  ServiceImpl  →  Repository
  ✅完成       ✅完成        ❌等你实现       ❌等你实现
```

**命名规律**：路由 `/api/recipes/:id` → `RecipeHandler::getRecipeDetail` → `RecipeServiceImpl::getRecipeDetail` → `PgRecipeRepository::findById` —— 方法名语义一致。

---

## 二、当前实现状态

| 状态 | 数量 | 端点 |
|------|------|------|
| **已实现 ✅** | 10 | 注册、登录、当前用户、公开菜谱列表、菜谱详情、菜谱搜索、投稿、库存增删查 |
| **待实现 ❌** | 59 | 其余全部在 Service/Repository 层抛出 501 "Not implemented" |
| **MVP 目标 🎯** | ~39 | 见下方分工表格 |

---

## 三、环境搭建

```sh
# 安装 Docker
sudo pacman -S docker
# 启动 Docker 服务并设置开机自启：
sudo systemctl enable --now docker

# 安装 C++ 客户端库，用于访问 PostgreSQL 数据库
sudo pacman -S libpqxx

# 安装 Docker Compose 用于定义和运行多容器 Docker 应用
sudo pacman -S docker-compose

# 1. 起数据库
docker compose -f server/pg-docker/docker-compose.yml up -d

# 2. 建表 + 灌测试数据
docker exec -i my_postgres psql -U gocook -d gocookdb < server/sql/create_all_tables.sql
docker exec -i my_postgres psql -U gocook -d gocookdb < server/sql/seed_test_data.sql

# 3. 运行配置脚本生成 .env 文件（写入随机 JWT 密钥）：
./server/setup.sh

# 4. 构建服务端 + 测试
cmake -B build -G Ninja -S server/ -DBUILD_TESTING=ON
cmake --build build --target server          # 仅构建服务端
cmake --build build --target test_services   # 构建测试
cmake --build build --target appclient       # 构建客户端

# 5. 跑测试
./server/run_tests.sh
```

---

## 四、关键目录速查

| 你要改的文件 | 路径 |
|-------------|------|
| 服务端 Service 实现 | `server/services/XxxServiceImpl.{h,cpp}` |
| 服务端 Repository 实现 | `server/repositories/PgXxxRepository.{h,cpp}` |
| Service 接口（只读参考） | `contracts/include/gocook/IServices.h` |
| 仓库接口（只读参考） | `contracts/include/gocook/I*Repository.h` |
| 数据模型定义（只读参考） | `contracts/include/gocook/DataModels.h` |
| 客户端 API 实现 | `client/HttpGoCookApi.{h,cpp}` |
| 客户端 API 接口（只读参考） | `contracts/include/gocook/IGoCookApi.h` |
| ViewModel | `client/viewmodels/XxxViewModel.{h,cpp}` |
| QML 页面 | `client/qml/pages/XxxPage.qml` |
| API 规范（只读参考） | `docs/api-spec.md` |

---

## 五、开发模板

### 5.1 Service 层（模板 A）

以已实现的 `getPublicRecipes` 为例（`RecipeServiceImpl.cpp:6-9`）：

```cpp
PagedRecipes RecipeServiceImpl::getPublicRecipes(int page, int size,
                                                  const nlohmann::json& filters) {
    return recipeRepo_->findPublicRecipes(page, size, filters);
}
```

模式：**校验 → 调用 xxxRepo_->方法名(...) → return**。

> **注意**：`RecipeServiceImpl.cpp:11` 的注释 `// 以下方法暂时未实现（骨架）` 当前覆盖了已实现的 `searchRecipes`（第 12-16 行），该注释已过时。开发中若方法已填充实现，应将此类注释下移至后续未实现方法之前。

### 5.2 Repository 层（模板 B）

以已实现的 `PgRecipeRepository::findPublicRecipes` 为例：

```cpp
PagedRecipes PgRecipeRepository::findPublicRecipes(int page, int size,
                                                    const nlohmann::json& filters) {
    auto conn = db_.getConn();                    // 1. 从连接池取连接
    pqxx::work txn(*conn);                        // 2. 开启事务
                                                  // 3. 写 SQL（$1/$2 防注入）
    pqxx::result r = txn.exec_params(
        "SELECT id, name, ... FROM recipes WHERE status = 'approved' "
        "ORDER BY created_at DESC LIMIT $1 OFFSET $2",
        size, (page - 1) * size);

    PagedRecipes result;                          // 4. 逐行赋值
    for (const auto& row : r) {
        RecipeSummary item;
        item.id = row["id"].as<int>();
        item.name = row["name"].as<std::string>();
        // ...
        result.data.push_back(std::move(item));
    }
    txn.commit();
    return result;
}
```

关键规则：
- 绝对不要拼字符串 SQL，永远用 `exec_params($1, $2, ...)`
- JSON 字段先取字符串再 `nlohmann::json::parse()` 解析
- DataModels 结构体字段对照 `contracts/include/gocook/DataModels.h`

### 5.3 客户端 API 对接（模板 C）

```cpp
void HttpGoCookApi::getInventory(int page, int size,
    std::function<void(const QVariantList&, const QVariantMap&)> callback,
    std::function<void(const QString&)> onError) {
    QString path = QString("/api/inventory?page=%1&size=%2").arg(page).arg(size);
    sendGetRequest(path, [callback, onError](const QJsonDocument& doc) {
        QJsonObject obj = doc.object();
        QVariantList data = obj["data"].toArray().toVariantList();
        QVariantMap pagination = obj["pagination"].toObject().toVariantMap();
        callback(data, pagination);
    }, onError);
}
```

### 5.4 ViewModel 对接（模板 D）

```cpp
// 头文件（Q_PROPERTY + Q_INVOKABLE）
Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)

Q_INVOKABLE void loadInventory();

// 实现
void RecipeViewModel::loadPublicRecipes() {
    api_->getPublicRecipes(m_page, m_size, {},
        [self = QPointer<RecipeViewModel>(this)]
        (const QVariantList& data, const QVariantMap& pagination) {
            if (!self) return;     // QPointer 守卫防止 use-after-free
            // ... 处理数据
        },
        [self = QPointer<RecipeViewModel>(this)](const QString& err) {
            if (!self) return;
            self->m_error = err;
            emit self->errorOccurred(err);
        });
}
```

### 5.5 单元测试（模板 E）

参考已有 40 个用例：

```cpp
TEST(RecipeServiceTest, 搜索菜谱按关键字) {
    auto mock = std::make_unique<NiceMock<MockRecipeRepository>>();
    auto* repo = mock.get();
    RecipeServiceImpl service(std::move(mock));

    PagedRecipes fakeResult;
    fakeResult.data.push_back({1, "红烧肉", ...});
    EXPECT_CALL(*repo, searchRecipes("红烧", 1, 20, _))
        .WillOnce(Return(fakeResult));

    auto result = service.searchRecipes("红烧", 1, 20, {});
    EXPECT_EQ(result.data.size(), 1);
}
```

---

## 六、每个端点的开发流程

```
1. 读 api-spec.md → 确认请求参数和返回格式
2. 读 DataModels.h → 找到对应的 C++ 结构体
3. 读 I*Repository.h → 确认仓库接口签名（方法名 + 参数类型）
4. 找 Pg*Repository.cpp → 已经有方法骨架，填 SQL 实现
5. 找 *ServiceImpl.cpp → 已经有方法骨架，填委托调用
6. 编译：cmake --build build --target server
7. 启动服务端 · 手工 curl 验证
8. 跑测试：./server/run_tests.sh（先看已有测试是否通过，再补测试）
9. 对接客户端：HttpGoCookApi.cpp + ViewModel + QML
```

---

## 七、分工表格（定稿）

### 组长 · 菜谱域（11 个端点）

| 端点 | HTTP | 说明 |
|------|------|------|
| `GET /api/recipes/recommend` | ✅ Router 已注册 | 智能推荐（含用户偏好） |
| `GET /api/recipes/:id/nutrition` | ✅ Router 已注册 | 营养报告 |
| `GET /api/recipes/:id/videos` | ✅ Router 已注册 | 关联视频 |
| `GET /api/recipes/:id/ratings` | ✅ Router 已注册 | 评分与评论列表 |
| `POST /api/recipes` | ✅ 已实现 | 投稿新菜谱 |
| `GET /api/recipes/my` | ✅ Router 已注册 | 我的投稿列表 |
| `PUT /api/recipes/:id` | ✅ Router 已注册 | 编辑未审核菜谱 |
| `POST /api/recipes/:id/favorite` | ✅ Router 已注册 | 收藏切换（含客户端 QML 挂接） |
| `POST /api/recipes/:id/rate` | ✅ Router 已注册 | 评分提交（含客户端 QML 挂接） |
| `PUT /api/recipes/:id/ratings/:rid` | ✅ Router 已注册 | 修改评论 |
| `DELETE /api/recipes/:id/ratings/:rid` | ✅ Router 已注册 | 删除评论 |
| `GET /api/users/me/ratings` | ✅ Router 已注册 | 我的评论列表 |

**服务端**：`RecipeServiceImpl.{h,cpp}`（填 11 个 501 骨架）+ `PgRecipeRepository.{h,cpp}`（填 11 个 501 骨架）

**客户端**：
- `HttpGoCookApi.{h,cpp}` —— 对接上述接口
- `RecipeViewModel.{h,cpp}` —— 补推荐/评分方法
- `RecipeDetailPage.qml` —— 加评分评论 UI 区域 + 挂接 rateRecipe
- `RecommendPage.qml` —— 挂接收藏切换 toggleFavorite

**单元测试**：`test_recipe_service.cpp` 补推荐/营养/评分/收藏测试

---

### 组员 B · 用户域（20 个端点）

| 端点 | HTTP | 说明 |
|------|------|------|
| `PUT /api/users/me/profile` | ✅ Router 已注册 | 更新个人资料 |
| `POST /api/users/me/avatar` | ✅ Handler 已实现 | 头像上传 |
| `PUT /api/users/me/password` | ✅ Router 已注册 | 修改密码 |
| `DELETE /api/users/me` | ✅ Router 已注册 | 注销账户 |
| `GET /api/users/me/preferences` | ✅ Router 已注册 | 获取饮食偏好 |
| `PUT /api/users/me/preferences` | ✅ Router 已注册 | 更新饮食偏好 |
| `PUT /api/users/me/health-profile` | ✅ Router 已注册 | 录入健康指标 |
| `GET /api/users/me/favorites` | ✅ Router 已注册 | 收藏列表 |
| `GET /api/users/me/favorites/groups` | ✅ Router 已注册 | 收藏分组列表 |
| `POST /api/users/me/favorites/groups` | ✅ Router 已注册 | 创建收藏分组 |
| `PUT /api/users/me/favorites/groups/:id` | ✅ Router 已注册 | 更新分组名称 |
| `DELETE /api/users/me/favorites/groups/:id` | ✅ Router 已注册 | 删除分组 |
| `PATCH /api/users/me/favorites/:id` | ✅ Router 已注册 | 更新收藏项属性 |
| `DELETE /api/users/me/favorites/batch` | ✅ Router 已注册 | 批量删除收藏 |
| `GET /api/users/me/notifications` | ✅ Router 已注册 | 通知列表 |
| `PATCH /api/users/me/notifications/:id/read` | ✅ Router 已注册 | 标记已读 |
| `PUT /api/users/me/notifications/read-all` | ✅ Router 已注册 | 全部标记已读 |
| `DELETE /api/users/me/notifications/:id` | ✅ Router 已注册 | 删除通知 |
| `POST /api/password/forgot` | ✅ Router 已注册 | 忘记密码（发送重置邮件） |
| `POST /api/password/reset` | ✅ Router 已注册 | 重置密码 |

**服务端**：`UserServiceImpl.{h,cpp}`（从第 113 行起的骨架全填实现）+ `PgUserRepository.{h,cpp}`（从骨架全填实现）

**客户端**：
- `HttpGoCookApi.{h,cpp}` —— 对接上述接口
- 新建 `ProfileEditPage.qml` —— 个人资料编辑
- 新建 `PreferencesPage.qml` —— 饮食偏好设置
- 改造 `FavoritesPage.qml` —— 收藏列表与分页（文件已有，需补 ViewModel）
- `ProfilePage.qml` —— 添加入口按钮
- `LoginPage.qml` —— 添加忘记密码入口

**单元测试**：`test_user_service.cpp` 补 updateProfile/changePassword/getFavorites 等测试

---

### 组员 C · 库存与公告域（8 个端点）

| 端点 | HTTP | 说明 |
|------|------|------|
| `GET /api/inventory/shopping-lists` | ✅ Router 已注册 | 获取购物清单列表 |
| `POST /api/inventory/shopping-lists` | ✅ Router 已注册 | 创建购物清单 |
| `GET /api/inventory/shopping-lists/:id` | ✅ Router 已注册 | 购物清单详情 |
| `DELETE /api/inventory/shopping-lists/:id` | ✅ Router 已注册 | 删除购物清单 |
| `PATCH /api/inventory/shopping-lists/:lid/items/:iid` | ✅ Router 已注册 | 更新清单项状态（勾选后自动同步库存） |
| `POST /api/inventory/shopping-lists/:id/items/batch` | ✅ Router 已注册 | 批量添加清单项 |
| `GET /api/inventory/shopping-lists/:id/export` | ✅ Router 已注册 | 导出购物清单（纯文本 / PNG 图片） |
| `GET /api/announcements` | ✅ Router 已注册 | 获取系统公告列表（公开，无需认证） |

**服务端**：
- `InventoryServiceImpl.{h,cpp}`（填 7 个购物清单 501 骨架）
- `PgInventoryRepository.{h,cpp}`（填 7 个购物清单 SQL）
- `AnnouncementServiceImpl.{h,cpp}`（填 1 个 501 骨架）
- `PgAnnouncementRepository.{h,cpp}`（填 1 个 SQL）

**客户端**：
- `HttpGoCookApi.{h,cpp}` —— 对接购物清单 + 公告接口
- 新建 `ShoppingListPage.qml` —— 购物清单管理页面

**单元测试**：`test_inventory_service.cpp` 补购物清单测试 + 公告测试

---

## 八、注意事项

### 几个风险点

- **组员 B 的 20 个端点是三人中最多的**。建议 B 优先实现服务端核心端点（Profile/Password 等认证相关），客户端界面可复用 `CustomButton`、`RecipeCard`、`LoadingIndicator` 等已有组件，不追求 UI 精致程度
- **购物清单导出接口格式已确认**：api-spec.md §5.7 明确 `?format=text` 返回 `text/plain`，`?format=image` 返回 `image/png`
- **数据库表结构必须验证**：建表脚本在 `server/sql/create_all_tables.sql`，开发前对照 `contracts/include/gocook/DataModels.h` 确认每个字段齐全
- **SQL 注入红线**：`PgRecipeRepository.cpp:196-201` 的关键字 ILIKE 使用了 `txn.esc()` 字符串拼接，各组员写 SQL 时必须严格用 `pqxx::work::exec_params($1, $2, ...)` 参数化查询，禁止任何字符串拼接

---

## 九、分支策略与 Git 工作流

### 9.1 分支创建

三人各自从 `dev` 拉独立分支，并行开发：

```sh
# 组长 — 菜谱扩展
git checkout -b feat/recipe-extend

# 组员 B — 用户高级 + 收藏
git checkout -b feat/user-advanced

# 组员 C — 购物清单 + 菜谱互动
git checkout -b feat/shopping-list
```

### 9.2 分支命名规范

```
feat/<module-name>    # 新功能
fix/<bug-name>        # Bug 修复（联调阶段使用）
```

### 9.3 提交规范

每人开发完一个端点后，自测通过即提交，不等待所有端点完成。提交信息格式：

```
feat(server): 实现菜谱搜索 Service + Repository
feat(client): 对接搜索 API + SearchPage QML
```

### 9.4 合并到 dev 的时机

- **只通过 Pull Request / Merge Request 合入 dev**
- 每次合入前必须确保 `./server/run_tests.sh` 全部通过
- 注意：联调阶段才频繁合并，开发阶段各分支独立推进

---

## 十、开发规范

1. **开发一律开新分支**，见§9 分支策略，禁止直接推主分支
2. **每次提交前跑测试** `./server/run_tests.sh`，确保不破坏已有用例
3. **C++ 23 标准**，不引入新的第三方依赖
4. **QML 命名**：文件 `PascalCase.qml`，属性 `camelCase`，id 与文件名一致
5. **客户端 ViewModel 异步回调**：必须在 lambda 开头加 `QPointer` 守卫
6. **服务端 Repository SQL**：永远用 `pqxx::work::exec_params` 参数化查询，禁止拼接字符串

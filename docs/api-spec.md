# GoCook 服务端 API 契约文档 (修订版)

> **版本**：**v2.0**
> **最后更新**：2026-04-22
> **说明**：本文档定义了 GoCook 客户端与服务端之间的 RESTful API 契约。所有开发人员（客户端/服务端）必须严格遵守此规范，保证请求/响应格式、状态码语义的一致性。
> **修订记录**：
> - **v2.0**：**版权校验机制明确化**：根据需求对齐反馈，在菜谱投稿接口中明确版权校验为服务端内部自动执行逻辑，不暴露额外 API 端点；在审核拒绝接口及投稿列表响应中补充版权相关的拒绝原因示例，保证与需求文档中“系统自动进行版权校验”的表述一致。
> - **v1.9**：**营养趋势对比增强**：为 `GET /api/meal-plans/nutrition-trend` 接口响应增加 `recommended` 和 `comparison` 字段，支持展示实际摄入量与推荐摄入量的直观对比，提升健康管理闭环体验。
> - **v1.8**：**需求对齐与接口增强**：为菜谱列表/搜索接口增加明确的筛选查询参数；在购物清单接口响应中增加库存对比字段；在文档中增加烹饪进度保存、营养目标对比等未来迭代备注。
> - **v1.7**：**后台管理功能补全**：新增向指定用户发送通知的接口 (`POST /api/admin/notifications`)；新增菜谱批量审核接口 (`POST /api/admin/recipes/batch-review`)。
> - **v1.6**：**规范化增强**：系统公告接口调整为统一分页格式，保持全系统响应结构一致；在通用约定中明确定义 JWT Token 有效期为 7 天；在库存管理部分增加关于单位标准化的未来迭代备注。
> - **v1.5**：**接口优化增强**：智能推荐接口返回增加 `match_score` 匹配度字段；膳食计划日历视图 `daily_total` 补全 `fat`、`carbs` 字段；健康指标接口忌口建议改为结构化对象，包含原因说明。
> - **v1.4**：**需求对齐补缺**：增加用户个人资料更新接口；智能推荐接口返回的缺失食材增加建议用量与单位，支持一键加入购物车。
> - **v1.3**：菜谱详情增加步骤计时字段；增加菜谱视频关联接口；明确膳食计划餐别枚举值与评分范围。
> - **v1.2**：补全后台用户管理接口；增加收藏列表、膳食计划详情与更新接口；增强营养汇总与购物车批量操作能力；明确分享与运维功能的实现边界。
> - **v1.1**：根据需求对齐评审结果修订：补充公告/评论接口，统一分页格式，重构库存删除为ID方式。
> - **v1.0**：初始版本，定义最小可行子集。

---

### 1. 通用约定

#### 1.1 基础信息
- **基础 URL**：`http://127.0.0.1:8080`（开发环境）
- **数据格式**：`application/json`
- **字符编码**：`UTF-8`

#### 1.2 认证方式与 Token 策略
需要认证的接口必须在请求头中携带 Bearer Token：
```
Authorization: Bearer <token>
```
Token 通过登录接口获取。

**Token 有效期约定：**
- **有效期**：Token 自颁发起 **7 天（168小时）** 后失效。
- **刷新机制**：当前版本采用单 Token 机制。客户端应在 Token 过期前引导用户重新登录，或由服务端在后续版本中引入 Refresh Token 机制以提供更流畅的体验。

#### 1.3 响应状态码语义
| 状态码 | 含义 |
|--------|------|
| 200 | 请求成功 |
| 201 | 资源创建成功 |
| 400 | 请求参数错误 |
| 401 | 未认证或 Token 无效/过期 |
| 403 | 权限不足 |
| 404 | 资源不存在 |
| 409 | 资源冲突（如用户名已存在） |
| 500 | 服务器内部错误 |

#### 1.4 错误响应格式
当发生 4xx/5xx 错误时，响应体统一采用以下 JSON 格式：
```json
{
  "error": "Human readable error message"
}
```

#### 1.5 分页约定（强制统一）
对于所有列表类接口，**必须**使用以下查询参数：
- `page`：页码，从 1 开始，默认 1
- `size`：每页数量，默认 20，最大 100

**所有列表接口的响应体格式必须严格遵守以下结构：**
```json
{
  "data": [ /* 数据数组 */ ],
  "pagination": {
    "page": 1,
    "size": 20,
    "total": 100,
    "total_pages": 5
  }
}
```

---

### 2. 用户认证相关（公开）

#### 2.1 用户注册
- **方法**：`POST`
- **路径**：`/api/register`
- **请求体**：
```json
{
  "username": "testuser",
  "password": "123456"
}
```
- **成功响应**：
  - 状态码：`201 Created`
  - 响应体：
```json
{
  "message": "User registered successfully"
}
```
- **失败响应**：
  - `400`：用户名或密码为空
  - `409`：用户名已存在

#### 2.2 用户登录
- **方法**：`POST`
- **路径**：`/api/login`
- **请求体**：
```json
{
  "username": "testuser",
  "password": "123456"
}
```
- **成功响应**：
  - 状态码：`200 OK`
  - 响应体：
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "user_id": 1,
  "username": "testuser"
}
```
- **失败响应**：
  - `400`：参数缺失
  - `401`：用户名或密码错误

---

### 3. 用户相关

#### 3.1 获取当前用户信息（需认证）
- **方法**：`GET`
- **路径**：`/api/users/me`
- **成功响应**：
```json
{
  "id": 1,
  "username": "testuser",
  "email": "test@example.com",
  "avatar_url": "https://...",
  "created_at": "2026-04-01T12:00:00Z"
}
```

#### 3.2 更新当前用户个人资料（需认证）【v1.4 新增】
- **方法**：`PUT`
- **路径**：`/api/users/me/profile`
- **说明**：对齐需求 3.3 中“管理个人资料（昵称、头像、联系方式）”功能。
- **请求体**：
```json
{
  "username": "new_nickname",
  "avatar_url": "https://example.com/avatar.jpg",
  "email": "new_email@example.com",
  "phone": "13800138000"
}
```
- **字段说明**：所有字段均为可选，仅更新传入的非空字段。`username` 为用户昵称（显示名），`email` 和 `phone` 用于联系方式。
- **成功响应**：`200 OK`
- **响应体**：返回更新后的用户信息（同 3.1 结构）
- **失败响应**：
  - `400`：邮箱或手机格式不正确
  - `409`：昵称或邮箱已被占用

#### 3.3 获取用户收藏列表（需认证）
- **方法**：`GET`
- **路径**：`/api/users/me/favorites`
- **查询参数**：`?page=1&size=20`
- **成功响应**：**统一分页格式**
```json
{
  "data": [
    {
      "id": 1,
      "name": "番茄炒蛋",
      "description": "经典家常菜",
      "image_url": "https://...",
      "favorited_at": "2026-04-21T18:30:00Z"
    }
  ],
  "pagination": { ... }
}
```

#### 3.4 更新用户饮食偏好（需认证）
- **方法**：`PUT`
- **路径**：`/api/users/me/preferences`
- **请求体**：
```json
{
  "likes": ["中式", "清淡"],
  "dislikes": ["香菜"],
  "allergies": ["花生"],
  "health_goal": "高蛋白"
}
```
- **成功响应**：`200 OK`

#### 3.5 获取用户饮食偏好（需认证）
- **方法**：`GET`
- **路径**：`/api/users/me/preferences`
- **成功响应**：同 3.4 请求体结构

#### 3.6 录入/更新健康指标（需认证）
- **方法**：`PUT`
- **路径**：`/api/users/me/health-profile`
- **说明**：对应需求中的身体指标分析功能。
- **请求体**：
```json
{
  "height_cm": 175,
  "weight_kg": 70,
  "conditions": ["高血压"]
}
```
- **成功响应**：`200 OK`
- **响应体**：返回系统生成的忌口建议（**v1.5 增强为结构化对象**）
```json
{
  "suggested_avoidances": [
    {
      "ingredient": "高钠食物",
      "reason": "高血压患者应限制钠摄入"
    },
    {
      "ingredient": "动物内脏",
      "reason": "含较高胆固醇，不利于血压控制"
    }
  ]
}
```

#### 3.7 获取系统公告列表（公开）【v1.6 修订】
- **方法**：`GET`
- **路径**：`/api/announcements`
- **查询参数**：`?page=1&size=5`（默认 `size` 为 5，保持与原行为一致）
- **认证**：不需要
- **成功响应**：`200 OK`，**现已采用统一分页格式**
```json
{
  "data": [
    {
      "id": 1,
      "title": "系统维护通知",
      "content": "今晚 22:00-24:00 进行系统升级，届时服务不可用。",
      "created_at": "2026-04-21T10:00:00Z"
    }
  ],
  "pagination": {
    "page": 1,
    "size": 5,
    "total": 1,
    "total_pages": 1
  }
}
```

---

### 4. 菜谱相关

#### 4.1 获取公开菜谱列表（分页）【v1.8 增强筛选参数】
- **方法**：`GET`
- **路径**：`/api/recipes/public`
- **查询参数**：
  - `?page=1&size=20` (分页)
  - `cuisine`: 菜系，如 "中式"、"西式"
  - `meal_type`: 菜品类型，如 "主食"、"汤"
  - `difficulty`: 难易程度，如 "简单"、"中等"、"困难"
  - `max_time`: 最大烹饪时长（分钟），如 `30`
  - `tags`: 标签，如 "快手"、"高蛋白"（可传递多个）
- **认证**：不需要
- **成功响应**：**已统一为分页格式**
```json
{
  "data": [
    {
      "id": 1,
      "name": "番茄炒蛋",
      "description": "经典家常菜",
      "image_url": "https://...",
      "prep_time_minutes": 5,
      "cook_time_minutes": 10,
      "tags": ["中式", "快手"],
      "author_id": 1,
      "author_name": "testuser"
    }
  ],
  "pagination": { ... }
}
```

#### 4.2 智能推荐菜谱（需认证）【v1.5 增强】
- **方法**：`GET`
- **路径**：`/api/recipes/recommend`
- **说明**：根据用户偏好与库存自动推荐。**缺失食材已包含建议用量与单位，便于一键批量加入购物车。v1.5 新增 match_score 字段表示整体匹配度。**
- **成功响应**：
```json
{
  "data": [
    {
      "id": 1,
      "name": "番茄炒蛋",
      "match_score": 0.92,
      "match_status": {
        "available_ingredients": [
          { "name": "鸡蛋", "quantity": 2, "unit": "个" },
          { "name": "番茄", "quantity": 1, "unit": "个" }
        ],
        "missing_ingredients": [
          { "name": "葱花", "suggested_quantity": 1, "unit": "把" }
        ]
      }
    }
  ],
  "pagination": { ... }
}
```
- **字段说明**：
  - `match_score`：菜谱与用户库存、偏好的综合匹配度，取值范围 0.0 ~ 1.0，保留两位小数。
  - `available_ingredients`：用户库存中已有的匹配食材，包含实际库存数量。
  - `missing_ingredients`：制作该菜谱尚缺的食材，包含**建议用量**和单位，客户端可直接调用批量添加购物车接口（5.6）加入。

#### 4.3 关键词搜索菜谱（分页）【v1.8 增强筛选参数】
- **方法**：`GET`
- **路径**：`/api/recipes/search`
- **查询参数**：
  - `?keyword=番茄&page=1&size=20` (基础搜索)
  - `cuisine`: 菜系，如 "中式"、"西式"
  - `meal_type`: 菜品类型，如 "主食"、"汤"
  - `difficulty`: 难易程度，如 "简单"、"中等"、"困难"
  - `max_time`: 最大烹饪时长（分钟），如 `30`
  - `tags`: 标签，如 "快手"、"高蛋白"（可传递多个）
- **认证**：可选
- **成功响应**：**统一分页格式**，数组项结构同 4.1

#### 4.4 获取菜谱详情
- **方法**：`GET`
- **路径**：`/api/recipes/:id`
- **认证**：可选
- **成功响应**：包含完整步骤、食材用量、图片及营养信息。步骤中包含 `duration` 字段（单位：秒）支持烹饪计时。
```json
{
  "id": 1,
  "name": "番茄炒蛋",
  "description": "经典家常菜",
  "image_url": "https://...",
  "prep_time_minutes": 5,
  "cook_time_minutes": 10,
  "ingredients": [
    { "name": "鸡蛋", "quantity": 2, "unit": "个" },
    { "name": "番茄", "quantity": 1, "unit": "个" }
  ],
  "steps": [
    { "order": 1, "description": "鸡蛋打散，加少许盐搅拌均匀。", "duration": 60 },
    { "order": 2, "description": "番茄切块备用。", "duration": 120 },
    { "order": 3, "description": "热锅放油，倒入蛋液炒熟盛出。", "duration": 180 },
    { "order": 4, "description": "锅中留底油，放入番茄炒出汁，加入鸡蛋翻炒均匀。", "duration": 240 }
  ],
  "nutrition": {
    "calories": 350,
    "protein": 15,
    "fat": 20,
    "carbs": 30
  },
  "tags": ["中式", "快手"],
  "author_id": 1,
  "author_name": "testuser",
  "created_at": "2026-04-21T10:00:00Z"
}
```

#### 4.5 获取菜谱关联视频列表
- **方法**：`GET`
- **路径**：`/api/recipes/:id/videos`
- **认证**：可选
- **成功响应**：
```json
[
  {
    "id": 201,
    "title": "大厨教你做番茄炒蛋",
    "platform": "youtube",
    "url": "https://www.youtube.com/watch?v=xxxx",
    "thumbnail_url": "https://img.youtube.com/vi/xxxx/hqdefault.jpg",
    "duration_seconds": 245
  }
]
```

#### 4.6 获取菜谱的评分与评论列表（分页）
- **方法**：`GET`
- **路径**：`/api/recipes/:id/ratings`
- **查询参数**：`?page=1&size=10`
- **认证**：可选
- **成功响应**：
```json
{
  "data": [
    {
      "id": 101,
      "user_id": 5,
      "username": "foodie_lily",
      "rating": 5,
      "comment": "简单易做，味道好极了！",
      "created_at": "2026-04-20T18:30:00Z"
    }
  ],
  "pagination": { ... }
}
```

#### 4.7 投稿新菜谱（需认证）【v2.0 版权校验说明强化】
- **方法**：`POST`
- **路径**：`/api/recipes`
- **说明**：菜谱投稿者提交原创菜谱。**版权校验为服务端内部自动执行的逻辑，不对外暴露独立 API。** 服务端在接收到投稿请求后，会调用内部版权检测服务（如图像指纹比对、文本相似度分析）对提交内容进行自动审查。若检测到疑似侵权，菜谱状态将直接标记为 `rejected`，并在后续查看投稿状态时返回相应拒绝原因。
- **请求体**：包含名称、食材、步骤、图片等。
- **成功响应**：`201 Created`
```json
{
  "id": 123,
  "status": "pending"
}
```
- **字段说明**：
  - `status`：初始状态为 `pending`（待审核）。若自动版权校验未通过，服务端内部将状态更新为 `rejected`，但投稿接口本身仍返回 `201 Created`，仅表示请求已被接收。

#### 4.8 获取我的投稿列表（分页，需认证）【v2.0 增强拒绝原因示例】
- **方法**：`GET`
- **路径**：`/api/recipes/my`
- **查询参数**：`?page=1&size=20`
- **成功响应**：**统一分页格式**，数据项中包含拒绝原因字段。
```json
{
  "data": [
    {
      "id": 123,
      "name": "清炒西兰花",
      "status": "rejected",
      "reject_reason": "图片不清晰，请重新上传",
      "submitted_at": "2026-04-21T10:00:00Z"
    },
    {
      "id": 124,
      "name": "红烧肉",
      "status": "rejected",
      "reject_reason": "疑似侵权：与已发布菜谱'外婆红烧肉'相似度过高",
      "submitted_at": "2026-04-21T11:00:00Z"
    }
  ],
  "pagination": { ... }
}
```
- **字段说明**：
  - `reject_reason`：当 `status` 为 `rejected` 时返回拒绝原因，可能包含审核员填写的反馈或系统自动生成的版权检测结果（如“疑似侵权”）。

#### 4.9 编辑未审核的菜谱（需认证）
- **方法**：`PUT`
- **路径**：`/api/recipes/:id`
- **请求体**：同投稿
- **成功响应**：`200 OK`

#### 4.10 收藏/取消收藏菜谱（需认证）
- **方法**：`POST`
- **路径**：`/api/recipes/:id/favorite`
- **成功响应**：`200 OK`

#### 4.11 评分与评论（需认证）
- **方法**：`POST`
- **路径**：`/api/recipes/:id/rate`
- **请求体**：
```json
{
  "rating": 5,
  "comment": "味道很棒，下次还会做！"
}
```
- **字段约束**：`rating` 为 **1 到 5 的整数**。
- **成功响应**：`201 Created`
- **失败响应**：
  - `400`：评分不在 1-5 范围内或参数缺失

#### 4.12 关于分享功能的说明
- **实现策略**：菜谱分享功能由客户端直接生成链接（如 `https://gocook.com/recipe/123`）并调用系统分享能力实现，**服务端无需提供专门的分享 API**。

#### 4.13 关于烹饪进度保存的说明【v1.8 新增备注】
> **【v1.8 设计备注 - 烹饪进度保存】**
> 需求用例 1.c 备选流提到“用户中途退出烹饪模式，系统保存当前进度，下次可继续”。当前版本中，烹饪计时功能由客户端基于菜谱详情中的 `steps[].duration` 字段自行实现，进度保存也建议优先使用客户端本地存储（如 SQLite）。
> **未来迭代方向**：若需实现跨设备同步烹饪进度（如手机开始烹饪，平板继续），可考虑增加以下接口：
> - `POST /api/recipes/:id/cooking-progress` - 保存当前步骤索引与计时器状态
> - `GET /api/recipes/:id/cooking-progress` - 获取上次进度
> 当前版本暂不实现此服务端接口。

---

### 5. 库存管理（需认证）

#### 5.1 获取当前用户库存（分页）
- **方法**：`GET`
- **路径**：`/api/inventory`
- **查询参数**：`?page=1&size=50`
- **成功响应**：**统一分页格式**
```json
{
  "data": [
    {
      "id": 501,
      "ingredient_name": "鸡蛋",
      "quantity": 6,
      "unit": "个",
      "expiry_date": "2026-04-20",
      "added_at": "2026-04-10T12:00:00Z"
    }
  ],
  "pagination": { ... }
}
```

#### 5.2 添加/更新库存项
- **方法**：`POST`
- **路径**：`/api/inventory`
- **请求体**：
```json
{
  "ingredient_name": "鸡蛋",
  "quantity": 6,
  "unit": "个",
  "expiry_date": "2026-04-20"
}
```
- **成功响应**：`200 OK`，返回包含 `id` 的对象。

> **【v1.6 设计备注 - 单位标准化】**
> 当前版本中，食材单位 (`unit`) 为自由文本，便于用户灵活输入（如“个”、“把”、“颗”、“袋”）。但此举会导致购物清单合并计算时的困难（例如无法将“1把葱花”与“1把香菜”合并为“2把叶菜”）。
> **未来迭代方向**：建议引入一个食材-标准单位映射表，在后台将用户自由输入的单位映射为标准枚举（如 `weight_g`， `volume_ml`， `count`），以支持精确的购物汇总和营养计算。当前客户端开发者可暂不考虑此复杂逻辑。

#### 5.3 删除库存项
- **方法**：`DELETE`
- **路径**：`/api/inventory/:item_id`
- **成功响应**：`200 OK`

#### 5.4 获取购物清单【v1.8 增强响应结构】
- **方法**：`GET`
- **路径**：`/api/inventory/shopping-list`
- **查询参数**：`?plan_id=xxx`（可选，传入膳食计划ID以基于计划生成清单）
- **成功响应**：响应中明确展示与库存对比后的待购数量。
```json
{
  "id": 801,
  "items": [
    {
      "id": 9001,
      "ingredient_name": "西兰花",
      "required_quantity": 2,
      "inventory_quantity": 0,
      "to_buy_quantity": 2,
      "unit": "颗",
      "checked": false
    },
    {
      "id": 9002,
      "ingredient_name": "鸡蛋",
      "required_quantity": 4,
      "inventory_quantity": 2,
      "to_buy_quantity": 2,
      "unit": "个",
      "checked": false
    }
  ]
}
```
- **字段说明**：
  - `required_quantity`: 膳食计划/菜谱总共需要的食材数量。
  - `inventory_quantity`: 用户库存中当前可用的数量。
  - `to_buy_quantity`: 建议购买的数量 (`required_quantity` - `inventory_quantity`)，最小值为 0。

#### 5.5 更新购物清单项状态
- **方法**：`PATCH`
- **路径**：`/api/inventory/shopping-list/items/:item_id`
- **请求体**：
```json
{
  "checked": true
}
```

#### 5.6 批量添加购物清单项
- **方法**：`POST`
- **路径**：`/api/inventory/shopping-list/items/batch`
- **说明**：用于智能推荐场景，将缺失食材一键批量加入购物清单。现在可直接使用推荐接口返回的 `missing_ingredients` 数组。
- **请求体**：
```json
[
  {
    "ingredient_name": "葱花",
    "quantity": 1,
    "unit": "把"
  },
  {
    "ingredient_name": "盐",
    "quantity": 1,
    "unit": "袋"
  }
]
```
- **成功响应**：`201 Created`
```json
{
  "message": "Successfully added 2 items",
  "items": [
    { "id": 9002, "ingredient_name": "葱花", "quantity": 1, "unit": "把", "checked": false },
    { "id": 9003, "ingredient_name": "盐", "quantity": 1, "unit": "袋", "checked": false }
  ]
}
```

---

### 6. 膳食计划（需认证）

#### 6.1 创建膳食计划项
- **方法**：`POST`
- **路径**：`/api/meal-plans`
- **请求体**：
```json
{
  "recipe_id": 1,
  "date": "2026-04-22",
  "meal_type": "dinner"
}
```
- **`meal_type` 枚举值**：`breakfast`、`lunch`、`dinner`、`snack`

#### 6.2 获取膳食计划（分页）
- **方法**：`GET`
- **路径**：`/api/meal-plans`
- **查询参数**：`?start_date=2026-04-21&end_date=2026-04-27&page=1&size=30`
- **成功响应**：**统一分页格式，包含营养汇总**。
```json
{
  "data": [ /* 计划项数组 */ ],
  "nutrition_summary": {
    "total_calories": 4500,
    "avg_protein": 60
  },
  "pagination": { ... }
}
```

#### 6.3 获取膳食计划详情（日历视图）【v1.5 增强】
- **方法**：`GET`
- **路径**：`/api/meal-plans/detail`
- **查询参数**：`?start_date=2026-04-21&end_date=2026-04-27`
- **成功响应**：
```json
{
  "days": [
    {
      "date": "2026-04-21",
      "meals": {
        "breakfast": {
          "plan_id": 101,
          "recipe": { /* 菜谱简略信息 */ },
          "nutrition": { "calories": 450, "protein": 15, "fat": 12, "carbs": 60 }
        },
        "lunch": null,
        "dinner": {
          "plan_id": 102,
          "recipe": { /* 菜谱简略信息 */ },
          "nutrition": { "calories": 800, "protein": 40, "fat": 35, "carbs": 80 }
        }
      },
      "daily_total": { "calories": 1250, "protein": 55, "fat": 47, "carbs": 140 }
    }
  ]
}
```
- **字段说明**：`daily_total` 现包含 `fat` 与 `carbs`，与营养趋势接口字段保持一致。

#### 6.4 更新膳食计划项
- **方法**：`PUT`
- **路径**：`/api/meal-plans/:id`
- **请求体**：
```json
{
  "recipe_id": 2,
  "date": "2026-04-23",
  "meal_type": "lunch"
}
```
- **成功响应**：`200 OK`

#### 6.5 删除膳食计划项
- **方法**：`DELETE`
- **路径**：`/api/meal-plans/:id`
- **成功响应**：`200 OK`

#### 6.6 获取营养摄入趋势【v1.9 增强对比功能】
- **方法**：`GET`
- **路径**：`/api/meal-plans/nutrition-trend`
- **查询参数**：`?start_date=2026-04-21&end_date=2026-04-27`
- **说明**：返回指定时间范围内的每日实际摄入量，并与基于用户健康档案计算出的每日推荐摄入量进行对比。
- **成功响应**：
```json
{
  "trend": [
    {
      "date": "2026-04-21",
      "actual": {
        "calories": 1250,
        "protein": 55,
        "fat": 47,
        "carbs": 140
      },
      "recommended": {
        "calories": 2000,
        "protein": 60,
        "fat": 65,
        "carbs": 250
      },
      "comparison": {
        "calories": { "diff": -750, "percentage": 62.5 },
        "protein": { "diff": -5, "percentage": 91.7 },
        "fat": { "diff": -18, "percentage": 72.3 },
        "carbs": { "diff": -110, "percentage": 56.0 }
      }
    },
    {
      "date": "2026-04-22",
      "actual": {
        "calories": 1400,
        "protein": 65,
        "fat": 50,
        "carbs": 160
      },
      "recommended": {
        "calories": 2000,
        "protein": 60,
        "fat": 65,
        "carbs": 250
      },
      "comparison": {
        "calories": { "diff": -600, "percentage": 70.0 },
        "protein": { "diff": 5, "percentage": 108.3 },
        "fat": { "diff": -15, "percentage": 76.9 },
        "carbs": { "diff": -90, "percentage": 64.0 }
      }
    }
  ],
  "daily_goals": {
    "calories": 2000,
    "protein": 60,
    "fat": 65,
    "carbs": 250
  }
}
```
- **字段说明**：
  - `actual`: 用户在当天通过膳食计划摄入的实际营养总量。
  - `recommended`: 系统根据用户健康档案（身高、体重、年龄、健康状况）计算出的当日推荐摄入目标。
  - `comparison`: 实际值与推荐值的对比详情。
    - `diff`: 差值（实际值 - 推荐值）。负值表示摄入不足，正值表示摄入过量。
    - `percentage`: 完成百分比（实际值 / 推荐值 * 100%）。用于绘制环形进度条或进度条。
  - `daily_goals`: 该用户当前的每日营养目标摘要，便于客户端在顶部展示固定目标（如“今日目标 2000 大卡”）。

---

### 7. 管理接口（需管理员权限）

#### 7.1 用户管理

##### 7.1.1 获取用户列表（分页）
- **方法**：`GET`
- **路径**：`/api/admin/users`
- **查询参数**：`?username=test&page=1&size=20`
- **成功响应**：
```json
{
  "data": [
    {
      "id": 1,
      "username": "testuser",
      "email": "test@example.com",
      "role": "user",
      "status": "active",
      "created_at": "2026-04-01T12:00:00Z"
    }
  ],
  "pagination": { ... }
}
```

##### 7.1.2 创建用户账户
- **方法**：`POST`
- **路径**：`/api/admin/users`
- **请求体**：
```json
{
  "username": "newuser",
  "password": "temppass123",
  "email": "new@example.com",
  "role": "user"
}
```
- **成功响应**：`201 Created`

##### 7.1.3 修改用户信息
- **方法**：`PUT`
- **路径**：`/api/admin/users/:user_id`
- **请求体**：
```json
{
  "email": "updated@example.com",
  "role": "admin"
}
```
- **成功响应**：`200 OK`

##### 7.1.4 冻结/解封用户
- **方法**：`PATCH`
- **路径**：`/api/admin/users/:user_id/status`
- **请求体**：
```json
{
  "status": "frozen"
}
```
- **成功响应**：`200 OK`

##### 7.1.5 删除用户
- **方法**：`DELETE`
- **路径**：`/api/admin/users/:user_id`
- **成功响应**：`204 No Content`

#### 7.2 获取待审核菜谱列表（分页）
- **方法**：`GET`
- **路径**：`/api/admin/recipes/pending`
- **成功响应**：**统一分页格式**。

#### 7.3 审核通过菜谱（单个）
- **方法**：`POST`
- **路径**：`/api/admin/recipes/:id/approve`

#### 7.4 审核拒绝菜谱（单个）【v2.0 补充版权原因示例】
- **方法**：`POST`
- **路径**：`/api/admin/recipes/:id/reject`
- **说明**：管理员拒绝菜谱时，需填写拒绝原因，可包含版权侵权、内容质量不达标、信息不完整等情况。
- **请求体**：
```json
{
  "reason": "图片不清晰，请重新上传"
}
```
- **其他拒绝原因示例**：
```json
{
  "reason": "疑似侵权：与站内菜谱'经典红烧肉'内容高度雷同，请提交原创证明"
}
```

#### 7.5 批量审核菜谱【v1.7 新增】
- **方法**：`POST`
- **路径**：`/api/admin/recipes/batch-review`
- **说明**：对齐用例 3.3 备选流中的“批量审核通过/拒绝”需求，提高管理员审核效率。
- **请求体**：
```json
{
  "recipe_ids": [101, 102, 105],
  "action": "approve",
  "reason": "批量通过"
}
```
- **字段说明**：
  - `recipe_ids`：待批量操作的菜谱 ID 数组，最大支持 **50** 个。
  - `action`：操作类型，枚举值 `approve` 或 `reject`。
  - `reason`：可选，当 `action` 为 `reject` 时建议提供统一拒绝原因。
- **成功响应**：`200 OK`
```json
{
  "message": "Batch review completed",
  "success_count": 3,
  "failed_ids": []
}
```
- **失败/部分成功响应**：`200 OK`（业务部分成功）
```json
{
  "message": "Batch review completed with some errors",
  "success_count": 2,
  "failed_ids": [105]
}
```

#### 7.6 发布系统公告
- **方法**：`POST`
- **路径**：`/api/admin/announcements`

#### 7.7 发送系统通知【v1.7 新增】
- **方法**：`POST`
- **路径**：`/api/admin/notifications`
- **说明**：对齐需求 3.5 与用例 3.2，支持向指定用户或群组发送站内信或邮件通知。
- **请求体**：
```json
{
  "title": "活动通知",
  "content": "亲爱的用户，新版本已上线，点击查看新功能！",
  "target_type": "specific",
  "target_ids": [1, 5, 23],
  "channels": ["in_app", "email"],
  "scheduled_at": null
}
```
- **字段说明**：
  - `target_type`：发送范围，枚举值 `all`（全站用户） 或 `specific`（指定用户）。
  - `target_ids`：当 `target_type` 为 `specific` 时必填，为用户 ID 数组，最大支持 **1000** 个。
  - `channels`：发送渠道数组，支持 `in_app`（站内信）和 `email`（邮件）。需确保用户资料中有对应联系方式。
  - `scheduled_at`：可选，ISO 8601 格式时间。若不填或为 `null` 则立即发送；若填写未来时间则定时发送。
- **成功响应**：`201 Created`
```json
{
  "notification_id": 5001,
  "status": "pending",
  "estimated_recipients": 3
}
```
- **失败响应**：
  - `400`：`target_type` 为 `specific` 但 `target_ids` 为空或无效。
  - `403`：当前管理员无通知发送权限。

---

### 8. 运维与扩展功能边界说明

以下功能需求根据系统架构设计，**不通过 RESTful API 对外暴露**，而是由运维层面的工具或客户端本地能力实现。

| 功能点 | 实现策略 |
|--------|----------|
| **菜谱分享** | 客户端生成分享链接，调用系统分享能力。 |
| **日志监控** | 服务器本地脚本监控或使用 ELK 等日志系统。 |
| **数据备份** | 使用 PostgreSQL 自带工具（pg_dump）或 Cron 定时任务。 |
| **广告管理** | 暂未纳入 API 范围，后续视运营需求独立设计管理后台。 |

---

### 9. 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| **2.0** | 2026-04-22 | GoCook 团队 | **版权校验机制明确化**：在菜谱投稿接口 (`POST /api/recipes`) 说明中明确版权校验为服务端内部逻辑，不额外暴露 API；在获取投稿列表 (`GET /api/recipes/my`) 和审核拒绝接口 (`POST /api/admin/recipes/:id/reject`) 的示例中增加版权相关的拒绝原因（如“疑似侵权”），确保与需求文档对齐。 |
| 1.9 | 2026-04-22 | GoCook 团队 | **营养趋势对比增强**：`GET /api/meal-plans/nutrition-trend` 接口响应增加 `recommended` 推荐摄入量及 `comparison` 对比详情（差值/百分比），支持客户端直观展示营养摄入达成情况，完善健康管理闭环。 |
| 1.8 | 2026-04-22 | GoCook 团队 | **需求对齐与接口增强**：为 `GET /api/recipes/public` 和 `/search` 增加明确的筛选查询参数（cuisine, meal_type, difficulty, max_time）；增强 `GET /api/inventory/shopping-list` 响应结构，增加 `required_quantity`, `inventory_quantity`, `to_buy_quantity` 字段以明确库存对比；在文档中增加关于烹饪进度保存接口、营养摄入目标对比字段的未来迭代备注。 |
| 1.7 | 2026-04-22 | GoCook 团队 | **后台管理功能补全**：新增通知发送接口 `POST /api/admin/notifications`；新增菜谱批量审核接口 `POST /api/admin/recipes/batch-review`。 |
| 1.6 | 2026-04-22 | GoCook 团队 | **规范化增强**：系统公告接口 `GET /api/announcements` 调整为统一分页格式；补充 JWT Token 有效期约定；增加库存单位标准化的未来迭代备注。 |
| 1.5 | 2026-04-22 | GoCook 团队 | **接口优化增强**：智能推荐接口返回增加 `match_score` 匹配度字段；膳食计划日历视图 `daily_total` 补全 `fat`、`carbs` 字段；健康指标接口忌口建议改为结构化对象，包含原因说明。 |
| 1.4 | 2026-04-22 | GoCook 团队 | **需求对齐补缺**：增加用户个人资料更新接口 `PUT /api/users/me/profile`；智能推荐接口 `missing_ingredients` 增加建议用量与单位，支持一键批量加入购物车。 |
| 1.3 | 2026-04-22 | GoCook 团队 | 菜谱详情增加步骤计时字段；增加菜谱视频关联接口；明确膳食计划餐别枚举值与评分范围。 |
| 1.2 | 2026-04-22 | GoCook 团队 | 补全后台用户管理接口；增加收藏列表、膳食计划详情与更新接口；增强营养汇总与购物车批量操作能力；明确分享与运维功能的实现边界。 |
| 1.1 | 2026-04-22 | GoCook 团队 | 根据需求对齐评审结果修订：补充公告/评论接口，统一分页格式，重构库存删除为ID方式，投稿增加拒绝原因字段。 |
| 1.0 | 2026-04-21 | GoCook 团队 | 初始版本，定义最小可行子集。 |
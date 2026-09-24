pragma Singleton
import QtQuick

/**
 * 页面登录策略表 —— 导航守卫 guardedPush 的声明式策略单一来源。
 *
 * 约定：
 *   - 属性名 = Main.qml 中对应页面 Component 的 id（页面改名时同步改这里）；
 *   - 值 = 该页面是否要求登录：true = 未登录先弹应用内登录页（登录成功后继续原导航）；
 *     false = 游客可直接进入——进入后按页面自身语义呈现（Silent 登录空态 / 页内分层 / 纯公开）。
 *   - Main.qml 的调用点统一传 PagePolicy.<pageId>，不再散落布尔字面量。
 *
 * 注意：拼错属性名会得到 undefined（假值 → 守卫静默放行），
 * guardedPush 内含 typeof 防御告警兜底，qmllint 静态检查覆盖拼写。
 *
 * 维护：新增页面 ① 在此加一行策略；② Main.qml 调用点传 PagePolicy.<pageId>。
 * 不在本表：登录页/主页/加载页（非 guardedPush 目标）。
 */
QtObject {
    // ---- 需登录（未登录 → 先弹应用内登录页，成功后继续原导航） ----
    readonly property bool recommendResultsPage: true     // 智能推荐结果
    readonly property bool shoppingListPage: true         // 购物清单
    readonly property bool submitRecipePage: true         // 提交/编辑投稿
    readonly property bool shoppingListDetailPage: true   // 购物清单详情
    readonly property bool profileEditPage: true          // 编辑资料
    readonly property bool accountSecurityPage: true      // 账号安全
    readonly property bool preferencesPage: true          // 饮食偏好
    readonly property bool healthProfilePage: true        // 健康档案
    readonly property bool changePasswordPage: true       // 修改密码

    // ---- 游客可进入 · 页内按语义分层（Silent 登录空态 / 页内守卫点击拦截） ----
    readonly property bool myRecipesPage: false           // 我的投稿（未登录显示登录空态）
    readonly property bool myRatingsPage: false           // 我的评论（未登录显示登录空态）
    readonly property bool notificationPage: false        // 消息中心（系统公告可看 / 个人通知登录空态）
    readonly property bool settingsPage: false            // 设置（主题本地可用 / 个人入口点击守卫）

    // ---- 游客可浏览（纯公开页面） ----
    readonly property bool recipeDetailPage: false        // 菜谱详情
    readonly property bool nutritionReportPage: false     // 营养报告（详情族）
    readonly property bool searchPage: false              // 搜索
    readonly property bool systemNoticePage: false        // 系统通知（公告）
    readonly property bool notificationDetailPage: false  // 通知详情（纯展示页；游客仅能经公告到达）
    readonly property bool featurePlaceholderPage: false  // 功能占位页
}

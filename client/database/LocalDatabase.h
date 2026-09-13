#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVariantList>
#include <QMutex>

/**
 * @brief 本地 SQLite 数据库管理类（单例）
 *
 * 持久化三类数据：
 *   - 用户凭证（user 表，单行不变量——自动登录的唯一事实源）；
 *   - 库存快照（inventory_cache + inventory_cache_meta，在线优先+断网读兜底，按用户隔离）；
 *   - 菜谱详情缓存（recipe_detail_cache，按 id upsert，上限 20 条——断网详情页兜底）。
 *
 * 仅由 ViewModel / main 经 C++ 调用（已不注册给 QML）；内部互斥锁保护全部操作
 * （QSqlDatabase 非线程安全）。schema 演进走 PRAGMA user_version 门控（见 .cpp）。
 */
class LocalDatabase : public QObject
{
    Q_OBJECT
public:
    // 获取单例实例
    static LocalDatabase* instance();

    // 测试专用：用指定数据库路径/连接名创建独立实例（不设置单例，不污染生产路径）。
    // 默认使用内存库（:memory:），可用于单元测试（需要 QSqlDatabase::removeDatabase 释放连接）
    static LocalDatabase* createForTesting(const QString& dbPath = QStringLiteral(":memory:"),
                                           const QString& connectionName = QStringLiteral("gocook_test"));

    // 检查数据库连接是否已打开（仅反映 open 结果；建表/迁移失败不改变本值，见构造期日志）
    bool isOpen() const;

    // 保存用户凭证（整体替换 user 表——单行不变量；失败返回 false 并打日志）
    bool saveUser(int userId, const QString &username, const QString &token);
    // 获取本地保存的用户信息（未登录返回空 map）
    QVariantMap getUser() const;
    // 清除用户凭证与库存快照（登出/换号的唯一清理点）
    bool clearUser();

    // 保存库存快照（整体替换当前用户快照，未登录返回 false；事务包裹，失败整体回滚——
    // 含非空输入整批缺键的放弃写入，不落“假空快照”）
    bool saveInventoryCache(const QVariantList &items);
    // 获取当前用户的库存快照（保持服务端原始顺序；无用户/无快照返回空列表）
    QVariantList getInventoryCache() const;
    // 当前用户是否存在库存快照（区分"空快照"与"从未同步"）
    bool hasInventoryCache() const;
    // 从当前用户快照中移除指定行（复合键 = 服务端三维语义 ingredient_name + unit）。
    // 删除库存成功后同步调用（防"删除后断网兜底复活已删项"）；
    // 未登录/键为空返回 false（拒绝）；行不存在时静默 no-op（exec 成功即 true）
    bool removeInventoryCacheItem(const QString &ingredientName, const QString &unit);

    // 保存菜谱详情缓存（按菜谱 id upsert，最多保留最近 20 条）——断网时详情页兜底显示
    bool saveRecipeDetailCache(int recipeId, const QVariantMap &detail);
    // 获取菜谱详情缓存（无缓存返回空 map）
    QVariantMap getRecipeDetailCache(int recipeId) const;

private:
    // 私有构造函数，单例模式
    explicit LocalDatabase(QObject *parent = nullptr);
    // 私有构造函数（测试用）：指定数据库路径与连接名
    explicit LocalDatabase(const QString& dbPath, const QString& connectionName, QObject *parent = nullptr);
    // 创建/升级表结构（PRAGMA user_version 门控，见 .cpp）
    bool createTables();
    // 生产路径初始化（QStandardPaths::AppDataLocation + gocook.db；仅构造函数调用）
    bool initialize();
    // 用指定路径/连接名初始化（仅构造函数调用）
    bool initialize(const QString& dbPath, const QString& connectionName);
    // 读取 user 表中的当前用户 id；未登录返回 0。调用方须已持 m_mutex（Locked 惯例）
    int currentUserIdLocked() const;

    // 数据库连接对象
    QSqlDatabase m_db;
    mutable QMutex m_mutex;
    // 单例静态指针
    static LocalDatabase *m_instance;
};
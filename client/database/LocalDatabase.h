#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVariantList>
#include <QMutex>

/**
 * @brief 本地 SQLite 数据库管理类（单例）
 *
 * 负责存储用户凭证、库存缓存、菜谱缓存以及离线操作队列。
 * 提供 Q_INVOKABLE 方法供 QML 直接调用。
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

    // 初始化数据库连接和表结构
    Q_INVOKABLE bool initialize();
    // 检查数据库是否已打开
    Q_INVOKABLE bool isOpen() const;

    // 保存用户凭证到本地
    Q_INVOKABLE bool saveUser(int userId, const QString &username, const QString &token);
    // 获取本地保存的用户信息
    Q_INVOKABLE QVariantMap getUser() const;
    // 清除本地用户信息
    Q_INVOKABLE bool clearUser();

    // 保存库存缓存数据
    Q_INVOKABLE bool saveInventoryCache(const QVariantList &items);
    // 获取库存缓存数据
    Q_INVOKABLE QVariantList getInventoryCache() const;

    // 保存菜谱详情缓存（按菜谱 id upsert，最多保留最近 20 条）——断网时详情页兜底显示
    Q_INVOKABLE bool saveRecipeDetailCache(int recipeId, const QVariantMap &detail);
    // 获取菜谱详情缓存（无缓存返回空 map）
    Q_INVOKABLE QVariantMap getRecipeDetailCache(int recipeId) const;

signals:
    // 数据库错误信号
    void databaseError(const QString &error);

private:
    // 私有构造函数，单例模式
    explicit LocalDatabase(QObject *parent = nullptr);
    // 私有构造函数（测试用）：指定数据库路径与连接名
    explicit LocalDatabase(const QString& dbPath, const QString& connectionName, QObject *parent = nullptr);
    // 创建所有表结构
    bool createTables();
    // 用指定路径/连接名初始化（生产路径走 QStandardPaths 默认位置）
    bool initialize(const QString& dbPath, const QString& connectionName);

    // 数据库连接对象
    QSqlDatabase m_db;
    mutable QMutex m_mutex;
    // 单例静态指针
    static LocalDatabase *m_instance;
};
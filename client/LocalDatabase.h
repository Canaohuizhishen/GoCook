#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QVariantList>

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

    // 保存菜谱缓存数据
    Q_INVOKABLE bool saveRecipesCache(const QVariantList &recipes);
    // 获取菜谱缓存数据
    Q_INVOKABLE QVariantList getRecipesCache() const;

    // 添加一条待处理的离线操作
    Q_INVOKABLE bool addPendingOperation(const QString &operation, const QVariantMap &data);
    // 获取所有待处理的离线操作
    Q_INVOKABLE QVariantList getPendingOperations() const;
    // 清空待处理操作队列
    Q_INVOKABLE bool clearPendingOperations();

signals:
    // 数据库错误信号
    void databaseError(const QString &error);

private:
    // 私有构造函数，单例模式
    explicit LocalDatabase(QObject *parent = nullptr);
    // 创建所有表结构
    bool createTables();

    // 数据库连接对象
    QSqlDatabase m_db;
    // 单例静态指针
    static LocalDatabase *m_instance;
};
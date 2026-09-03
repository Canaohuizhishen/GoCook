#include "LocalDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>
#include <QDebug>

LocalDatabase* LocalDatabase::m_instance = nullptr;

LocalDatabase* LocalDatabase::instance()
{
    if (!m_instance) {
        m_instance = new LocalDatabase();
    }
    return m_instance;
}

LocalDatabase::LocalDatabase(QObject *parent) : QObject(parent)
{
    initialize();
}

LocalDatabase* LocalDatabase::createForTesting(const QString& dbPath, const QString& connectionName)
{
    return new LocalDatabase(dbPath, connectionName);
}

LocalDatabase::LocalDatabase(const QString& dbPath, const QString& connectionName, QObject *parent)
    : QObject(parent)
{
    initialize(dbPath, connectionName);
}

bool LocalDatabase::initialize(const QString& dbPath, const QString& connectionName)
{
    QMutexLocker locker(&m_mutex);

    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        emit databaseError("无法打开数据库：" + m_db.lastError().text());
        return false;
    }
    return createTables();
}

bool LocalDatabase::initialize()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    return initialize(dataPath + "/gocook.db", QString());
}

bool LocalDatabase::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_db.isOpen();
}

bool LocalDatabase::createTables()
{
    QSqlQuery query(m_db);

    if (!query.exec("CREATE TABLE IF NOT EXISTS user ("
                    "id INTEGER PRIMARY KEY,"
                    "username TEXT,"
                    "token TEXT)") ||
        !query.exec("CREATE TABLE IF NOT EXISTS inventory_cache ("
                    "ingredient_name TEXT PRIMARY KEY,"
                    "quantity REAL,"
                    "unit TEXT,"
                    "expiry_date TEXT,"
                    "added_at TEXT)") ||
        !query.exec("CREATE TABLE IF NOT EXISTS recipe_detail_cache ("
                    "id INTEGER PRIMARY KEY,"
                    "data TEXT NOT NULL,"
                    "updated_at TEXT)") ||
        // 离线写队列已整体拆除（v2 决策：断网写操作明确报错，不做自动重放）——
        // 老库遗留的 pending_operations 表直接清理，避免死数据堆积
        !query.exec("DROP TABLE IF EXISTS pending_operations")) {
        qWarning() << "[LocalDatabase] createTables 基础表创建/清理失败:" << query.lastError().text();
        return false;
    }

    // 迁移：老结构（明细列，字段名与页面读取的 DataMapper camelCase 键不匹配）从未被
    // 使用（死代码），检测到无 data 列直接重建，避免写入空值缓存
    QSqlQuery cachePragma(m_db);
    if (!cachePragma.exec("PRAGMA table_info(recipe_detail_cache)")) {
        qWarning() << "[LocalDatabase] PRAGMA table_info 失败:" << cachePragma.lastError().text();
        return false;
    }
    bool hasData = false;
    while (cachePragma.next()) {
        if (cachePragma.value(1).toString() == QLatin1String("data")) hasData = true;
    }
    if (!hasData) {
        if (!query.exec("DROP TABLE recipe_detail_cache") ||
            !query.exec("CREATE TABLE recipe_detail_cache ("
                        "id INTEGER PRIMARY KEY,"
                        "data TEXT NOT NULL,"
                        "updated_at TEXT)")) {
            qWarning() << "[LocalDatabase] 迁移重建 recipe_detail_cache 失败:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool LocalDatabase::saveUser(int userId, const QString &username, const QString &token)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT OR REPLACE INTO user (id, username, token) VALUES (?, ?, ?)");
    query.addBindValue(userId);
    query.addBindValue(username);
    query.addBindValue(token);
    return query.exec();
}

QVariantMap LocalDatabase::getUser() const
{
    QMutexLocker locker(&m_mutex);
    QVariantMap user;
    QSqlQuery query("SELECT id, username, token FROM user LIMIT 1", m_db);
    if (query.next()) {
        user["id"] = query.value(0);
        user["username"] = query.value(1);
        user["token"] = query.value(2);
    }
    return user;
}

bool LocalDatabase::clearUser()
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query("DELETE FROM user", m_db);
    return query.exec();
}

bool LocalDatabase::saveInventoryCache(const QVariantList &items)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.exec("DELETE FROM inventory_cache");
    query.prepare("INSERT INTO inventory_cache (ingredient_name, quantity, unit, expiry_date, added_at) "
                  "VALUES (?, ?, ?, ?, ?)");
    for (const QVariant &itemVar : items) {
        QVariantMap item = itemVar.toMap();
        query.addBindValue(item["ingredient_name"]);
        query.addBindValue(item["quantity"]);
        query.addBindValue(item["unit"]);
        query.addBindValue(item["expiry_date"]);
        query.addBindValue(item["added_at"]);
        if (!query.exec()) {
            return false;
        }
    }
    return true;
}

QVariantList LocalDatabase::getInventoryCache() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList list;
    QSqlQuery query("SELECT ingredient_name, quantity, unit, expiry_date, added_at FROM inventory_cache", m_db);
    while (query.next()) {
        QVariantMap item;
        item["ingredient_name"] = query.value(0);
        item["quantity"] = query.value(1);
        item["unit"] = query.value(2);
        item["expiry_date"] = query.value(3);
        item["added_at"] = query.value(4);
        list.append(item);
    }
    return list;
}

bool LocalDatabase::saveRecipeDetailCache(int recipeId, const QVariantMap &detail)
{
    QMutexLocker locker(&m_mutex);
    // 整份 detail map 以 JSON 存储：键名与页面读取的 DataMapper camelCase 字段天然一致
    // 写入与上限裁剪放进同一事务：任一失败整体回滚，避免缓存与实际状态不一致
    if (!m_db.transaction()) {
        qWarning() << "[LocalDatabase] 开启事务失败:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    // 毫秒级时间戳（%f）：同秒大量写入也能稳定排序，避免上限裁剪误删刚写入的行
    query.prepare("INSERT INTO recipe_detail_cache (id, data, updated_at) "
                  "VALUES (?, ?, strftime('%Y-%m-%d %H:%M:%f','now')) "
                  "ON CONFLICT(id) DO UPDATE SET data = excluded.data, updated_at = excluded.updated_at");
    query.addBindValue(recipeId);
    query.addBindValue(QJsonDocument(QJsonObject::fromVariantMap(detail)).toJson(QJsonDocument::Compact));
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 写入详情缓存失败 (id=" << recipeId << "):" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    // 上限清理：只保留最近 20 条（按更新时间倒序）
    QSqlQuery prune(m_db);
    if (!prune.exec("DELETE FROM recipe_detail_cache WHERE id NOT IN "
                    "(SELECT id FROM recipe_detail_cache ORDER BY updated_at DESC LIMIT 20)")) {
        qWarning() << "[LocalDatabase] 裁剪详情缓存失败:" << prune.lastError().text();
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        // commit 失败必须回滚关闭事务，否则事务悬挂导致后续所有缓存写入静默失败
        m_db.rollback();
        qWarning() << "[LocalDatabase] 提交缓存事务失败:" << m_db.lastError().text();
        return false;
    }
    return true;
}

QVariantMap LocalDatabase::getRecipeDetailCache(int recipeId) const
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("SELECT data FROM recipe_detail_cache WHERE id = ?");
    query.addBindValue(recipeId);
    if (!query.exec() || !query.next()) return QVariantMap();
    return QJsonDocument::fromJson(query.value(0).toByteArray()).object().toVariantMap();
}

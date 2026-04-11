#include "LocalDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// 初始化单例静态指针
LocalDatabase* LocalDatabase::m_instance = nullptr;

// 获取单例实例
LocalDatabase* LocalDatabase::instance()
{
    if (!m_instance) {
        m_instance = new LocalDatabase();
    }
    return m_instance;
}

// 构造函数
LocalDatabase::LocalDatabase(QObject *parent) : QObject(parent)
{
    // 自动初始化数据库
    initialize();
}

// 初始化数据库连接和表结构
bool LocalDatabase::initialize()
{
    // 获取应用程序数据存储路径
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    QString dbPath = dataPath + "/gocook.db";

    // 添加 SQLite 数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);
    // 打开数据库
    if (!m_db.open()) {
        emit databaseError("Failed to open database: " + m_db.lastError().text());
        return false;
    }
    // 创建表结构
    return createTables();
}

// 检查数据库是否已打开
bool LocalDatabase::isOpen() const
{
    return m_db.isOpen();
}

// 创建所有表结构
bool LocalDatabase::createTables()
{
    QSqlQuery query(m_db);
    // 用户表
    query.exec("CREATE TABLE IF NOT EXISTS user ("
               "id INTEGER PRIMARY KEY,"
               "username TEXT,"
               "token TEXT)");
    // 库存缓存表
    query.exec("CREATE TABLE IF NOT EXISTS inventory_cache ("
               "ingredient_name TEXT PRIMARY KEY,"
               "quantity REAL,"
               "unit TEXT,"
               "expiry_date TEXT,"
               "added_at TEXT)");
    // 菜谱缓存表
    query.exec("CREATE TABLE IF NOT EXISTS recipes_cache ("
               "id INTEGER PRIMARY KEY,"
               "name TEXT,"
               "description TEXT,"
               "ingredients TEXT,"
               "instructions TEXT,"
               "prep_time INTEGER,"
               "cook_time INTEGER,"
               "servings INTEGER,"
               "tags TEXT,"
               "image_url TEXT)");
    // 离线操作队列表
    query.exec("CREATE TABLE IF NOT EXISTS pending_operations ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "operation TEXT,"
               "data TEXT,"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
    return true;
}

// 保存用户凭证
bool LocalDatabase::saveUser(int userId, const QString &username, const QString &token)
{
    QSqlQuery query(m_db);
    // 使用 INSERT OR REPLACE 确保只有一条记录
    query.prepare("INSERT OR REPLACE INTO user (id, username, token) VALUES (?, ?, ?)");
    query.addBindValue(userId);
    query.addBindValue(username);
    query.addBindValue(token);
    return query.exec();
}

// 获取本地保存的用户信息
QVariantMap LocalDatabase::getUser() const
{
    QVariantMap user;
    QSqlQuery query("SELECT id, username, token FROM user LIMIT 1", m_db);
    if (query.next()) {
        user["id"] = query.value(0);
        user["username"] = query.value(1);
        user["token"] = query.value(2);
    }
    return user;
}

// 清除本地用户信息
bool LocalDatabase::clearUser()
{
    QSqlQuery query("DELETE FROM user", m_db);
    return query.exec();
}

// 保存库存缓存
bool LocalDatabase::saveInventoryCache(const QVariantList &items)
{
    QSqlQuery query(m_db);
    // 清空原有缓存
    query.exec("DELETE FROM inventory_cache");
    // 准备插入语句
    query.prepare("INSERT INTO inventory_cache (ingredient_name, quantity, unit, expiry_date, added_at) "
                  "VALUES (?, ?, ?, ?, ?)");
    // 遍历列表插入数据
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

// 获取库存缓存
QVariantList LocalDatabase::getInventoryCache() const
{
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

// 保存菜谱缓存
bool LocalDatabase::saveRecipesCache(const QVariantList &recipes)
{
    QSqlQuery query(m_db);
    // 清空原有缓存
    query.exec("DELETE FROM recipes_cache");
    // 准备插入语句
    query.prepare("INSERT INTO recipes_cache "
                  "(id, name, description, ingredients, instructions, prep_time, cook_time, servings, tags, image_url) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    // 遍历菜谱列表插入数据
    for (const QVariant &recVar : recipes) {
        QVariantMap rec = recVar.toMap();
        query.addBindValue(rec["id"]);
        query.addBindValue(rec["name"]);
        query.addBindValue(rec["description"]);
        // 将 ingredients 列表转换为 JSON 字符串存储
        query.addBindValue(QJsonDocument(QJsonArray::fromVariantList(rec["ingredients"].toList())).toJson(QJsonDocument::Compact));
        query.addBindValue(rec["instructions"]);
        query.addBindValue(rec["prep_time_minutes"]);
        query.addBindValue(rec["cook_time_minutes"]);
        query.addBindValue(rec["servings"]);
        // 将 tags 列表转换为 JSON 字符串存储
        query.addBindValue(QJsonDocument(QJsonArray::fromVariantList(rec["tags"].toList())).toJson(QJsonDocument::Compact));
        query.addBindValue(rec["image_url"]);
        if (!query.exec()) {
            return false;
        }
    }
    return true;
}

// 获取菜谱缓存
QVariantList LocalDatabase::getRecipesCache() const
{
    QVariantList list;
    QSqlQuery query("SELECT id, name, description, ingredients, instructions, "
                    "prep_time, cook_time, servings, tags, image_url FROM recipes_cache", m_db);
    while (query.next()) {
        QVariantMap rec;
        rec["id"] = query.value(0);
        rec["name"] = query.value(1);
        rec["description"] = query.value(2);
        // 将存储的 JSON 字符串解析回列表
        QJsonArray ingArray = QJsonDocument::fromJson(query.value(3).toByteArray()).array();
        rec["ingredients"] = ingArray.toVariantList();
        rec["instructions"] = query.value(4);
        rec["prep_time_minutes"] = query.value(5);
        rec["cook_time_minutes"] = query.value(6);
        rec["servings"] = query.value(7);
        QJsonArray tagsArray = QJsonDocument::fromJson(query.value(8).toByteArray()).array();
        rec["tags"] = tagsArray.toVariantList();
        rec["image_url"] = query.value(9);
        list.append(rec);
    }
    return list;
}

// 添加离线操作
bool LocalDatabase::addPendingOperation(const QString &operation, const QVariantMap &data)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO pending_operations (operation, data) VALUES (?, ?)");
    query.addBindValue(operation);
    // 将数据转换为 JSON 字符串存储
    query.addBindValue(QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact));
    return query.exec();
}

// 获取所有待处理操作
QVariantList LocalDatabase::getPendingOperations() const
{
    QVariantList list;
    QSqlQuery query("SELECT id, operation, data FROM pending_operations ORDER BY created_at", m_db);
    while (query.next()) {
        QVariantMap op;
        op["id"] = query.value(0);
        op["operation"] = query.value(1);
        // 将存储的 JSON 字符串解析回 Map
        op["data"] = QJsonDocument::fromJson(query.value(2).toByteArray()).object().toVariantMap();
        list.append(op);
    }
    return list;
}

// 清空待处理操作队列
bool LocalDatabase::clearPendingOperations()
{
    QSqlQuery query("DELETE FROM pending_operations", m_db);
    return query.exec();
}
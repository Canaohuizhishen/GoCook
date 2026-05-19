#include "LocalDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutexLocker>

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

bool LocalDatabase::initialize()
{
    QMutexLocker locker(&m_mutex);
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    QString dbPath = dataPath + "/gocook.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        emit databaseError("Failed to open database: " + m_db.lastError().text());
        return false;
    }
    return createTables();
}

bool LocalDatabase::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_db.isOpen();
}

bool LocalDatabase::createTables()
{
    QSqlQuery query(m_db);

    query.exec("CREATE TABLE IF NOT EXISTS user ("
               "id INTEGER PRIMARY KEY,"
               "username TEXT,"
               "token TEXT)");

    query.exec("CREATE TABLE IF NOT EXISTS inventory_cache ("
               "ingredient_name TEXT PRIMARY KEY,"
               "quantity REAL,"
               "unit TEXT,"
               "expiry_date TEXT,"
               "added_at TEXT)");

    query.exec("CREATE TABLE IF NOT EXISTS recipe_detail_cache ("
               "id INTEGER PRIMARY KEY,"
               "name TEXT,"
               "description TEXT,"
               "image_url TEXT,"
               "cooking_method TEXT,"
               "flavor TEXT,"
               "prep_time_minutes INTEGER,"
               "cook_time_minutes INTEGER,"
               "view_count INTEGER,"
               "avg_rating REAL,"
               "ingredients TEXT,"
               "steps TEXT,"
               "nutrition TEXT,"
               "tags TEXT,"
               "author_id INTEGER,"
               "author_name TEXT,"
               "created_at TEXT)");

    query.exec("CREATE TABLE IF NOT EXISTS pending_operations ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "operation TEXT,"
               "data TEXT,"
               "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
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

bool LocalDatabase::saveRecipeDetailCache(const QVariantList &recipes)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.exec("DELETE FROM recipe_detail_cache");
    query.prepare("INSERT INTO recipe_detail_cache "
                  "(id, name, description, image_url, cooking_method, flavor, "
                  "prep_time_minutes, cook_time_minutes, view_count, avg_rating, "
                  "ingredients, steps, nutrition, tags, author_id, author_name, created_at) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    for (const QVariant &recVar : recipes) {
        QVariantMap rec = recVar.toMap();
        query.addBindValue(rec["id"]);
        query.addBindValue(rec["name"]);
        query.addBindValue(rec["description"]);
        query.addBindValue(rec["image_url"]);
        query.addBindValue(rec["cooking_method"]);
        query.addBindValue(rec["flavor"]);
        query.addBindValue(rec["prep_time_minutes"]);
        query.addBindValue(rec["cook_time_minutes"]);
        query.addBindValue(rec["view_count"]);
        query.addBindValue(rec["avg_rating"]);
        query.addBindValue(QJsonDocument(QJsonArray::fromVariantList(rec["ingredients"].toList())).toJson(QJsonDocument::Compact));
        query.addBindValue(QJsonDocument(QJsonArray::fromVariantList(rec["steps"].toList())).toJson(QJsonDocument::Compact));
        query.addBindValue(QJsonDocument(QJsonObject::fromVariantMap(rec["nutrition"].toMap())).toJson(QJsonDocument::Compact));
        query.addBindValue(QJsonDocument(QJsonArray::fromVariantList(rec["tags"].toList())).toJson(QJsonDocument::Compact));
        query.addBindValue(rec["author_id"]);
        query.addBindValue(rec["author_name"]);
        query.addBindValue(rec["created_at"]);
        if (!query.exec()) {
            return false;
        }
    }
    return true;
}

QVariantList LocalDatabase::getRecipeDetailCache() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList list;
    QSqlQuery query("SELECT id, name, description, image_url, cooking_method, flavor, "
                    "prep_time_minutes, cook_time_minutes, view_count, avg_rating, "
                    "ingredients, steps, nutrition, tags, author_id, author_name, created_at "
                    "FROM recipe_detail_cache", m_db);
    while (query.next()) {
        QVariantMap rec;
        rec["id"] = query.value(0);
        rec["name"] = query.value(1);
        rec["description"] = query.value(2);
        rec["image_url"] = query.value(3);
        rec["cooking_method"] = query.value(4);
        rec["flavor"] = query.value(5);
        rec["prep_time_minutes"] = query.value(6);
        rec["cook_time_minutes"] = query.value(7);
        rec["view_count"] = query.value(8);
        rec["avg_rating"] = query.value(9);
        rec["ingredients"] = QJsonDocument::fromJson(query.value(10).toByteArray()).array().toVariantList();
        rec["steps"] = QJsonDocument::fromJson(query.value(11).toByteArray()).array().toVariantList();
        rec["nutrition"] = QJsonDocument::fromJson(query.value(12).toByteArray()).object().toVariantMap();
        rec["tags"] = QJsonDocument::fromJson(query.value(13).toByteArray()).array().toVariantList();
        rec["author_id"] = query.value(14);
        rec["author_name"] = query.value(15);
        rec["created_at"] = query.value(16);
        list.append(rec);
    }
    return list;
}

bool LocalDatabase::addPendingOperation(const QString &operation, const QVariantMap &data)
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO pending_operations (operation, data) VALUES (?, ?)");
    query.addBindValue(operation);
    query.addBindValue(QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact));
    return query.exec();
}

QVariantList LocalDatabase::getPendingOperations() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList list;
    QSqlQuery query("SELECT id, operation, data FROM pending_operations ORDER BY created_at", m_db);
    while (query.next()) {
        QVariantMap op;
        op["id"] = query.value(0);
        op["operation"] = query.value(1);
        op["data"] = QJsonDocument::fromJson(query.value(2).toByteArray()).object().toVariantMap();
        list.append(op);
    }
    return list;
}

bool LocalDatabase::clearPendingOperations()
{
    QMutexLocker locker(&m_mutex);
    QSqlQuery query("DELETE FROM pending_operations", m_db);
    return query.exec();
}

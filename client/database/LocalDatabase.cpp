#include "LocalDatabase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QDebug>
#include <mutex>

namespace {
// 库结构版本（PRAGMA user_version 门控；演进说明见 createTables）：
//   v0 = 版本门控引入前的存量库（旧版单主键 inventory_cache，从未被任何代码写入过）
//   v1 = 库存快照复合主键结构首版（详情表遗留旧结构未修复）
//   v2 = 当前：修复"明细列"老结构详情表 + 清理已拆功能的死表（recipes_cache / pending_operations）
constexpr int kSchemaVersion = 2;
}

LocalDatabase* LocalDatabase::m_instance = nullptr;

LocalDatabase* LocalDatabase::instance()
{
    // call_once：并发首次调用只构造一个实例。单例有意不析构（进程级对象，
    // 避免静态析构阶段触碰已销毁的 Qt 运行时）
    static std::once_flag once;
    std::call_once(once, [] { m_instance = new LocalDatabase(); });
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
        // 单例在构造期初始化，此刻无人能连接信号，失败只能靠日志暴露（isOpen() 供调用方复查）
        qWarning() << "[LocalDatabase] 无法打开数据库:" << m_db.lastError().text();
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

    // 读取库结构版本（PRAGMA user_version；未设置过为 0）。
    // 演进说明：版本步进式迁移（自上而下依次执行未达标步骤，不再使用探测式迁移）：
    //   v0→v1：重建 inventory_cache——旧版结构（单主键 ingredient_name）是未启用的
    //          预留（从未有任何写入方），丢弃重建无数据损失；
    //   v1→v2：修复"明细列"老结构详情表（纯缓存：探测无 data 列才重建）+ 清理死表
    int schemaVersion = 0;
    {
        QSqlQuery versionQuery(m_db);
        if (versionQuery.exec("PRAGMA user_version") && versionQuery.next())
            schemaVersion = versionQuery.value(0).toInt();
    }
    if (schemaVersion > kSchemaVersion) {
        qWarning() << "[LocalDatabase] 库结构版本高于程序支持（库=" << schemaVersion
                   << "，程序=" << kSchemaVersion << "），继续按当前版本运行";
    }
    if (schemaVersion < 1) {
        // v0→v1 步骤：丢弃旧版单主键库存表，由下方统一建表重建为复合主键结构
        if (!query.exec("DROP TABLE IF EXISTS inventory_cache")) {
            qWarning() << "[LocalDatabase] 重建库存表失败（DROP）:" << query.lastError().text();
            return false;
        }
    }
    if (schemaVersion < 2) {
        // v0→v2 / v1→v2 步骤（详情表老结构修复 + 死表清理）：
        //  1) 详情表：若是"明细列"时代老结构（无 data 列）则丢弃重建——CREATE IF NOT EXISTS
        //     不会修复已存在的老表，不重建则详情缓存写入持续失败；新结构保留（缓存不丢）；
        //  2) 已拆除功能的历史死表（离线写队列/旧菜谱缓存）：纯清理，防旧库死数据常驻
        bool detailCacheExists = false;
        bool detailCacheHasData = false;
        {
            QSqlQuery pragma(m_db);
            if (!pragma.exec("PRAGMA table_info(recipe_detail_cache)")) {
                qWarning() << "[LocalDatabase] PRAGMA table_info 失败:" << pragma.lastError().text();
                return false;
            }
            while (pragma.next()) {
                detailCacheExists = true;
                if (pragma.value(1).toString() == QLatin1String("data")) detailCacheHasData = true;
            }
        }
        if (detailCacheExists && !detailCacheHasData && !query.exec("DROP TABLE recipe_detail_cache")) {
            qWarning() << "[LocalDatabase] 重建详情表失败（DROP）:" << query.lastError().text();
            return false;
        }
        if (!query.exec("DROP TABLE IF EXISTS recipes_cache") ||
            !query.exec("DROP TABLE IF EXISTS pending_operations")) {
            qWarning() << "[LocalDatabase] 清理遗留死表失败:" << query.lastError().text();
            return false;
        }
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS user ("
                    "id INTEGER PRIMARY KEY,"
                    "username TEXT,"
                    "token TEXT)") ||
        !query.exec("CREATE TABLE IF NOT EXISTS inventory_cache ("
                    "user_id INTEGER NOT NULL,"
                    "ingredient_name TEXT NOT NULL,"
                    "unit TEXT NOT NULL,"
                    "ordinal INTEGER NOT NULL,"
                    "data TEXT NOT NULL,"
                    "updated_at TEXT,"
                    "PRIMARY KEY (user_id, ingredient_name, unit))") ||
        !query.exec("CREATE TABLE IF NOT EXISTS inventory_cache_meta ("
                    "user_id INTEGER PRIMARY KEY,"
                    "updated_at TEXT NOT NULL)") ||
        !query.exec("CREATE TABLE IF NOT EXISTS recipe_detail_cache ("
                    "id INTEGER PRIMARY KEY,"
                    "data TEXT NOT NULL,"
                    "updated_at TEXT)")) {
        qWarning() << "[LocalDatabase] createTables 建表失败:" << query.lastError().text();
        return false;
    }

    if (schemaVersion < kSchemaVersion) {
        if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion))) {
            qWarning() << "[LocalDatabase] 写入结构版本失败:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool LocalDatabase::saveUser(int userId, const QString &username, const QString &token)
{
    QMutexLocker locker(&m_mutex);
    // 单行不变量：整体替换（清空后插入），保证 getUser 的 LIMIT 1 无歧义——
    // 换号/重登绝不残留旧账号行（旧实现 INSERT OR REPLACE 按 id 行级替换、不清理其它行）
    if (!m_db.transaction()) {
        qWarning() << "[LocalDatabase] 开启保存用户事务失败:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM user")) {
        qWarning() << "[LocalDatabase] 清理旧用户行失败:" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    query.prepare("INSERT INTO user (id, username, token) VALUES (?, ?, ?)");
    query.addBindValue(userId);
    query.addBindValue(username);
    query.addBindValue(token);
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 写入用户凭证失败:" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    // 换号清理：快照/标记只归属"当前账号"——
    // 不经登出直接再登录（saveUser 覆盖）时旧账号快照不再有效：若留着，
    // 之后回到旧账号（又未经 clearUser）会命中陈快照。物理删除非当前 user_id 行
    query.prepare("DELETE FROM inventory_cache WHERE user_id <> ?");
    query.addBindValue(userId);
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 清理非当前用户快照失败:" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    query.prepare("DELETE FROM inventory_cache_meta WHERE user_id <> ?");
    query.addBindValue(userId);
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 清理非当前用户快照标记失败:" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        m_db.rollback();
        qWarning() << "[LocalDatabase] 提交保存用户事务失败:" << m_db.lastError().text();
        return false;
    }
    return true;
}

QVariantMap LocalDatabase::getUser() const
{
    QMutexLocker locker(&m_mutex);
    QVariantMap user;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, username, token FROM user LIMIT 1")) {
        qWarning() << "[LocalDatabase] 读取用户信息失败:" << query.lastError().text();
        return user;
    }
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
    // 登出/换号的唯一清理点：凭证与库存快照一并清除（快照按用户隔离，防换号后残留串台）；
    // 三步放同一事务，避免清一半留下不一致状态
    if (!m_db.transaction()) {
        qWarning() << "[LocalDatabase] 开启清理用户事务失败:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM user") ||
        !query.exec("DELETE FROM inventory_cache") ||
        !query.exec("DELETE FROM inventory_cache_meta")) {
        qWarning() << "[LocalDatabase] 清理用户数据失败:" << query.lastError().text();
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        m_db.rollback();
        qWarning() << "[LocalDatabase] 提交清理用户事务失败:" << m_db.lastError().text();
        return false;
    }
    return true;
}

bool LocalDatabase::saveInventoryCache(const QVariantList &items)
{
    QMutexLocker locker(&m_mutex);
    // 快照归属"当前登录用户"（user 表为唯一事实源）；未登录拒绝写入，防无主数据
    const int userId = currentUserIdLocked();
    if (userId <= 0) {
        qWarning() << "[LocalDatabase] 未登录，拒绝写入库存快照";
        return false;
    }

    // 整体替换 + meta 标记进同一事务：任一失败回滚，快照数据与"存在性"标记不分离
    if (!m_db.transaction()) {
        qWarning() << "[LocalDatabase] 开启库存快照事务失败:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery del(m_db);
    del.prepare("DELETE FROM inventory_cache WHERE user_id = ?");
    del.addBindValue(userId);
    if (!del.exec()) {
        qWarning() << "[LocalDatabase] 清理旧库存快照失败:" << del.lastError().text();
        m_db.rollback();
        return false;
    }

    // data 存整份 item map 的 JSON（键名与 DataMapper camelCase 天然一致，服务端模型演进无需改表）；
    // 键字段从 map 提取用于用户隔离与幂等（同名不同单位 = 两条，对齐服务端三维唯一语义）
    QSqlQuery ins(m_db);
    ins.prepare("INSERT OR REPLACE INTO inventory_cache "
                "(user_id, ingredient_name, unit, ordinal, data, updated_at) "
                "VALUES (?, ?, ?, ?, ?, strftime('%Y-%m-%d %H:%M:%f','now'))");
    int ordinal = 0;
    for (const QVariant &itemVar : items) {
        const QVariantMap item = itemVar.toMap();
        const QString name = item.value("ingredientName").toString();
        const QString unit = item.value("unit").toString();
        if (name.isEmpty() || unit.isEmpty()) {
            // 键字段缺失 = 脏数据，跳过防坏行占坑（服务端契约保证正常数据必有这两个字段）
            qWarning() << "[LocalDatabase] 库存快照项缺少 ingredientName/unit，已跳过";
            continue;
        }
        ins.addBindValue(userId);
        ins.addBindValue(name);
        ins.addBindValue(unit);
        ins.addBindValue(ordinal++);
        ins.addBindValue(QJsonDocument(QJsonObject::fromVariantMap(item)).toJson(QJsonDocument::Compact));
        if (!ins.exec()) {
            qWarning() << "[LocalDatabase] 写入库存快照项失败（" << name << unit << "）:" << ins.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // 整批脏数据防御：非空输入却一行未写出（ordinal 仍为 0，即全部条目都因缺键被跳过）——
    // 数据整体不可信，放弃本次替换（回滚保留旧快照与标记）。否则会落下“空快照”，
    // 离线兜底时谎报“暂无库存”；空快照语义只留给真实的空库存（items 为空）
    if (!items.isEmpty() && ordinal == 0) {
        qWarning() << "[LocalDatabase] 库存快照整批条目缺键，放弃本次写入:" << items.size() << "条";
        m_db.rollback();
        return false;
    }

    QSqlQuery meta(m_db);
    meta.prepare("INSERT INTO inventory_cache_meta (user_id, updated_at) "
                 "VALUES (?, strftime('%Y-%m-%d %H:%M:%f','now')) "
                 "ON CONFLICT(user_id) DO UPDATE SET updated_at = excluded.updated_at");
    meta.addBindValue(userId);
    if (!meta.exec()) {
        qWarning() << "[LocalDatabase] 更新库存快照标记失败:" << meta.lastError().text();
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        m_db.rollback();
        qWarning() << "[LocalDatabase] 提交库存快照事务失败:" << m_db.lastError().text();
        return false;
    }
    return true;
}

QVariantList LocalDatabase::getInventoryCache() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList list;
    const int userId = currentUserIdLocked();
    if (userId <= 0)
        return list;

    QSqlQuery query(m_db);
    query.prepare("SELECT data FROM inventory_cache WHERE user_id = ? ORDER BY ordinal");
    query.addBindValue(userId);
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 读取库存快照失败:" << query.lastError().text();
        return list;
    }
    while (query.next()) {
        list.append(QJsonDocument::fromJson(query.value(0).toByteArray()).object().toVariantMap());
    }
    return list;
}

bool LocalDatabase::hasInventoryCache() const
{
    QMutexLocker locker(&m_mutex);
    const int userId = currentUserIdLocked();
    if (userId <= 0)
        return false;

    QSqlQuery query(m_db);
    query.prepare("SELECT 1 FROM inventory_cache_meta WHERE user_id = ?");
    query.addBindValue(userId);
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 查询库存快照标记失败:" << query.lastError().text();
        return false;
    }
    return query.next();
}

bool LocalDatabase::removeInventoryCacheItem(const QString &ingredientName, const QString &unit)
{
    QMutexLocker locker(&m_mutex);
    const int userId = currentUserIdLocked();
    if (userId <= 0) {
        qWarning() << "[LocalDatabase] 未登录，拒绝移除库存快照行";
        return false;
    }
    // 复合键不完整无法定位行（防御：调用方应传服务端契约字段值）
    if (ingredientName.isEmpty() || unit.isEmpty())
        return false;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM inventory_cache WHERE user_id = ? AND ingredient_name = ? AND unit = ?");
    query.addBindValue(userId);
    query.addBindValue(ingredientName);
    query.addBindValue(unit);
    if (!query.exec()) {
        qWarning() << "[LocalDatabase] 移除库存快照行失败（" << ingredientName << unit << "）:"
                   << query.lastError().text();
        return false;
    }
    return true;
}

int LocalDatabase::currentUserIdLocked() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM user LIMIT 1"))
        return 0;
    if (query.next())
        return query.value(0).toInt();
    return 0;
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

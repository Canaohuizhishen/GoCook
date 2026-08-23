#include "PgAnnouncementRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"
#include "../common/DbExecutor.h"

using namespace gocook::repository;
using namespace gocook::services;
using namespace gocook::models;

//   本文件已按"生产级收敛形态"重构：方法用 executeDb 包裹（见 ../common/DbExecutor.h），
//   异常分层/事务边界由辅助函数统一保证。

PagedAnnouncements PgAnnouncementRepository::findAll(int page, int size) {
    return executeDb(db_, [&](pqxx::work& txn) {
        pqxx::result countRes = txn.exec(
            "SELECT COUNT(*) FROM announcements");
        int total = countRes[0][0].as<int>();

        int offset = (page > 0) ? (page - 1) * size : 0;

        pqxx::result rows = txn.exec(
            "SELECT id, title, content, created_at "
            "FROM announcements ORDER BY created_at DESC LIMIT $1 OFFSET $2",
            pqxx::params{size, offset});

        PagedAnnouncements result;
        for (const auto& row : rows) {
            AnnouncementItem item;
            item.id = row["id"].as<int>();
            item.title = row["title"].c_str();
            if (!row["content"].is_null())
                item.content = row["content"].c_str();
            item.created_at = row["created_at"].c_str();
            result.data.push_back(std::move(item));
        }

        result.pagination.page = page;
        result.pagination.size = size;
        result.pagination.total = total;
        result.pagination.total_pages = safeTotalPages(total, size);

        return result;
    }, "数据库操作失败");
}

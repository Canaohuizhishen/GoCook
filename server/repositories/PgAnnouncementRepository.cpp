#include "PgAnnouncementRepository.h"
#include <pqxx/pqxx>
#include <gocook/IServices.h>
#include "../common/Logger.h"

using namespace gocook::repository;
using namespace gocook::services;
using namespace gocook::models;

PagedAnnouncements PgAnnouncementRepository::findAll(int page, int size) {
    try {
        auto conn = db_.getConnection();
        pqxx::work txn(*conn);

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
        result.pagination.total_pages = (total + size - 1) / size;

        txn.commit();
        return result;
    } catch (const ServiceException&) {
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Database error: %s", e.what());
        throw ServiceException("数据库操作失败");
    }
}

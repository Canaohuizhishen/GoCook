#pragma once

#include <nlohmann/json.hpp>
#include <gocook/DataModels.h>

/**
 * @brief 将分页元信息序列化为统一 JSON 对象
 *
 * 所有 Handler 共用此定义，避免在每个文件中重复粘贴相同代码。
 * 输出字段：page / size / total / total_pages。
 */
inline nlohmann::json toJson(const gocook::models::Pagination& pag) {
    return {
        {"page", pag.page},
        {"size", pag.size},
        {"total", pag.total},
        {"total_pages", pag.total_pages}
    };
}

#include "AnnouncementHandler.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "../common/ErrorHelper.h"

using json = nlohmann::json;
using namespace gocook::models;

static json toJson(const AnnouncementItem& ann) {
    return {
        {"id", ann.id},
        {"title", ann.title},
        {"content", ann.content},
        {"created_at", ann.created_at}
    };
}

static json toJson(const Pagination& pag) {
    return {
        {"page", pag.page},
        {"size", pag.size},
        {"total", pag.total},
        {"total_pages", pag.total_pages}
    };
}

AnnouncementHandler::AnnouncementHandler(gocook::services::IAnnouncementService& service)
    : service_(service)
{
}

void AnnouncementHandler::getAnnouncements(const httplib::Request& req, httplib::Response& res)
{
    try {
        int page = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 1;
        int size = req.has_param("size") ? std::stoi(req.get_param_value("size")) : 5;
        auto paged = service_.getAnnouncements(page, size);
        json resp;
        resp["data"] = json::array();
        for (const auto& ann : paged.data)
            resp["data"].push_back(toJson(ann));
        resp["pagination"] = toJson(paged.pagination);
        res.set_header("Content-Type", "application/json");
        res.status = 200;
        res.body = resp.dump();
    } catch (const gocook::services::ServiceException& e) {
        handleStandardException(e, res);
    } catch (const std::exception& e) {
        handleStandardException(e, res);
    }
}
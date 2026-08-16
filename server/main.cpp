#include <httplib/httplib.h>
#include <csignal>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <memory>
#include "common/Config.h"
#include "common/Logger.h"
#include "common/ConnectionPool.h"
#include "repositories/PgUserRepository.h"
#include "repositories/PgRecipeRepository.h"
#include "repositories/PgInventoryRepository.h"
#include "repositories/PgMealPlanRepository.h"
#include "repositories/PgAnnouncementRepository.h"
#include "repositories/PgAdminRepository.h"
#include "services/RecipeServiceImpl.h"
#include "services/UserServiceImpl.h"
#include "services/InventoryServiceImpl.h"
#include "services/MealPlanServiceImpl.h"
#include "services/AnnouncementServiceImpl.h"
#include "services/AdminServiceImpl.h"
#include "handlers/RecipeHandler.h"
#include "handlers/UserHandler.h"
#include "handlers/InventoryHandler.h"
#include "handlers/MealPlanHandler.h"
#include "handlers/AnnouncementHandler.h"
#include "handlers/AdminHandler.h"
#include "middleware/auth_middleware.h"
#include "Router.h"

namespace {
    std::atomic<bool> gRunning{true};

    void signalHandler(int sig) {
        LOG_INFO("Received signal %d, shutting down...", sig);
        gRunning = false;
    }
}

int main(int argc, char* argv[]) {
    try {
        auto cfg = Config::load();

        // CLI 端口参数：./server [port] 优先级高于环境变量/.env（便于本地多实例调试与端口冲突排查）
        if (argc >= 2) {
            char* end = nullptr;
            long cliPort = std::strtol(argv[1], &end, 10);
            if (end != argv[1] && *end == '\0' && cliPort > 0 && cliPort <= 65535) {
                cfg.port = static_cast<int>(cliPort);
                LOG_INFO("Using CLI port argument: %d", cfg.port);
            } else {
                fprintf(stderr, "Usage: %s [port]\n", argv[0]);
                fflush(stderr);
                return 2;
            }
        }

    auto url = (cfg.host == "0.0.0.0")
        ? "http://127.0.0.1:" + std::to_string(cfg.port) + "/"
        : "http://" + cfg.host + ":" + std::to_string(cfg.port) + "/";
    LOG_INFO("GoCook server starting on %s", url.c_str());

    if (!cfg.tlsCertPath.empty()) {
        LOG_WARN("TLS cert configured but CPPHTTPLIB_OPENSSL_SUPPORT not enabled, "
                 "falling back to HTTP. Add -DCPPHTTPLIB_OPENSSL_SUPPORT to enable HTTPS.");
    }

    ConnectionPool db(cfg.dbConnString, cfg.dbPoolSize);
    AuthMiddleware authMiddleware(cfg.jwtSecret);

    RecipeServiceImpl recipeService(
        std::make_unique<PgRecipeRepository>(db),
        std::make_unique<PgUserRepository>(db),
        std::make_unique<PgInventoryRepository>(db));
    UserServiceImpl userService(std::make_unique<PgUserRepository>(db), cfg.jwtSecret);
    InventoryServiceImpl inventoryService(std::make_unique<PgInventoryRepository>(db));
    MealPlanServiceImpl mealPlanService(std::make_unique<PgMealPlanRepository>(db));
    AnnouncementServiceImpl announcementService(std::make_unique<PgAnnouncementRepository>(db));
    AdminServiceImpl adminService(std::make_unique<PgAdminRepository>(db));

    RecipeHandler recipeHandler(recipeService, authMiddleware);
    UserHandler userHandler(userService, authMiddleware);
    InventoryHandler inventoryHandler(inventoryService, authMiddleware);
    MealPlanHandler mealPlanHandler(mealPlanService, authMiddleware);
    AnnouncementHandler announcementHandler(announcementService);
    AdminHandler adminHandler(adminService, authMiddleware);

    Router router(db, recipeHandler, userHandler, inventoryHandler,
                  mealPlanHandler, announcementHandler, adminHandler);

    httplib::Server svr;
    // 请求日志：每个请求一行（方法/路径/对端 IP/状态码），运维排查必备
    // （Router 里原有的请求日志是 LOG_DEBUG 级，默认 logLevel=info 不可见）
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        LOG_INFO("HTTP %s %s from %s -> %d", req.method.c_str(), req.path.c_str(),
                 req.remote_addr.c_str(), res.status);
    });
    // 覆盖 httplib 默认 socket 选项：Linux 下默认只设 SO_REUSEPORT，
    // 两个进程可同时监听同一端口（NetDemo ⑦ 事故：连接被轮流分配、token 随机 401）。
    // 改为仅 SO_REUSEADDR：第二个实例 bind 必然失败，杜绝静默双实例。
    svr.set_socket_options([](socket_t sock) {
#ifdef SO_REUSEADDR
        int reuse = 1;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const void*>(&reuse), sizeof(reuse));
#else
        (void)sock;
#endif
    });
    router.setupRoutes(svr);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::atomic<bool> gListenOk{true};
    std::thread serverThread([&]() {
        LOG_INFO("Starting HTTP server on %s", url.c_str());
        if (!svr.listen(cfg.host.c_str(), cfg.port)) {
            // httplib 默认静默失败（连 stderr 都不打），必须自己报错并退出，
            // 否则主循环永久空转、进程挂死不服务（比 NetDemo ③ 更糟）。
            LOG_ERROR("Failed to bind/listen on %s:%d (端口被占用或权限不足?)", cfg.host.c_str(), cfg.port);
            gListenOk = false;
            gRunning = false; // 唤醒主循环退出
        }
    });

    LOG_INFO("Server is running. Press Ctrl+C to stop.");

    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("Stopping server...");
    svr.stop();
    serverThread.join();
    LOG_INFO("Server stopped gracefully.");

    fflush(stderr);
    fflush(stdout);
    _Exit(gListenOk.load() ? 0 : 1);
    } catch (const std::exception& e) {
        fprintf(stderr, "FATAL: %s\n", e.what());
        fflush(stderr);
        _Exit(1);
    }
}

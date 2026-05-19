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

int main() {
    try {
        auto cfg = Config::load();
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

    RecipeServiceImpl recipeService(std::make_unique<PgRecipeRepository>(db));
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
    router.setupRoutes(svr);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::thread serverThread([&]() {
        LOG_INFO("Starting HTTP server on %s", url.c_str());
        svr.listen(cfg.host.c_str(), cfg.port);
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
    _Exit(0);
    } catch (const std::exception& e) {
        fprintf(stderr, "FATAL: %s\n", e.what());
        fflush(stderr);
        _Exit(1);
    }
}

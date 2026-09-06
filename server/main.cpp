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
#include "repositories/PgIngredientNutritionRepository.h"
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

    // 信号处理器仅修改原子标志 gRunning，不做复杂操作（规避死锁）；
    // 真正的资源回收（svr.stop、join 线程）交由主循环执行。
    void signalHandler(int sig) {
        LOG_INFO("收到信号 %d，正在关闭服务……", sig);
        gRunning = false;
    }
}

int main(int argc, char* argv[]) {
    try {
        auto cfg = Config::load();

        // CLI 端口参数：./server [port] 优先级高于环境变量/.env（便于本地多实例调试与端口冲突排查）
        if (argc >= 2) {
            // strtol（String to Long）在尝试把字符串转成数字时，会把第一个无法转换的字符的地址存到 end 里。
            char* end = nullptr;
            long cliPort = std::strtol(argv[1], &end, 10);

            // 前两条件确保 argv[1] 必须是纯数字，后两条件确保范围在[1,65535]
            if (end != argv[1] && *end == '\0' && cliPort > 0 && cliPort <= 65535) {
                cfg.port = static_cast<int>(cliPort);
                LOG_INFO("使用命令行端口参数：%d", cfg.port);
            } else { // 非法参数时的错误处理出口
                fprintf(stderr, "用法：%s [端口]\n", argv[0]);
                fflush(stderr);
                return 2; // 退出码 2 专门用于“命令行参数用法错误”
            }
        }

    // 打印启动地址
    auto url = (cfg.host == "0.0.0.0")
        ? "http://127.0.0.1:" + std::to_string(cfg.port) + "/"
        : "http://" + cfg.host + ":" + std::to_string(cfg.port) + "/";
    LOG_INFO("GoCook 服务端正在启动，地址：%s", url.c_str());

    // TLS 降级警告
    if (!cfg.tlsCertPath.empty()) {
        LOG_WARN("已配置 TLS 证书，但未启用 CPPHTTPLIB_OPENSSL_SUPPORT，将回退到 HTTP。"
                 "如需 HTTPS，请添加 -DCPPHTTPLIB_OPENSSL_SUPPORT 后重新编译。");
    }

    /* 依赖注入装配
     *
     * 依赖方向：
     *   Repository 依赖 ConnectionPool（通过构造函数传入）；
     *   Service 依赖 IRepository 接口（通过 unique_ptr<IRepository> 注入），不依赖具体实现；
     *   Handler 依赖 IService 接口（通过引用注入），同时依赖 AuthMiddleware 做认证；
     *   Router 持有所有 Handler 的引用，负责注册路由。
     *   整个依赖链严格面向接口，符合依赖倒置原则。
     */
    ConnectionPool db(cfg.dbConnString, cfg.dbPoolSize);
    AuthMiddleware authMiddleware(cfg.jwtSecret);

    RecipeServiceImpl recipeService(
        std::make_unique<PgRecipeRepository>(db),
        std::make_unique<PgUserRepository>(db),
        std::make_unique<PgInventoryRepository>(db),
        std::make_unique<PgIngredientNutritionRepository>(db) // 增强而非强依赖，若未注入，Service 内部会有降级逻辑
        );
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

    // 注册 HTTP 请求生命周期回调：记录结构化访问日志，每个请求一行（方法/路径/对端 IP/状态码），运维排查必备
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        LOG_INFO("HTTP %s %s（来源 %s）→ %d", req.method.c_str(), req.path.c_str(),
                 req.remote_addr.c_str(), res.status);
    });

    // 覆盖 httplib 默认 socket 选项：Linux 下默认只设 SO_REUSEPORT，两个进程可同时监听同一端口。
    // 改为仅 SO_REUSEADDR：第二个实例 bind 必然失败，杜绝静默双实例。
    svr.set_socket_options([](socket_t sock) {
#ifdef SO_REUSEADDR // 条件编译可以避免在非 Linux 平台（或环境缺失头文件）时编译失败
        int reuse = 1;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const void*>(&reuse), sizeof(reuse));
#else
        (void)sock; //标准的‘未使用参数’抑制写法
#endif
    });

    // 业务路由注册
    router.setupRoutes(svr);

    // 注册信号处理函数，用于捕获 Ctrl+C 与系统终止事件，以触发服务关闭流程。
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 原子布尔变量，用于跨线程安全地记录“监听是否成功”。
    std::atomic<bool> gListenOk{true};

    // httplib::listen() 会阻塞当前线程，必须放到独立线程中执行
    std::thread serverThread([&]() {
        LOG_INFO("HTTP 服务开始监听 %s", url.c_str());
        if (!svr.listen(cfg.host.c_str(), cfg.port)) {
            // httplib 监听失败时静默返回 false（不抛异常、不打日志）
            // 若不处理，主线程会永远卡在 while(gRunning) 循环中，进程挂死不服务
            LOG_ERROR("绑定/监听失败：%s:%d（端口被占用或权限不足）", cfg.host.c_str(), cfg.port);
            gListenOk = false; // 记录失败状态，最终返回非 0 退出码
            gRunning = false; // 唤醒主循环退出
        }
    });

    LOG_INFO("服务已启动。按 Ctrl+C 停止。");

    // 主循环等待信号（SIGINT/SIGTERM）或监听失败触发的 gRunning=false。
    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 执行优雅停机：停止接收新请求，等待已有请求处理完成。
    LOG_INFO("正在停止服务……");
    svr.stop();

    // 等待后台监听线程完全退出，确保资源释放。
    serverThread.join();
    LOG_INFO("服务已正常停止。");

    // 手动刷出日志缓冲区：_Exit() 不会自动刷新 stdio，务必在此落盘，防止启动错误信息丢失。
    fflush(stderr);
    fflush(stdout);

    // 使用_Exit()直接终止进程：跳过全局对象的析构（避免多线程环境下销毁顺序错乱导致死锁或二次崩溃），让 OS 直接回收资源。
    // 根据原子变量 gListenOk 的最终状态（.load()），向父进程返回退出码（0 成功 / 1 失败）。
    _Exit(gListenOk.load() ? 0 : 1);
    } catch (const std::exception& e) {
        fprintf(stderr, "致命错误：%s\n", e.what());
        fflush(stderr);
        _Exit(1);
    }
}

#include "./third_party/httplib/httplib.h"
#include <iostream>
#include "DBConnection.h"
#include "RecipeHandler.h"
#include "UserHandler.h"
#include "InventoryHandler.h"
#include "Router.h"

int main() {
    std::string connStr = "dbname=gocookdb user=gocook password=gocook123 host=127.0.0.1 port=5432";
    DBConnection db(connStr);
    if (!db.connect()) {
        std::cerr << "Cannot connect to database.\n";
        return 1;
    }

    RecipeHandler recipeHandler(db);
    UserHandler userHandler(db);
    InventoryHandler inventoryHandler(db);
    // 后续创建其他 Handler

    Router router(db, recipeHandler, userHandler, inventoryHandler /*, userHandler, ... */);

    httplib::Server svr;
    router.setupRoutes(svr);   // 一行注册所有路由

    std::cout << "Server started on http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    return 0;
}
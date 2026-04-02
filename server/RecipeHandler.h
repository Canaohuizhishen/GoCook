#pragma once

#include "./third_party/httplib/httplib.h"
#include "DBConnection.h"

class RecipeHandler
{
public:
    RecipeHandler(DBConnection& db);
    void getRecipes(const httplib::Request& req, httplib::Response& res);
    void getRecipesPublic(const httplib::Request& req, httplib::Response& res);
    // 其他方法...

private:
    DBConnection& db_;
};

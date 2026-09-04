#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../services/InventoryServiceImpl.h"
#include "MockInventoryRepository.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <unordered_set>

using namespace testing;
using namespace gocook::models;
using namespace gocook::services;
using namespace gocook::repository;

namespace {
    PagedInventory makePagedInventory(int count = 2) {
        PagedInventory result;
        for (int i = 1; i <= count; ++i)
            result.data.push_back({i, "Ingredient " + std::to_string(i),
                                   1.0 * i, "kg", std::nullopt, "2026-01-0" + std::to_string(i)});
        result.pagination = {1, count, 5, 1};
        return result;
    }

    UpsertInventoryRequest makeUpsertReq() {
        return {"Tomato", 3.0, "个", std::nullopt};
    }
}

// ==================== 已实现的方法 ====================

TEST(InventoryServiceTest, 获取库存正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    auto expected = makePagedInventory(2);
    EXPECT_CALL(*repo, findInventoryFiltered(1, 1, 20, "")).WillOnce(Return(expected));

    auto result = service.getInventory(1, 1, 20);
    EXPECT_EQ(result.data.size(), 2);
    EXPECT_EQ(result.data[0].ingredient_name, "Ingredient 1");
    EXPECT_EQ(result.data[1].ingredient_name, "Ingredient 2");
}

TEST(InventoryServiceTest, 库存分页参数透传) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, findInventoryFiltered(2, 3, 10, ""))
        .WillOnce(Return(PagedInventory{}));

    auto result = service.getInventory(2, 3, 10);
    EXPECT_TRUE(result.data.empty());
}

TEST(InventoryServiceTest, 库存关键字过滤透传) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    // 过滤词必须原样透传给仓库层（库存页过滤框场景）
    EXPECT_CALL(*repo, findInventoryFiltered(1, 1, 20, "料酒"))
        .WillOnce(Return(PagedInventory{}));

    auto result = service.getInventory(1, 1, 20, "料酒");
    EXPECT_TRUE(result.data.empty());
}

TEST(InventoryServiceTest, 更新库存正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    auto req = makeUpsertReq();
    EXPECT_CALL(*repo, upsertInventory(1, Truly([](const auto& r) {
        return r.ingredient_name == "Tomato" && r.quantity == 3.0;
    }))).WillOnce(Return(42));

    int id = service.upsertInventory(1, req);
    EXPECT_EQ(id, 42);
}

TEST(InventoryServiceTest, 删除库存正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(*repo, deleteInventoryItem(1, 5)).Times(1);
    EXPECT_NO_THROW(service.deleteInventoryItem(1, 5));
}

// ==================== 库存校验边界 ====================

TEST(InventoryServiceTest, 库存数量为零拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 0.0;
    req.unit = "个";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("必须大于0"));
    }
}

TEST(InventoryServiceTest, 库存数量为负拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = -1.0;
    req.unit = "个";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("必须大于0"));
    }
}

TEST(InventoryServiceTest, 食材单位不合法拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "xyz";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("单位不合法"));
    }
}

// ==================== 过期日期校验边界 ====================

TEST(InventoryServiceTest, 过期日期格式非法拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "个";
    req.expiry_date = "2026/01/01";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("格式无效"));
    }
}

TEST(InventoryServiceTest, 过期日期非闰年2月29日拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "个";
    req.expiry_date = "2025-02-29";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("过期日期无效"));
    }
}

TEST(InventoryServiceTest, 过期日期不存在的月日拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "个";
    req.expiry_date = "2026-02-31";

    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("过期日期无效"));
    }

    req.expiry_date = "2026-04-31";
    try {
        service.upsertInventory(1, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_THAT(e.what(), testing::HasSubstr("过期日期无效"));
    }
}

TEST(InventoryServiceTest, 过期日期闰年2月29日正常委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "个";
    req.expiry_date = "2024-02-29";

    EXPECT_CALL(*repo, upsertInventory(1, Truly([](const auto& r) {
        return r.expiry_date.has_value() && r.expiry_date.value() == "2024-02-29";
    }))).WillOnce(Return(42));

    EXPECT_EQ(service.upsertInventory(1, req), 42);
}

TEST(InventoryServiceTest, 过期日期合法正常委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    UpsertInventoryRequest req;
    req.ingredient_name = "测试食材";
    req.quantity = 1.0;
    req.unit = "个";
    req.expiry_date = "2026-05-10";

    EXPECT_CALL(*repo, upsertInventory(1, Truly([](const auto& r) {
        return r.expiry_date.has_value() && r.expiry_date.value() == "2026-05-10";
    }))).WillOnce(Return(42));

    EXPECT_EQ(service.upsertInventory(1, req), 42);
}

// ==================== 未实现的方法 ====================

TEST(InventoryServiceTest, 购物清单列表正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    std::vector<gocook::models::ShoppingListSummary> expected;
    expected.push_back({1, "周末采购", 3, "2026-05-10T14:30:00Z"});
    EXPECT_CALL(*repo, findShoppingLists(1)).WillOnce(Return(expected));

    auto result = service.getShoppingLists(1);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].name, "周末采购");
    EXPECT_EQ(result[0].item_count, 3);
}

TEST(InventoryServiceTest, 创建购物清单正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    gocook::models::CreateShoppingListRequest req;
    req.name = "周末采购";
    EXPECT_CALL(*repo, createShoppingList(1, Truly([](const auto& r) {
        return r.name == "周末采购";
    }))).WillOnce(Return(42));

    int id = service.createShoppingList(1, req);
    EXPECT_EQ(id, 42);
}

TEST(InventoryServiceTest, 编辑库存项正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = "料酒";
    req.quantity = 45.0;
    req.unit = "毫升";

    // 按 id 替换语义：原样透传给仓库层
    EXPECT_CALL(*repo, updateInventoryItem(1, 7, Truly([](const auto& r) {
        return r.ingredient_name == "料酒" && r.quantity == 45.0 && r.unit == "毫升";
    }))).Times(1);

    service.updateInventoryItem(1, 7, req);
}

TEST(InventoryServiceTest, 编辑库存项数量非法拒绝) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    gocook::models::UpsertInventoryRequest req;
    req.ingredient_name = "料酒";
    req.quantity = 0.0;
    req.unit = "毫升";

    EXPECT_CALL(*repo, updateInventoryItem(_, _, _)).Times(0);

    try {
        service.updateInventoryItem(1, 7, req);
        FAIL() << "Expected ServiceException";
    } catch (const ServiceException& e) {
        EXPECT_EQ(e.statusCode(), 400);
        EXPECT_STREQ(e.what(), "库存数量必须大于0");
    }
}

TEST(InventoryServiceTest, 购物清单详情正确委派) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto* repo = mock.get();
    InventoryServiceImpl service(std::move(mock));

    gocook::models::ShoppingList expected;
    expected.id = 42;
    expected.name = "周末采购";
    EXPECT_CALL(*repo, findShoppingListDetail(1, 42))
        .WillOnce(Return(expected));

    auto result = service.getShoppingListDetail(1, 42);
    EXPECT_EQ(result.id, 42);
    EXPECT_EQ(result.name, "周末采购");
}

TEST(InventoryServiceTest, 删除清单正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(repo, deleteShoppingList(1, 42))
        .Times(1);

    service.deleteShoppingList(1, 42);
}

TEST(InventoryServiceTest, 更新清单项正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    UpdateShoppingItemRequest req;
    req.checked = true;

    EXPECT_CALL(repo, updateShoppingListItem(1, 42, 7, _))
        .Times(1);

    service.updateShoppingListItem(1, 42, 7, req);
}

TEST(InventoryServiceTest, 批量添加清单正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    std::vector<BatchShoppingItem> items;
    items.push_back({"盐", 1.0, "袋"});

    EXPECT_CALL(repo, batchAddShoppingItems(1, 42, _))
        .WillOnce(Return(BatchShoppingResponse{}));

    auto result = service.batchAddShoppingItems(1, 42, items);
    EXPECT_EQ(result.message, "");
}

TEST(InventoryServiceTest, 导出购物清单正确委派Repositories) {
    auto mock = std::make_unique<NiceMock<MockInventoryRepository>>();
    auto& repo = *mock;
    InventoryServiceImpl service(std::move(mock));

    EXPECT_CALL(repo, exportShoppingList(1, 42, "text"))
        .WillOnce(Return(std::string("GoCook 购物清单：test\n\n[ ] item  1个\n")));

    auto result = service.exportShoppingList(1, 42, "text");
    EXPECT_EQ(result, "GoCook 购物清单：test\n\n[ ] item  1个\n");
}

// ==================== 单位白名单双端一致性守卫 ====================
// InventoryServiceImpl::validUnits()（服务端唯一事实源）与客户端 InventoryPage.qml 的
// supportedUnits（提交前拦截副本）必须逐项一致：任一单侧增/删/改单位都会在本用例失败，
// 强制两侧同步（此前仅靠注释约束，无机制防漂移）。
TEST(InventoryServiceTest, 单位白名单与客户端QML逐项一致) {
    // 经 __FILE__ 逐级上溯定位仓库根下的 InventoryPage.qml：兼容 __FILE__ 为绝对/相对路径
    // （编译命令形态不定）以及测试运行 CWD 任意的情况——找到即用，找不到给出明确错误
    std::filesystem::path qmlPath;
    std::filesystem::path p = std::filesystem::path(__FILE__).parent_path();
    for (int i = 0; i < 8 && !p.empty(); ++i, p = p.parent_path()) {
        auto cand = p / "client" / "qml" / "pages" / "InventoryPage.qml";
        if (std::filesystem::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }
    ASSERT_FALSE(qmlPath.empty())
        << "找不到客户端白名单源文件 client/qml/pages/InventoryPage.qml"
        << "（已从 " << std::filesystem::path(__FILE__).parent_path()
        << " 向上搜索 8 层；源码布局变更需同步本用例的定位逻辑）";

    std::ifstream in(qmlPath);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // 拉平换行（supportedUnits 数组跨行书写，ECMAScript 正则的 . 不匹配 \n）
    for (char& c : content)
        if (c == '\n') c = ' ';

    std::regex listRe(R"(supportedUnits:\s*\[(.*?)\])");
    std::smatch listM;
    ASSERT_TRUE(std::regex_search(content, listM, listRe))
        << "InventoryPage.qml 中未找到 supportedUnits 数组（QML 结构变更需同步本用例）";
    std::regex unitRe(R"q("([^"]*)")q");
    std::unordered_set<std::string> qmlUnits;
    for (std::sregex_iterator it(listM[1].first, listM[1].second, unitRe), end; it != end; ++it)
        qmlUnits.insert((*it)[1].str());

    const auto& serverUnits = InventoryServiceImpl::validUnits();
    ASSERT_FALSE(serverUnits.empty()) << "服务端单位白名单不应为空";
    ASSERT_FALSE(qmlUnits.empty()) << "客户端 supportedUnits 不应为空";

    std::ostringstream onlyInQml, onlyInServer;
    for (const auto& u : qmlUnits)
        if (!serverUnits.count(u)) onlyInQml << u << " ";
    for (const auto& u : serverUnits)
        if (!qmlUnits.count(u)) onlyInServer << u << " ";
    EXPECT_TRUE(onlyInQml.str().empty())
        << "以下单位仅存在于客户端 QML（服务端缺，须两侧同步）: " << onlyInQml.str();
    EXPECT_TRUE(onlyInServer.str().empty())
        << "以下单位仅存在于服务端（客户端缺，须两侧同步）: " << onlyInServer.str();
    EXPECT_EQ(serverUnits.size(), qmlUnits.size()) << "两侧单位数量不一致";
}

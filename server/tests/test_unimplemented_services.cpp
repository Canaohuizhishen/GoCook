#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gocook/IServices.h>
#include <gocook/IMealPlanRepository.h>
#include <gocook/IAnnouncementRepository.h>
#include <gocook/IAdminRepository.h>
#include "../services/MealPlanServiceImpl.h"
#include "../services/AnnouncementServiceImpl.h"
#include "../services/AdminServiceImpl.h"

using namespace testing;
using namespace gocook::services;
using namespace gocook::models;

// ==================== MealPlanService ====================

class MockMealPlanRepository : public gocook::repository::IMealPlanRepository {
public:
    MOCK_METHOD(int, createMealPlan, (int, const MealPlanRequest&), (override));
    MOCK_METHOD(MealPlansResponse, findMealPlans,
                (int, const std::string&, const std::string&, int, int), (override));
    MOCK_METHOD(PagedCalendarDays, findMealPlanDetail,
                (int, const std::string&, const std::string&, int, int), (override));
    MOCK_METHOD(void, updateMealPlan, (int, int, const MealPlanRequest&), (override));
    MOCK_METHOD(void, deleteMealPlan, (int, int), (override));
    MOCK_METHOD(NutritionTrendResponse, findNutritionTrend,
                (int, const std::string&, const std::string&), (override));
};

TEST(MealPlanServiceTest, 全部方法返回501) {
    auto mock = std::make_unique<NiceMock<MockMealPlanRepository>>();
    MealPlanServiceImpl service(std::move(mock));

    EXPECT_THROW(service.createMealPlan(1, {}), ServiceException);
    EXPECT_THROW(service.getMealPlans(1, "", "", 1, 20), ServiceException);
    EXPECT_THROW(service.getMealPlanDetail(1, "", "", 1, 20), ServiceException);
    EXPECT_THROW(service.updateMealPlan(1, 1, {}), ServiceException);
    EXPECT_THROW(service.deleteMealPlan(1, 1), ServiceException);
    EXPECT_THROW(service.getNutritionTrend(1, "", ""), ServiceException);
}

// ==================== AnnouncementService ====================

class MockAnnouncementRepository : public gocook::repository::IAnnouncementRepository {
public:
    MOCK_METHOD(PagedAnnouncements, findAll, (int, int), (override));
};

TEST(AnnouncementServiceTest, 公告方法正确委派) {
    auto mock = std::make_unique<NiceMock<MockAnnouncementRepository>>();
    auto* repo = mock.get();
    AnnouncementServiceImpl service(std::move(mock));

    PagedAnnouncements expected;
    expected.data.push_back({1, "维护通知", "今晚系统升级", "2026-04-21T10:00:00Z"});
    expected.pagination = {1, 5, 1, 1};
    EXPECT_CALL(*repo, findAll(1, 5)).WillOnce(Return(expected));

    auto result = service.getAnnouncements(1, 5);
    EXPECT_EQ(result.data.size(), 1);
    EXPECT_EQ(result.data[0].title, "维护通知");
    EXPECT_EQ(result.pagination.total, 1);
}

// ==================== AdminService ====================

class MockAdminRepository : public gocook::repository::IAdminRepository {
public:
    MOCK_METHOD(PagedUsers, findUsers, (int, int, const nlohmann::json&), (override));
    MOCK_METHOD(void, createUser, (const CreateUserRequest&), (override));
    MOCK_METHOD(void, updateUser, (int, const UpdateUserRequest&), (override));
    MOCK_METHOD(void, setUserStatus, (int, const SetUserStatusRequest&), (override));
    MOCK_METHOD(void, deleteUser, (int), (override));
    MOCK_METHOD(PagedPendingRecipes, findPendingRecipes, (int, int), (override));
    MOCK_METHOD(void, approveRecipe, (int), (override));
    MOCK_METHOD(void, rejectRecipe, (int, const RejectRecipeRequest&), (override));
    MOCK_METHOD(BatchReviewResponse, batchReviewRecipes, (const BatchReviewRequest&), (override));
    MOCK_METHOD(void, publishAnnouncement, (const AnnouncementRequest&), (override));
    MOCK_METHOD(NotificationResponse, sendNotification, (const NotificationRequest&), (override));
    MOCK_METHOD(StatisticsData, findStatistics, (), (override));
    MOCK_METHOD(PagedAdminLogs, findAdminLogs, (int, int, const std::string&, int), (override));
    MOCK_METHOD(PagedActivityLogs, findActivityLogs, (int, int, int, const std::string&), (override));
};

TEST(AdminServiceTest, 全部方法返回501) {
    auto mock = std::make_unique<NiceMock<MockAdminRepository>>();
    AdminServiceImpl service(std::move(mock));

    EXPECT_THROW(service.getUsers(1, 20, {}), ServiceException);
    EXPECT_THROW(service.createUser({}), ServiceException);
    EXPECT_THROW(service.updateUser(1, {}), ServiceException);
    EXPECT_THROW(service.setUserStatus(1, {}), ServiceException);
    EXPECT_THROW(service.deleteUser(1), ServiceException);
    EXPECT_THROW(service.getPendingRecipes(1, 20), ServiceException);
    EXPECT_THROW(service.approveRecipe(1), ServiceException);
    EXPECT_THROW(service.rejectRecipe(1, {}), ServiceException);
    EXPECT_THROW(service.batchReviewRecipes({}), ServiceException);
    EXPECT_THROW(service.publishAnnouncement({}), ServiceException);
    EXPECT_THROW(service.sendNotification({}), ServiceException);
    EXPECT_THROW(service.getStatistics(), ServiceException);
    EXPECT_THROW(service.getAdminLogs(1, 20), ServiceException);
    EXPECT_THROW(service.getActivityLogs(1, 20), ServiceException);
}

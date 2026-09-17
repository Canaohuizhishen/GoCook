// RequestGuards 值类型测试：代次票据语义 + 会话快照判定。
//
// 覆盖两类"过期响应作废"判定的最小语义（各 ViewModel 迁移后共用同一实现）：
//   G1  RequestEpoch：新一轮票据有效、旧票据失效、invalidate 作废全部在途
//   G2  RequestEpoch：续页沿用当前轮次（current 不递增，票据仍有效）
//   G3  SessionSnapshot：会话未变判定为当前（含游客空 token 快照）
//   G4  SessionSnapshot：换号后旧快照失效
//   G5  SessionSnapshot：游客快照在登录后失效（空 → 非空同样属会话切换）

#include <gtest/gtest.h>

#include "RequestGuards.h"
#include "HttpGoCookApi.h"

TEST(RequestEpochTest, 新一轮票据有效旧票据失效)
{
    RequestEpoch epoch;
    const int t1 = epoch.begin();
    EXPECT_EQ(epoch.current(), t1);
    EXPECT_TRUE(epoch.isCurrent(t1));

    const int t2 = epoch.begin();   // 开启新一轮
    EXPECT_FALSE(epoch.isCurrent(t1));   // 旧轮次票据失效
    EXPECT_TRUE(epoch.isCurrent(t2));

    epoch.invalidate();   // 作废全部在途（登出/清空场景）
    EXPECT_FALSE(epoch.isCurrent(t2));
}

TEST(RequestEpochTest, 续页沿用当前轮次票据)
{
    RequestEpoch epoch;
    const int t1 = epoch.begin();
    const int follow = epoch.current();   // 续页不开启新轮次
    EXPECT_EQ(follow, t1);
    EXPECT_TRUE(epoch.isCurrent(follow));

    // 续页在途期间换词（新轮次）→ 续页响应到达时票据已失效
    epoch.begin();
    EXPECT_FALSE(epoch.isCurrent(follow));
}

TEST(SessionSnapshotTest, 会话未变判定为当前)
{
    HttpGoCookApi api;
    api.setToken("token-a");
    const auto snap = SessionSnapshot::capture(&api);
    EXPECT_TRUE(snap.isCurrent(&api));

    // 游客空 token 快照同样成立
    HttpGoCookApi guestApi;
    const auto guestSnap = SessionSnapshot::capture(&guestApi);
    EXPECT_TRUE(guestSnap.isCurrent(&guestApi));
}

TEST(SessionSnapshotTest, 换号后旧快照失效)
{
    HttpGoCookApi api;
    api.setToken("token-a");
    const auto snap = SessionSnapshot::capture(&api);

    api.setToken("token-b");   // 换号
    EXPECT_FALSE(snap.isCurrent(&api));

    api.setToken("");   // 登出同样视为会话切换
    EXPECT_FALSE(snap.isCurrent(&api));
}

TEST(SessionSnapshotTest, 游客快照在登录后失效)
{
    HttpGoCookApi api;   // 游客（空 token）
    const auto snap = SessionSnapshot::capture(&api);

    api.setToken("token-a");   // 登录：空 → 非空同样是会话切换
    EXPECT_FALSE(snap.isCurrent(&api));
}

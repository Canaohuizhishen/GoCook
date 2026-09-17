#pragma once

#include <string>
#include <utility>

#include <gocook/IGoCookApi.h>

/**
 * @brief 异步请求守卫值类型：把"请求发出后世界可能变了"的作废判定收敛到一处。
 *
 * 语义三分（各 ViewModel 统一按此归类，勿再手写各自变体）：
 *   1. 会话类（人变了：登出/换号）      → SessionSnapshot：发送时捕获，回调一行比对；
 *   2. 轮次类（轮次变了：换词/刷新/登出）→ RequestEpoch：发送时取票据，回调一行比对；
 *   3. 目标键类（看的还是这个对象吗）    → 不属于本文件：按请求对象 ID 比对即可
 *      （如详情族的 m_detailRequestedId），语义与轮次不同，留在使用点。
 *
 * 共性契约：守卫只做"该响应是否已失效"的判定，不管理加载态；过期响应一律
 * 静默丢弃（不落数据、不发信号、不弹提示）。是否顺带复位 loading 标记由
 * 使用点按各自契约决定（如库存 VM 在会话切换分支按代次判定后复位）。
 *
 * 使用范式（异步回调第一行）：
 * @code
 *   const auto session = SessionSnapshot::capture(m_api);
 *   const int ticket = m_epoch.begin();          // 或续页：m_epoch.current()
 *   m_api->getXxx(..., [self, session, ticket](...){
 *       if (!self) return;                        // QPointer 悬垂守卫（Qt 惯用，保留原样）
 *       if (!session.isCurrent(self->m_api)) { ... }   // 会话已切换 → 丢弃（按需复位 loading）
 *       if (!self->m_epoch.isCurrent(ticket)) return;  // 轮次已过期 → 静默丢弃
 *       ...
 *   });
 * @endcode
 */

/**
 * @brief 请求代次：单调递增的轮次票据（会话/轮次作废机制中"轮次"类）。
 *
 * 语义：每次开启新一轮（换词/刷新/清空）取一张票据随请求发出；期间任何
 * 新一轮开启或显式作废都会使旧票据失效——旧响应到达时 isCurrent 为假，
 * 静默丢弃。保证"最后一次输入必达、过期响应不闪回"。
 */
class RequestEpoch
{
public:
    /// 默认构造：代次从 0 起（禁用拷贝：代次计数器只应有一个所有者）
    RequestEpoch() = default;
    RequestEpoch(const RequestEpoch&) = delete;
    RequestEpoch& operator=(const RequestEpoch&) = delete;

    /// 开启新一轮：递增代次并返回本轮票据（发送请求前调用，随回调捕获）
    int begin() { return ++m_epoch; }

    /// 作废全部在途：递增后所有旧票据一律失效（登出/清空场景）
    void invalidate() { ++m_epoch; }

    /// 当前票据值（续页等"延续当前轮次"的请求取用，不递增）
    int current() const { return m_epoch; }

    /// 票据是否仍属当前轮次（false = 过期响应，应静默丢弃）
    bool isCurrent(int ticket) const { return ticket == m_epoch; }

private:
    int m_epoch = 0;   ///< 当前代次（只增不减）
};

/**
 * @brief 会话快照：发送时捕获 token，回调到达时一行比对（"会话"类作废机制）。
 *
 * 语义：响应到达时若当前 token 与发送时快照不一致（登出/换号），该响应
 * 属于旧账号——静默丢弃，防止旧账号数据串入当前界面（跨账号串台防线）。
 */
class SessionSnapshot
{
public:
    /// 捕获当前会话（发送请求前调用；游客的空 token 同样成立）
    static SessionSnapshot capture(const IGoCookApi* api)
    {
        return SessionSnapshot(api->authToken());
    }

    /// 会话是否仍是发送时那个（false = 旧账号响应，应静默丢弃）
    bool isCurrent(const IGoCookApi* api) const
    {
        return api->authToken() == m_token;
    }

private:
    /// 私有构造：只经 capture 创建（快照必须来自发送时刻，不允许事后拼装）
    explicit SessionSnapshot(std::string token) : m_token(std::move(token)) {}

    std::string m_token;   ///< 发送时刻的会话 token
};

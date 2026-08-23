// DbExecutor.h —— 全项目唯一一处"借连接 / 开事务 / 提交 / 异常分层"
//
// 规范：常规 Repository 方法必须用 executeDb 包裹，方法体只写"差异部分"
// （SQL 文本 + 参数 + 行→结构体映射），异常分层由这里统一保证。
//   · 借连接（RAII 自动归还）：getConnection 抛出的异常（含 ServiceException(503)）
//     发生在 try 之外，原样上抛、不翻译——"系统繁忙"语义不丢
//   · ServiceException 原样上抛（404/403/409/503 等业务状态码不丢）
//   · 其他 std::exception 统一翻译成 ServiceException(errorMsg)，日志带调用点方法名
//   · errorMsg 必传：每个方法给出业务可理解的失败文案（不确定时用"数据库操作失败"），
//     编译期强制，防止新方法忘了传而静默降级成通用文案
//   · lambda 返回 void（写操作）或有值（查询的映射结果）均可；禁止返回指针/引用、
//     以及 std::string_view / pqxx::field 这类"视图型"值
//     （事务与 result 在返回时销毁，会悬空——见下方 static_assert 编译期拦截）
//   · 特例（文件操作 + DB 混编、吞错策略等）保持手写，方法内必须注释原因
#pragma once
#include "ConnectionPool.h"
#include "Logger.h"
#include <gocook/IServices.h>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

// 分页总页数：size <= 0 时返回 0，杜绝 (total + size - 1) / size 的除零 UB。
// HTTP 层 parsePagination 已把 size 钳位到 >= 1，这里双保险——Repository 是公开接口，
// 不能假设调用方都经过路由器。
inline int safeTotalPages(int total, int size) noexcept {
    return size > 0 ? (total + size - 1) / size : 0;
}

template <typename Pool, typename Func>
auto executeDb(Pool& pool, Func&& func,
               const std::string& errorMsg,
               const std::source_location loc = std::source_location::current()) -> decltype(auto) {
    using FuncResult = std::invoke_result_t<Func, pqxx::work&>;
    static_assert(!std::is_reference_v<FuncResult>,
                  "executeDb: lambda 必须按值返回——引用在事务/result 销毁后悬空");
    static_assert(!std::is_pointer_v<std::remove_cv_t<FuncResult>>,
                  "executeDb: lambda 不得返回裸指针（例如 c_str()），请先拷贝成 std::string");
    static_assert(!std::is_same_v<std::remove_cvref_t<FuncResult>, std::string_view>,
                  "executeDb: lambda 不得返回 std::string_view——它只是 result 数据上的视图，"
                  "result 销毁后悬空，请拷贝成 std::string");
    static_assert(!std::is_same_v<std::remove_cvref_t<FuncResult>, pqxx::field>,
                  "executeDb: lambda 不得返回 pqxx::field——它只是 result 行内字段的视图，"
                  "result 销毁后悬空，请先取值（as<T>()）再返回");

    auto conn = pool.getConnection();                    // 借连接异常（含 503）原样上抛，不翻译
    try {
        pqxx::work txn(*conn);
        if constexpr (std::is_void_v<FuncResult>) {
            func(txn);                                   // 写操作：lambda 不返回值
            txn.commit();
        } else {
            auto result = func(txn);                     // 查询：lambda 返回映射结果
            txn.commit();
            return result;
        }
    } catch (const gocook::services::ServiceException&) {
        throw;                                           // 业务异常原样上抛
    } catch (const std::exception& e) {
        LOG_WARN("Database error in %s: %s", loc.function_name(), e.what());
        throw gocook::services::ServiceException(errorMsg);
    }
}

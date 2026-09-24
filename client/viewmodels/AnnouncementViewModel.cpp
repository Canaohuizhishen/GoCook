#include "AnnouncementViewModel.h"
#include <DataMapper.h>
#include <QPointer>
#include <algorithm>
#include "RequestGuards.h"

AnnouncementViewModel::AnnouncementViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList AnnouncementViewModel::announcements() const { return m_announcements; }
bool AnnouncementViewModel::isLoading() const { return m_isLoading; }
bool AnnouncementViewModel::hasMore() const { return m_hasMore; }
bool AnnouncementViewModel::isRefreshing() const { return m_isRefreshing; }
QString AnnouncementViewModel::previewTitle() const { return m_previewTitle; }
QString AnnouncementViewModel::previewCreatedAt() const { return m_previewCreatedAt; }

void AnnouncementViewModel::refresh()
{
    // 进入页面 / 下拉刷新：第一页会整表替换（loadAnnouncements page==1 清空重建）
    if (!m_isRefreshing) {
        m_isRefreshing = true;
        emit isRefreshingChanged();
    }
    loadAnnouncements(1, kPageSize);
}

void AnnouncementViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadAnnouncements(m_currentPage + 1, kPageSize);
}

void AnnouncementViewModel::loadPreview()
{
    // 消息页三横条预览：只取最新一条公告的标题与时间。
    // 红线：此路径绝不触发已读语义——不发 announcementsSeen、不上报水位、不动列表/分页/加载态；
    // 失败或空表静默保留旧值（下一次进入消息页会再拉）。
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getAnnouncements(1, 1,
        [self = QPointer<AnnouncementViewModel>(this), session]
        (bool success, const gocook::models::PagedAnnouncements& data, const std::string&) {
            if (!self) return;
            if (!session.isCurrent(self->m_api)) return;
            if (!success || data.data.empty()) return;
            const QString title = QString::fromStdString(data.data.front().title);
            const QString createdAt = QString::fromStdString(data.data.front().created_at);
            if (title == self->m_previewTitle && createdAt == self->m_previewCreatedAt) return;
            self->m_previewTitle = title;
            self->m_previewCreatedAt = createdAt;
            emit self->previewChanged();
        });
}

void AnnouncementViewModel::hideAnnouncement(int announcementId)
{
    // 页面「删除」的语义：会话级本地隐藏（服务端无删除接口，不产生网络请求）。
    // 同一会话内刷新/翻页不复活；clearAll（登出/换号）后隐藏集合清空、恢复显示。
    if (m_hiddenIds.contains(announcementId))
        return;
    m_hiddenIds.insert(announcementId);

    QVariantList kept;
    kept.reserve(m_announcements.size());
    for (const auto& v : m_announcements) {
        if (v.toMap()["id"].toInt() == announcementId)
            continue;
        kept.append(v);
    }
    m_announcements = kept;
    emit announcementsChanged();
}

void AnnouncementViewModel::loadAnnouncements(int page, int size)
{
    m_isLoading = true;
    emit isLoadingChanged();

    // 快照当前会话（token，SessionSnapshot）：登出/换号后到达的过期响应作废
    const auto session = SessionSnapshot::capture(m_api);
    m_api->getAnnouncements(page, size,
        [self = QPointer<AnnouncementViewModel>(this), session, page]
        (bool success, const gocook::models::PagedAnnouncements& data, const std::string& error) {
            if (!self) return;
            self->m_isLoading = false;
            emit self->isLoadingChanged();
            self->m_isRefreshing = false;
            emit self->isRefreshingChanged();
            if (!session.isCurrent(self->m_api)) return;   // 会话已切换：过期响应作废

            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                return;
            }

            if (page == 1)
                self->m_announcements.clear();
            for (const auto& item : data.data) {
                if (self->m_hiddenIds.contains(item.id))
                    continue;   // 会话级隐藏：刷新/翻页不复活
                self->m_announcements.append(DataMapper::toNotificationMap(item));
            }

            self->m_currentPage = data.pagination.page;
            self->m_totalPages = data.pagination.total_pages;
            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

            // 进入系统通知页 = 已读：先清本地红点（announcementsSeen），再上报水位
            if (page == 1) {
                emit self->announcementsSeen();

                int maxId = 0;
                for (const auto& item : data.data)
                    maxId = std::max(maxId, item.id);
                if (maxId > 0 && maxId > self->m_reportedWatermark) {
                    const auto reportSession = SessionSnapshot::capture(self->m_api);
                    self->m_api->setAnnouncementsReadState(maxId,
                        [self, reportSession, maxId](bool success, const std::string&) {
                            if (!self) return;
                            if (!reportSession.isCurrent(self->m_api)) return;
                            if (success && maxId > self->m_reportedWatermark)
                                self->m_reportedWatermark = maxId;   // 仅成功记账；失败下次进页重报
                        });
                }
            }

            emit self->announcementsChanged();
            emit self->hasMoreChanged();
        });
}

void AnnouncementViewModel::clearAll()
{
    // 登出统一清理（main.cpp 单点接线）：列表 / 分页 / 刷新态 / 已上报水位全部归零
    m_announcements.clear();
    m_currentPage = 1;
    m_totalPages = 0;
    m_hasMore = false;
    m_isRefreshing = false;
    m_reportedWatermark = 0;
    m_previewTitle.clear();
    m_previewCreatedAt.clear();
    m_hiddenIds.clear();
    emit announcementsChanged();
    emit hasMoreChanged();
    emit isRefreshingChanged();
    emit previewChanged();
}

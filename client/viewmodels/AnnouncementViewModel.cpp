#include "AnnouncementViewModel.h"
#include <DataMapper.h>
#include <QPointer>

AnnouncementViewModel::AnnouncementViewModel(IGoCookApi *api, QObject *parent)
    : QObject(parent), m_api(api) {}

QVariantList AnnouncementViewModel::announcements() const { return m_announcements; }
bool AnnouncementViewModel::isLoading() const { return m_isLoading; }
bool AnnouncementViewModel::hasMore() const { return m_hasMore; }

void AnnouncementViewModel::refresh()
{
    m_currentPage = 1;
    m_announcements.clear();
    emit announcementsChanged();
    m_hasMore = false;
    emit hasMoreChanged();
    loadAnnouncements(1);
}

void AnnouncementViewModel::loadNextPage()
{
    if (m_isLoading || !m_hasMore) return;
    loadAnnouncements(m_currentPage + 1);
}

void AnnouncementViewModel::loadAnnouncements(int page)
{
    m_isLoading = true;
    emit isLoadingChanged();

    m_api->getAnnouncements(page, m_pageSize,
        [self = QPointer<AnnouncementViewModel>(this), page](bool success,
                              const gocook::models::PagedAnnouncements& data,
                              const std::string& error) {
            if (!self) return;
            if (!success) {
                emit self->errorOccurred(QString::fromStdString(error));
                self->m_isLoading = false;
                emit self->isLoadingChanged();
                return;
            }

            if (page == 1)
                self->m_announcements.clear();

            for (const auto& item : data.data)
                self->m_announcements.append(DataMapper::toMap(item));

            self->m_currentPage = data.pagination.page;
            self->m_totalPages = data.pagination.total_pages;
            self->m_hasMore = (self->m_currentPage < self->m_totalPages);

            emit self->announcementsChanged();
            emit self->hasMoreChanged();
            self->m_isLoading = false;
            emit self->isLoadingChanged();
        });
}

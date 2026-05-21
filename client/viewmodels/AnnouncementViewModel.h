#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <gocook/IGoCookApi.h>

class AnnouncementViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList announcements READ announcements NOTIFY announcementsChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)

public:
    explicit AnnouncementViewModel(IGoCookApi *api, QObject *parent = nullptr);

    QVariantList announcements() const;
    bool isLoading() const;
    bool hasMore() const;

    Q_INVOKABLE void loadAnnouncements(int page = 1);
    Q_INVOKABLE void loadNextPage();
    Q_INVOKABLE void refresh();

signals:
    void announcementsChanged();
    void isLoadingChanged();
    void hasMoreChanged();
    void errorOccurred(const QString& error);

private:
    IGoCookApi *m_api;
    QVariantList m_announcements;
    bool m_isLoading = false;
    bool m_hasMore = false;
    int m_currentPage = 1;
    int m_pageSize = 20;
    int m_totalPages = 0;
};

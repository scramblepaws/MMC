#pragma once

#include "core/Model.h"

#include <QHash>
#include <QWidget>

class QListWidget;
class QTabBar;
class QToolButton;

namespace mmc {

// The middle column: the mail of the accounts, newest first.
class MessageList : public QWidget
{
    Q_OBJECT
public:
    enum class Filter { All, Unread, Starred, Attachments };

    explicit MessageList(QWidget* parent = nullptr);

    void setMessages(const QVector<Message>& messages, const QHash<QString, Mailbox>& mailboxes);
    void setFilter(Filter filter);
    Filter filter() const { return m_filter; }
    void setAccountFilter(const QString& mailboxId);
    QString selectedId() const;
    void selectId(const QString& id);
    void setSearchText(const QString& text);
    void setBusy(bool busy);

signals:
    void messageActivated(const QString& messageId);
    void refreshRequested();
    void markReadRequested(bool read);
    void deleteRequested();
    void starToggled(const QString& messageId, bool starred);
    void filterChanged(MessageList::Filter filter);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool matches(const Message& message) const;

    QListWidget* m_list = nullptr;
    QTabBar* m_tabs = nullptr;
    QToolButton* m_refreshButton = nullptr;
    QToolButton* m_readButton = nullptr;
    QToolButton* m_deleteButton = nullptr;
    QVector<Message> m_messages;
    QHash<QString, Mailbox> m_mailboxes;
    QHash<QString, bool> m_starred;
    Filter m_filter = Filter::All;
    QString m_accountFilter;
    QString m_search;
};

} // namespace mmc

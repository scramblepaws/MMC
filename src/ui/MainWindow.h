#pragma once

#include "core/Model.h"
#include "net/Mime.h"
#include "net/OAuth2.h"
#include "ui/MessageList.h"
#include "ui/Sidebar.h"
#include "ui/Toast.h"

#include <QHash>
#include <QMainWindow>

class QLabel;
class QLineEdit;
class QStackedWidget;

namespace mmc {

class Vault;
class MailService;
class Sidebar;
class MessageList;
class Reader;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(Vault* vault, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool event(QEvent* event) override;

private slots:
    void onAddAccount();
    void onEditAccount();
    void onRemoveAccount();
    void onSignInAgain();
    void onAccountMenu(const QString& mailboxId, const QPoint& position);
    void onViewChanged(Sidebar::View view);
    void onAccountSelected(const QString& mailboxId);
    void onImportAccounts();
    void onRefreshAll();
    void onRefreshAccount();
    void onSyncFinished(const QString& mailboxId, bool ok, const QString& error);
    void onMessageActivated(const QString& messageId);
    void onBodyReady(const QString& messageId, const QByteArray& raw);
    void onBodyFailed(const QString& messageId, const QString& error);
    void onCompose();
    void onReply();
    void onSendFinished(bool ok, const QString& error);
    void onSearchChanged(const QString& text);
    void onOpenWebmail();
    void onSettings();
    void onAccountManager();
    void onFilterChanged(MessageList::Filter filter);
    void onMarkRead();
    void onDeleteMessage();
    void onStarToggled(const QString& messageId, bool starred);

private:
    void buildUi();
    void rebuildAccounts();
    void rebuildMessages();
    void showMessage(const Message& message);
    void storeTokens(const QString& mailboxId, const OAuth2Tokens& tokens);
    void applyMailbox(const Mailbox& mailbox, bool isNew);
    QString selectedMailboxId() const;
    Message messageById(const QString& id) const;
    void log(const QString& text);
    void toast(const QString& title, const QString& text, Toast::Kind kind);
    void showCooldownStatus();
    QVector<Message> messagesForView() const;
    void fetchFolderForView();

    Vault* m_vault = nullptr;
    MailService* m_service = nullptr;

    Sidebar* m_sidebar = nullptr;
    MessageList* m_messages = nullptr;
    Reader* m_reader = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_cooldown = nullptr;
    QLabel* m_status = nullptr;
    QStackedWidget* m_toasts = nullptr;

    QHash<QString, QByteArray> m_bodyCache;
    QHash<QString, bool> m_starred;
    QString m_currentMessageId;
    Sidebar::View m_view = Sidebar::View::AllAccounts;
    QString m_accountFilter;
    QString m_searchText;
};

} // namespace mmc

#pragma once

#include "core/Model.h"
#include "net/Mime.h"

#include <QHash>
#include <QMainWindow>

class QListWidget;
class QTableWidget;
class QTextBrowser;
class QLineEdit;
class QLabel;
class QListWidgetItem;

namespace mmc {

class Vault;
class MailService;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(Vault* vault, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAddAccount();
    void onEditAccount();
    void onRemoveAccount();
    void onImportAccounts();
    void onRefreshAll();
    void onRefreshAccount();
    void onSyncFinished(const QString& mailboxId, bool ok, const QString& error);
    void onMessageSelectionChanged();
    void onBodyReady(const QString& messageId, const QByteArray& raw);
    void onCompose();
    void onSendFinished(bool ok, const QString& error);
    void onSearch();
    void onOpenWebmail();
    void onSettings();
    void onSaveAttachment();
    void onAccountSelectionChanged();

private:
    void buildUi();
    void rebuildAccountList();
    void rebuildMessageList();
    void showMessageBody(const ParsedMessage& parsed, const QByteArray& raw);
    QString selectedMailboxId() const;
    Message messageById(const QString& id) const;
    void log(const QString& text);
    void showCooldownStatus();

    Vault* m_vault = nullptr;
    MailService* m_service = nullptr;

    QListWidget* m_accounts = nullptr;
    QTableWidget* m_messages = nullptr;
    QTextBrowser* m_reader = nullptr;
    QListWidget* m_attachments = nullptr;
    QLineEdit* m_search = nullptr;
    QLabel* m_cooldown = nullptr;
    QLabel* m_status = nullptr;

    QHash<QString, QByteArray> m_bodyCache;
    QString m_currentMessageId;
};

} // namespace mmc

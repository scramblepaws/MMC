#pragma once

#include "core/Vault.h"
#include "net/ImapClient.h"
#include "net/OAuth2.h"

#include <QObject>
#include <QVector>

namespace mmc {

struct SyncOutcome {
    QString mailboxId;
    bool ok = false;
    QString error;
    QVector<ImapHeader> headers;
    QVector<ImapFolderInfo> folders;
    int unseen = 0;
    qint64 uidNext = 0;
    QString folderName = QStringLiteral("INBOX");
    QString folderKind = QStringLiteral("inbox");
    OAuth2Tokens refreshedTokens;   // valid when the access token had to be renewed
    bool haveRefreshedTokens = false;
};

// Talks to the mail servers. Everything runs on a worker thread, the results
// come back through signals.
class MailService : public QObject
{
    Q_OBJECT
public:
    explicit MailService(Vault* vault, QObject* parent = nullptr);

    void syncMailbox(const QString& mailboxId);
    void syncFolder(const QString& mailboxId, const QString& folderName, const QString& folderKind);
    void syncAll();
    void fetchBody(const QString& messageId);
    void sendMessage(const QString& mailboxId, const QString& to, const QString& cc,
                     const QString& subject, const QString& bodyText, const QString& bodyHtml,
                     const QVector<QByteArray>& attachmentData,
                     const QStringList& attachmentNames);

signals:
    void syncStarted(const QString& mailboxId);
    void syncFinished(const QString& mailboxId, bool ok, const QString& error);
    void bodyReady(const QString& messageId, const QByteArray& raw);
    void bodyFailed(const QString& messageId, const QString& error);
    void sendFinished(bool ok, const QString& error);
    void logMessage(const QString& text);

private:
    void syncFolderInternal(const QString& mailboxId, const QString& folderName,
                            const QString& folderKind);
    Vault* m_vault = nullptr;
};

} // namespace mmc

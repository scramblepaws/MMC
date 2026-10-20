#include "net/MailService.h"

#include "net/Mime.h"
#include "net/OAuth2.h"
#include "net/Providers.h"
#include "net/SmtpClient.h"

#include <QFutureWatcher>
#include <QRunnable>
#include <QtConcurrent/QtConcurrentRun>

namespace mmc {
namespace {

struct BodyRequest {
    QString mailboxId;
    QString folder;
    QString uid;
};

struct SendRequest {
    QString mailboxId;
    QString to;
    QString cc;
    QString subject;
    QString bodyText;
    QString bodyHtml;
    QVector<QByteArray> attachmentData;
    QStringList attachmentNames;
};

QString folderKey(const QString& mailboxId, const QString& folder)
{
    return mailboxId + QChar('|') + folder;
}

// Refreshes the access token when it is about to expire.
bool accessTokenFor(const OAuthToken* token, const Mailbox& mailbox, const Provider& provider,
                    const QMap<QString, QString>& settings, QString* accessToken,
                    OAuth2Tokens* fresh, QString* error)
{
    const QString clientId = settings.value(providerClientIdKey(provider.id));
    const QString clientSecret = settings.value(providerClientSecretKey(provider.id));

    if (!token || token->accessToken.isEmpty()) {
        *accessToken = QString();
        return true;   // fall back to the app password
    }
    const bool expired = token->expiry.isValid() && token->expiry < QDateTime::currentDateTimeUtc().addSecs(60);
    if (!expired) {
        *accessToken = token->accessToken;
        return true;
    }
    if (token->refreshToken.isEmpty()) {
        if (error) *error = QObject::tr("The access token expired and there is no refresh token. "
                                         "Sign in to this account again.");
        return false;
    }
    if (!OAuth2::refresh(provider, clientId, clientSecret, token->refreshToken, fresh, error))
        return false;
    *accessToken = fresh->accessToken;
    return true;
}

SyncOutcome runSync(const VaultState& state, const QMap<QString, QString>& settings,
                    const QString& mailboxId)
{
    SyncOutcome outcome;
    outcome.mailboxId = mailboxId;

    const Mailbox* mailbox = state.mailboxById(mailboxId);
    if (!mailbox) {
        outcome.error = QObject::tr("The account is not in the vault any more.");
        return outcome;
    }
    const Provider* provider = providerById(mailbox->providerId);
    if (!provider) {
        outcome.error = QObject::tr("Unknown provider %1.").arg(mailbox->providerId);
        return outcome;
    }

    QString accessToken;
    QString error;
    if (!accessTokenFor(state.tokenFor(mailboxId), *mailbox, *provider,
                        settings, &accessToken, &outcome.refreshedTokens, &error)) {
        outcome.error = error;
        return outcome;
    }
    outcome.haveRefreshedTokens = !outcome.refreshedTokens.accessToken.isEmpty();

    ImapClient client;
    if (!client.connect(provider->imapHost, provider->imapPort, provider->imapImplicitTls,
                        mailbox->address, accessToken,
                        QString::fromUtf8(mailbox->secret), provider->userAgent, &error)) {
        outcome.error = error;
        return outcome;
    }

    if (!client.listFolders(&outcome.folders, &error)) {
        outcome.error = error;
        return outcome;
    }

    ImapFolderStatus status;
    if (!client.status(QStringLiteral("INBOX"), &status, &error)) {
        outcome.error = error;
        return outcome;
    }
    outcome.unseen = status.unseen;
    outcome.uidNext = status.uidNext;

    qint64 since = 0;
    for (const Folder& f : state.folders)
        if (f.mailboxId == mailboxId && f.name == QLatin1String("INBOX"))
            since = f.uidNext;

    if (!client.fetchHeaders(QStringLiteral("INBOX"), since, &outcome.headers, &error)) {
        outcome.error = error;
        return outcome;
    }

    client.logout();
    outcome.ok = true;
    return outcome;
}

void runFetchBody(const VaultState& state, const QMap<QString, QString>& settings,
                  const BodyRequest& request, QByteArray* raw, QString* error)
{
    const Mailbox* mailbox = state.mailboxById(request.mailboxId);
    if (!mailbox) {
        if (error) *error = QObject::tr("The account is not in the vault any more.");
        return;
    }
    const Provider* provider = providerById(mailbox->providerId);
    if (!provider) {
        if (error) *error = QObject::tr("Unknown provider.");
        return;
    }
    QString accessToken;
    OAuth2Tokens fresh;
    if (!accessTokenFor(state.tokenFor(request.mailboxId), *mailbox, *provider,
                        settings, &accessToken, &fresh, error))
        return;

    ImapClient client;
    if (!client.connect(provider->imapHost, provider->imapPort, provider->imapImplicitTls,
                        mailbox->address, accessToken, QString::fromUtf8(mailbox->secret),
                        provider->userAgent, error))
        return;
    client.fetchMessage(request.folder, request.uid, raw, error);
    client.logout();
}

void runSend(const VaultState& state, const QMap<QString, QString>& settings,
             const SendRequest& request, QString* error)
{
    const Mailbox* mailbox = state.mailboxById(request.mailboxId);
    if (!mailbox) {
        if (error) *error = QObject::tr("The account is not in the vault any more.");
        return;
    }
    const Provider* provider = providerById(mailbox->providerId);
    if (!provider) {
        if (error) *error = QObject::tr("Unknown provider.");
        return;
    }
    QString accessToken;
    OAuth2Tokens fresh;
    if (!accessTokenFor(state.tokenFor(request.mailboxId), *mailbox, *provider,
                        settings, &accessToken, &fresh, error))
        return;

    QVector<OutgoingAttachment> attachments;
    for (int i = 0; i < request.attachmentData.size(); ++i) {
        OutgoingAttachment att;
        att.filename = request.attachmentNames.value(i, QStringLiteral("attachment"));
        att.data = request.attachmentData.at(i);
        attachments << att;
    }

    const QByteArray raw = buildMimeMessage(mailbox->address,
                                             QStringList{request.to},
                                             request.cc.isEmpty() ? QStringList() : QStringList{request.cc},
                                             request.subject, request.bodyText,
                                             request.bodyHtml, attachments);

    SmtpClient client;
    if (!client.connect(provider->smtpHost, provider->smtpPort, provider->smtpImplicitTls,
                        mailbox->address, accessToken, QString::fromUtf8(mailbox->secret),
                        provider->userAgent, error))
        return;
    if (!client.send(mailbox->address, QStringList{request.to}, raw, error))
        return;
    client.disconnect();
}

} // namespace

MailService::MailService(Vault* vault, QObject* parent)
    : QObject(parent), m_vault(vault)
{
}

void MailService::syncMailbox(const QString& mailboxId)
{
    if (!m_vault)
        return;
    emit syncStarted(mailboxId);

    const VaultState state = m_vault->state();
    const QMap<QString, QString> settings = m_vault->state().settings;

    auto* watcher = new QFutureWatcher<SyncOutcome>(this);
    connect(watcher, &QFutureWatcher<SyncOutcome>::finished, this, [this, watcher, mailboxId]() {
        const SyncOutcome outcome = watcher->result();
        watcher->deleteLater();

        if (!m_vault)
            return;

        if (outcome.haveRefreshedTokens) {
            OAuthToken* token = m_vault->state().tokenFor(mailboxId);
            if (token) {
                token->accessToken = outcome.refreshedTokens.accessToken;
                token->expiry = outcome.refreshedTokens.expiry;
                if (!outcome.refreshedTokens.refreshToken.isEmpty())
                    token->refreshToken = outcome.refreshedTokens.refreshToken;
            }
        }

        if (outcome.ok) {
            VaultState& state = m_vault->state();

            // Remember the folders of this account.
            for (const ImapFolderInfo& info : outcome.folders) {
                const QString id = mailboxId + QChar('|') + info.name;
                Folder* folder = state.folderById(id);
                if (!folder) {
                    Folder fresh;
                    fresh.id = id;
                    fresh.mailboxId = mailboxId;
                    fresh.name = info.name;
                    fresh.delimiter = info.delimiter;
                    state.folders.append(fresh);
                    folder = state.folderById(id);
                }
                if (folder && info.name == QLatin1String("INBOX"))
                    folder->uidNext = outcome.uidNext;
            }

            // Merge the new headers, newest first.
            const QString folderId = mailboxId + QChar('|') + QStringLiteral("INBOX");
            for (const ImapHeader& header : outcome.headers) {
                Message message;
                message.id = mailboxId + QChar('|') + QStringLiteral("INBOX") + QChar('|') + header.uid;
                message.mailboxId = mailboxId;
                message.folderId = folderId;
                message.uid = header.uid;
                message.messageId = header.messageId;
                message.from = header.from;
                message.to = header.to;
                message.subject = header.subject;
                message.date = QDateTime::fromString(header.date, Qt::RFC2822Date);
                message.size = header.size;
                message.flags = header.flags;

                int existing = -1;
                for (int i = 0; i < state.messages.size(); ++i) {
                    if (state.messages.at(i).id == message.id) {
                        existing = i;
                        break;
                    }
                }
                if (existing >= 0)
                    state.messages[existing] = message;
                else
                    state.messages.append(message);
            }

            // Keep the vault small: the newest 500 headers per account.
            int kept = 0;
            for (int i = state.messages.size() - 1; i >= 0; --i) {
                if (state.messages.at(i).mailboxId != mailboxId)
                    continue;
                if (++kept > 500)
                    state.messages.remove(i);
            }

            state.settings[QStringLiteral("unseen.%1").arg(mailboxId)] = QString::number(outcome.unseen);
        }

        QString error;
        m_vault->save(&error);
        emit syncFinished(mailboxId, outcome.ok, outcome.error);
    });

    watcher->setFuture(QtConcurrent::run([state, settings, mailboxId]() {
        return runSync(state, settings, mailboxId);
    }));
}

void MailService::syncAll()
{
    if (!m_vault)
        return;
    const QStringList ids = [&] {
        QStringList out;
        for (const Mailbox& m : m_vault->state().mailboxes)
            out << m.id;
        return out;
    }();
    for (const QString& id : ids)
        syncMailbox(id);
}

void MailService::fetchBody(const QString& messageId)
{
    if (!m_vault)
        return;
    const QStringList parts = messageId.split(QChar('|'));
    if (parts.size() < 3)
        return;

    BodyRequest request;
    request.mailboxId = parts.at(0);
    request.folder = parts.at(1);
    request.uid = parts.at(2);

    const VaultState state = m_vault->state();
    const QMap<QString, QString> settings = m_vault->state().settings;

    auto* watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, messageId]() {
        const QByteArray raw = watcher->result();
        watcher->deleteLater();
        if (raw.isEmpty()) {
            emit bodyFailed(messageId, tr("The message could not be downloaded."));
            return;
        }
        emit bodyReady(messageId, raw);
    });

    watcher->setFuture(QtConcurrent::run([state, settings, request]() {
        QByteArray raw;
        QString error;
        runFetchBody(state, settings, request, &raw, &error);
        return raw;
    }));
}

void MailService::sendMessage(const QString& mailboxId, const QString& to, const QString& cc,
                              const QString& subject, const QString& bodyText, const QString& bodyHtml,
                              const QVector<QByteArray>& attachmentData,
                              const QStringList& attachmentNames)
{
    if (!m_vault)
        return;
    SendRequest request;
    request.mailboxId = mailboxId;
    request.to = to;
    request.cc = cc;
    request.subject = subject;
    request.bodyText = bodyText;
    request.bodyHtml = bodyHtml;
    request.attachmentData = attachmentData;
    request.attachmentNames = attachmentNames;

    const VaultState state = m_vault->state();
    const QMap<QString, QString> settings = m_vault->state().settings;

    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        const QString error = watcher->result();
        watcher->deleteLater();
        emit sendFinished(error.isEmpty(), error);
    });

    watcher->setFuture(QtConcurrent::run([state, settings, request]() {
        QString error;
        runSend(state, settings, request, &error);
        return error;
    }));
}

} // namespace mmc

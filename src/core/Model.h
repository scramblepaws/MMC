#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mmc {

// A mailbox is one email account that MMC can read and send from.
struct Mailbox {
    QString id;
    QString providerId;                  // "google" | "microsoft" | "yandex"
    QString address;                     // user@host
    QString displayName;                 // shown in the account list
    QString label;                       // short tag shown on message badges
    QByteArray secret;                   // app password (optional, OAuth accounts do not need one)
    QDateTime cooldownUntil;             // "do not change the account behind this mailbox until"
    int cooldownDays = 30;               // default length of a new cooldown
    QString cooldownNote;
};

// OAuth2 tokens for one mailbox. Stored inside the encrypted vault.
struct OAuthToken {
    QString mailboxId;
    QString accessToken;
    QString refreshToken;
    QDateTime expiry;
    QString scope;
};

// An IMAP folder (INBOX, Sent, ...) discovered on the server.
struct Folder {
    QString id;                          // "<mailboxId>/<server name>"
    QString mailboxId;
    QString name;                        // server side name
    QString delimiter = "/";
    qint64 uidValidity = 0;
    qint64 uidNext = 0;
};

// Message headers only. Bodies are fetched on demand and kept in memory.
struct Message {
    QString id;                          // "<mailboxId>/<folderId>/<uid>"
    QString mailboxId;
    QString folderId;
    QString uid;
    QString messageId;                   // RFC 5322 Message-ID
    QString from;
    QString to;
    QString subject;
    QDateTime date;
    int size = 0;
    QString flags;                       // IMAP flags, e.g. "\\Seen \\Flagged"
    bool hasAttachment = false;
};

// Everything a vault holds. Serialised as one encrypted blob.
struct VaultState {
    QVector<Mailbox> mailboxes;
    QVector<OAuthToken> tokens;
    QVector<Folder> folders;
    QVector<Message> messages;
    QMap<QString, QString> settings;

    Mailbox* mailboxById(const QString& id);
    const Mailbox* mailboxById(const QString& id) const;
    OAuthToken* tokenFor(const QString& mailboxId);
    const OAuthToken* tokenFor(const QString& mailboxId) const;
    Folder* folderById(const QString& id);
    const Folder* folderById(const QString& id) const;

    void removeMailbox(const QString& id);
};

QDataStream& operator<<(QDataStream& out, const Mailbox& m);
QDataStream& operator>>(QDataStream& in, Mailbox& m);
QDataStream& operator<<(QDataStream& out, const OAuthToken& t);
QDataStream& operator>>(QDataStream& in, OAuthToken& t);
QDataStream& operator<<(QDataStream& out, const Folder& f);
QDataStream& operator>>(QDataStream& in, Folder& f);
QDataStream& operator<<(QDataStream& out, const Message& m);
QDataStream& operator>>(QDataStream& in, Message& m);
QDataStream& operator<<(QDataStream& out, const VaultState& s);
QDataStream& operator>>(QDataStream& in, VaultState& s);

} // namespace mmc

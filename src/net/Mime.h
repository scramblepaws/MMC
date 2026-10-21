#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mmc {

struct MimePart {
    QString contentType;      // "text/plain", "text/html", "application/pdf", ...
    QString charset;
    QByteArray data;          // transfer-decoded content
    QString filename;
    bool isAttachment = false;
};

struct ParsedMessage {
    QMap<QString, QString> headers;   // keys lowercased
    QVector<MimePart> parts;

    QString header(const QString& name) const;
    QString bodyText() const;
    QString bodyHtml() const;
    QVector<MimePart> attachments() const;
    // "From", "Subject", ... as shown in the UI.
    QString displayFrom() const;
};

// Parses a raw RFC 5322 message. Never throws, returns what it can.
ParsedMessage parseMime(const QByteArray& raw);

// Decodes RFC 2047 encoded words in a header value ("=?UTF-8?B?...?=").
QString decodeHeaderValue(const QString& value);

// "Someone <someone@gmail.com>" becomes "Someone".
QString senderName(const QString& from);

// Makes the html of a mail safe to show: no scripts, no event handlers, and
// no pictures from the internet unless that is wanted.
QString sanitizeMailHtml(const QString& html, bool allowRemoteImages);

// The first words of a mail, for the line under the subject.
QString snippetFromBody(const QByteArray& body);

struct OutgoingAttachment {
    QString filename;
    QByteArray data;
    QString contentType = QStringLiteral("application/octet-stream");
};

// Builds a multipart/mixed message with a plain text and an HTML alternative.
QByteArray buildMimeMessage(const QString& from,
                             const QStringList& to,
                             const QStringList& cc,
                             const QString& subject,
                             const QString& bodyText,
                             const QString& bodyHtml,
                             const QVector<OutgoingAttachment>& attachments);

} // namespace mmc

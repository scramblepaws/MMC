#include "core/Importer.h"

#include "net/Providers.h"

#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>

namespace mmc {
namespace {

QChar detectDelimiter(const QStringList& lines)
{
    const QVector<QChar> candidates = {QChar(','), QChar(';'), QChar('\t')};
    int best = 0;
    QChar bestChar = QChar(',');
    for (QChar c : candidates) {
        int count = 0;
        const int limit = qMin(lines.size(), 20);
        for (int i = 0; i < limit; ++i)
            count += lines.at(i).count(c);
        if (count > best) {
            best = count;
            bestChar = c;
        }
    }
    return bestChar;
}

QStringList splitLine(const QString& line, QChar delimiter)
{
    QStringList fields;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == QChar('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QChar('"')) {
                    current += QChar('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current += ch;
            }
        } else if (ch == QChar('"')) {
            inQuotes = true;
        } else if (ch == delimiter) {
            fields << current.trimmed();
            current.clear();
        } else {
            current += ch;
        }
    }
    fields << current.trimmed();
    return fields;
}

bool isEmail(const QString& s)
{
    static const QRegularExpression re(
        QStringLiteral("^[^@\\s]+@[^@\\s.]+(\\.[^@\\s.]+)+$"));
    return re.match(s).hasMatch();
}

} // namespace

ImportResult importMailboxesFromText(const QByteArray& text)
{
    ImportResult result;
    QString content = QString::fromUtf8(text);
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QChar('\r'), QChar('\n'));

    QStringList lines = content.split(QChar('\n'));
    // Drop a header line if it looks like one.
    if (!lines.isEmpty()) {
        const QString first = lines.first().trimmed().toLower();
        if (first.startsWith(QStringLiteral("address")) || first.startsWith(QStringLiteral("email")))
            lines.removeFirst();
    }
    lines.removeAll(QString());

    if (lines.isEmpty()) {
        result.errors << QObject::tr("The file is empty.");
        return result;
    }

    const QChar delimiter = detectDelimiter(lines);

    int lineNumber = 0;
    for (const QString& line : lines) {
        ++lineNumber;
        if (line.trimmed().startsWith(QChar('#')))
            continue;

        const QStringList fields = splitLine(line, delimiter);
        if (fields.isEmpty()) {
            ++result.skipped;
            continue;
        }

        QString address;
        QString provider;
        QString displayName;
        QString secret;

        for (int i = 0; i < fields.size(); ++i) {
            const QString value = fields.at(i).trimmed();
            if (value.isEmpty())
                continue;
            if (address.isEmpty() && isEmail(value)) {
                address = value.toLower();
                continue;
            }
            if (provider.isEmpty() && providerIdForAddress(value) != QString()) {
                provider = providerIdForAddress(value);
                continue;
            }
            if (displayName.isEmpty()) {
                displayName = value;
                continue;
            }
            secret = value;   // last leftover field is treated as the app password
        }

        if (address.isEmpty()) {
            result.errors << QObject::tr("Line %1: no email address found.").arg(lineNumber);
            ++result.skipped;
            continue;
        }
        if (provider.isEmpty())
            provider = providerIdForAddress(address);
        if (provider.isEmpty()) {
            result.errors << QObject::tr("Line %1: cannot tell which provider %2 belongs to.").arg(lineNumber).arg(address);
            ++result.skipped;
            continue;
        }

        Mailbox m;
        m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m.providerId = provider;
        m.address = address;
        m.displayName = displayName.isEmpty() ? address : displayName;
        m.label = m.displayName.left(2).toUpper();
        m.secret = secret.toUtf8();
        result.mailboxes << m;
    }

    if (result.mailboxes.isEmpty() && result.errors.isEmpty())
        result.errors << QObject::tr("No mailboxes found in the file.");
    return result;
}

} // namespace mmc

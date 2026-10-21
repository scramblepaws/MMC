#include "net/ImapClient.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QLatin1String>
#include <QSslSocket>
#include <QRegularExpression>

#include "net/Mime.h"

namespace mmc {
namespace {

constexpr int kConnectTimeoutMs = 20000;
constexpr int kCommandTimeoutMs = 60000;

// One node of a parsed IMAP item list: either a string or a nested list.
struct Node {
    bool isString = true;
    QByteArray text;
    QVector<Node> children;
};

// Tokenises the inside of a parenthesised IMAP item list.
QVector<Node> parseItems(const QByteArray& text, int* pos)
{
    QVector<Node> items;
    while (*pos < text.size()) {
        const char ch = text.at(*pos);
        if (ch == ' ') {
            ++*pos;
            continue;
        }
        if (ch == ')')
            return items;
        if (ch == '(') {
            ++*pos;
            Node list;
            list.isString = false;
            list.children = parseItems(text, pos);
            if (*pos < text.size() && text.at(*pos) == ')')
                ++*pos;
            items << list;
            continue;
        }
        if (ch == '"') {
            ++*pos;
            QByteArray value;
            while (*pos < text.size() && text.at(*pos) != '"') {
                if (text.at(*pos) == '\\' && *pos + 1 < text.size()) {
                    ++*pos;
                    char esc = text.at(*pos);
                    value += (esc == 'n') ? '\n' : (esc == 'r') ? '\r' : esc;
                } else {
                    value += text.at(*pos);
                }
                ++*pos;
            }
            if (*pos < text.size())
                ++*pos;   // closing quote
            Node node;
            node.text = value;
            items << node;
            continue;
        }
        if (ch == '[') {
            // BODY[...]: keep it as one atom, brackets included.
            QByteArray value;
            while (*pos < text.size() && text.at(*pos) != ' ' && text.at(*pos) != ')') {
                value += text.at(*pos);
                ++*pos;
            }
            Node node;
            node.text = value;
            items << node;
            continue;
        }
        QByteArray value;
        while (*pos < text.size() && text.at(*pos) != ' ' && text.at(*pos) != ')'
               && text.at(*pos) != '(') {
            value += text.at(*pos);
            ++*pos;
        }
        if (value.isEmpty()) {
            ++*pos;
            continue;
        }
        Node node;
        node.text = value;
        items << node;
    }
    return items;
}

QVector<Node> parseItemList(const QByteArray& text)
{
    int pos = 0;
    return parseItems(text, &pos);
}

QString nodeValue(const QVector<Node>& items, const char* key)
{
    for (int i = 0; i + 1 < items.size(); ++i) {
        if (items.at(i).isString && items.at(i).text == key) {
            const Node& v = items.at(i + 1);
            return v.isString ? QString::fromLatin1(v.text) : QString();
        }
    }
    return {};
}

QStringList nodeFlags(const QVector<Node>& items, const char* key)
{
    QStringList out;
    for (int i = 0; i + 1 < items.size(); ++i) {
        if (items.at(i).isString && items.at(i).text == key && !items.at(i + 1).isString) {
            for (const Node& child : items.at(i + 1).children)
                if (child.isString)
                    out << QString::fromLatin1(child.text);
            return out;
        }
    }
    return out;
}

} // namespace

ImapClient::ImapClient() = default;

ImapClient::~ImapClient()
{
    logout();
    delete m_socket;
}

bool ImapClient::sendLine(const QByteArray& line, QString* error)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        if (error) *error = QObject::tr("Not connected to the server.");
        return false;
    }
    m_socket->write(line + "\r\n");
    if (!m_socket->waitForBytesWritten(kCommandTimeoutMs)) {
        if (error) *error = QObject::tr("Timed out while sending to the server.");
        return false;
    }
    return true;
}

bool ImapClient::readRawLine(QByteArray* line, QString* error)
{
    for (;;) {
        const int idx = m_buffer.indexOf('\n');
        if (idx >= 0) {
            *line = m_buffer.left(idx);
            m_buffer.remove(0, idx + 1);
            if (line->endsWith('\r'))
                line->chop(1);
            return true;
        }
        if (!m_socket->waitForReadyRead(kCommandTimeoutMs)) {
            if (error) *error = QObject::tr("Timed out while waiting for the server.");
            return false;
        }
        m_buffer += m_socket->readAll();
    }
}

bool ImapClient::readExact(int count, QByteArray* data, QString* error)
{
    while (m_buffer.size() < count) {
        if (!m_socket->waitForReadyRead(kCommandTimeoutMs)) {
            if (error) *error = QObject::tr("Timed out while reading a message from the server.");
            return false;
        }
        m_buffer += m_socket->readAll();
    }
    *data = m_buffer.left(count);
    m_buffer.remove(0, count);
    return true;
}

bool ImapClient::readResponse(Response* response, QString* error)
{
    if (!readRawLine(&response->line, error))
        return false;

    // A line may end with a literal marker {123}; the literal data follows
    // and the rest of the line comes after it.
    for (;;) {
        static const QRegularExpression literalRe(QStringLiteral("\\{(\\d+)\\}$"));
        const QRegularExpressionMatch m = literalRe.match(QString::fromLatin1(response->line));
        if (!m.hasMatch())
            break;
        const int size = m.captured(1).toInt();
        QByteArray literal;
        if (!readExact(size, &literal, error))
            return false;
        response->literals << literal;

        QByteArray rest;
        if (!readRawLine(&rest, error))
            return false;
        response->line += ' ';
        response->line += rest;
    }
    return true;
}

bool ImapClient::command(const QByteArray& cmd, QVector<Response>* untagged,
                         QByteArray* tagged, QString* error)
{
    const QByteArray tag = QByteArrayLiteral("mmc") + QByteArray::number(++m_tagCounter);
    if (!sendLine(tag + QByteArrayLiteral(" ") + cmd, error))
        return false;

    for (;;) {
        Response response;
        if (!readResponse(&response, error))
            return false;
        if (response.line.startsWith(tag + QByteArrayLiteral(" "))) {
            const QByteArray status = response.line.mid(tag.size() + 1);
            if (tagged)
                *tagged = status;
            if (!status.startsWith("OK")) {
                if (error) {
                    *error = QString::fromUtf8(status).trimmed();
                    if (error->isEmpty())
                        *error = QObject::tr("The server rejected the command.");
                }
                return false;
            }
            return true;
        }
        if (response.line.startsWith("* ")) {
            if (untagged)
                untagged->append(response);
            continue;
        }
        if (response.line.startsWith("+ "))
            continue;   // continuation request, handled by the caller
    }
}

bool ImapClient::connect(const QString& host, int port, bool implicitTls,
                         const QString& user, const QString& authToken,
                         const QString& secret, const QString& userAgent, QString* error)
{
    delete m_socket;
    m_socket = new QSslSocket();
    m_buffer.clear();
    m_userAgent = userAgent.toLatin1();
    if (m_userAgent.isEmpty())
        m_userAgent = "MMC/0.1";

    m_socket->connectToHost(host, static_cast<quint16>(port));
    if (!m_socket->waitForConnected(kConnectTimeoutMs)) {
        if (error) *error = QObject::tr("Cannot reach %1: %2").arg(host, m_socket->errorString());
        return false;
    }
    if (implicitTls) {
        m_socket->startClientEncryption();
        if (!m_socket->waitForEncrypted(kConnectTimeoutMs)) {
            if (error) *error = QObject::tr("TLS handshake with %1 failed: %2").arg(host, m_socket->errorString());
            return false;
        }
    }

    // Greeting.
    Response greeting;
    if (!readResponse(&greeting, error))
        return false;
    if (!greeting.line.startsWith("* OK") && !greeting.line.startsWith("* PREAUTH")) {
        if (error) *error = QObject::tr("The server refused the connection: %1")
                                .arg(QString::fromUtf8(greeting.line));
        return false;
    }

    if (!authenticate(user, authToken, secret, error))
        return false;
    return true;
}

bool ImapClient::authenticate(const QString& user, const QString& authToken,
                              const QString& secret, QString* error)
{
    QVector<Response> untagged;
    QByteArray tagged;
    if (!command(QByteArrayLiteral("CAPABILITY"), &untagged, &tagged, error))
        return false;

    QByteArray capabilities;
    for (const Response& r : untagged)
        if (r.line.startsWith("* CAPABILITY"))
            capabilities = r.line.mid(13);

    const auto tryXoauth2 = [&]() -> bool {
        if (!authToken.isEmpty() && capabilities.isEmpty())
            return false;
        QByteArray initial;
        initial += "user=" + user.toUtf8() + "\x01";
        initial += "auth=Bearer " + authToken.toUtf8() + "\x01\x01";
        const QByteArray encoded = initial.toBase64();

        const QByteArray tag = QByteArrayLiteral("mmc") + QByteArray::number(++m_tagCounter);
        if (!sendLine(tag + QByteArrayLiteral(" AUTHENTICATE XOAUTH2 ") + encoded, error))
            return false;
        for (;;) {
            Response response;
            if (!readResponse(&response, error))
                return false;
            if (response.line.startsWith(tag + QByteArrayLiteral(" "))) {
                if (response.line.contains(" OK"))
                    return true;
                if (error) {
                    *error = QObject::tr("XOAUTH2 login failed: %1")
                                 .arg(QString::fromUtf8(response.line.mid(tag.size() + 1)).trimmed());
                }
                return false;
            }
            if (response.line.startsWith("+ ")) {
                // The server wants the initial response on its own line.
                if (!sendLine(encoded, error))
                    return false;
                continue;
            }
        }
    };

    const auto tryPlain = [&]() -> bool {
        if (secret.isEmpty())
            return false;
        QByteArray initial;
        initial += '\0';
        initial += user.toUtf8();
        initial += '\0';
        initial += secret.toUtf8();
        const QByteArray encoded = initial.toBase64();
        if (!command(QByteArrayLiteral("AUTHENTICATE PLAIN ") + encoded, nullptr, nullptr, error))
            return false;
        return true;
    };

    if (!authToken.isEmpty() && tryXoauth2())
        return true;
    if (tryPlain())
        return true;

    if (error && error->isEmpty())
        *error = QObject::tr("Could not log in. No OAuth token and no app password worked.");
    return false;
}

bool ImapClient::listFolders(QVector<ImapFolderInfo>* folders, QString* error)
{
    QVector<Response> untagged;
    if (!command(QByteArrayLiteral("LIST \"\" \"*\""), &untagged, nullptr, error))
        return false;

    for (const Response& r : untagged) {
        if (!r.line.startsWith("* LIST"))
            continue;
        const int open = r.line.indexOf('(');
        const int close = r.line.indexOf(')', open);
        if (open < 0 || close < 0)
            continue;
        ImapFolderInfo info;
        info.flags = QString::fromLatin1(r.line.mid(open + 1, close - open - 1)).split(QChar(' '), Qt::SkipEmptyParts);
        QByteArray rest = r.line.mid(close + 1).trimmed();
        if (rest.startsWith('"')) {
            const int end = rest.indexOf('"', 1);
            if (end < 0)
                continue;
            info.delimiter = QString::fromLatin1(rest.mid(1, end - 1));
            rest = rest.mid(end + 1).trimmed();
        } else {
            const int sp = rest.indexOf(' ');
            if (sp > 0) {
                info.delimiter = QString::fromLatin1(rest.left(sp));
                rest = rest.mid(sp + 1).trimmed();
            }
        }
        if (rest.startsWith('"')) {
            const int end = rest.indexOf('"', 1);
            if (end < 0)
                continue;
            info.name = QString::fromLatin1(rest.mid(1, end - 1));
        } else {
            info.name = QString::fromLatin1(rest);
        }
        folders->append(info);
    }
    return true;
}

bool ImapClient::status(const QString& folder, ImapFolderStatus* status, QString* error)
{
    QVector<Response> untagged;
    const QByteArray cmd = QByteArrayLiteral("STATUS \"") + folder.toUtf8()
                           + QByteArrayLiteral("\" (MESSAGES UNSEEN RECENT UIDNEXT UIDVALIDITY)");
    if (!command(cmd, &untagged, nullptr, error))
        return false;

    for (const Response& r : untagged) {
        if (!r.line.startsWith("* STATUS"))
            continue;
        const int open = r.line.indexOf('(');
        const int close = r.line.lastIndexOf(')');
        if (open < 0 || close < 0)
            continue;
        const QVector<Node> items = parseItemList(r.line.mid(open + 1, close - open - 1));
        status->messages = nodeValue(items, "MESSAGES").toInt();
        status->unseen = nodeValue(items, "UNSEEN").toInt();
        status->recent = nodeValue(items, "RECENT").toInt();
        status->uidNext = nodeValue(items, "UIDNEXT").toLongLong();
        status->uidValidity = nodeValue(items, "UIDVALIDITY").toLongLong();
    }
    return true;
}

bool ImapClient::selectFolder(const QString& folder, ImapFolderStatus* status, QString* error)
{
    QVector<Response> untagged;
    const QByteArray cmd = QByteArrayLiteral("SELECT \"") + folder.toUtf8() + QByteArrayLiteral("\"");
    if (!command(cmd, &untagged, nullptr, error))
        return false;

    for (const Response& r : untagged) {
        if (!status)
            continue;
        const QString text = QString::fromLatin1(r.line);
        if (text.startsWith(QStringLiteral("* ")) && text.contains(QStringLiteral("EXISTS")))
            status->messages = text.mid(2).split(QChar(' ')).value(0).toInt();
        else if (text.startsWith(QStringLiteral("* ")) && text.contains(QStringLiteral("RECENT")))
            status->recent = text.mid(2).split(QChar(' ')).value(0).toInt();
        else if (text.contains(QStringLiteral("UIDVALIDITY")))
            status->uidValidity = nodeValue(parseItemList(text.section('[', 1, 1).toLatin1()),
                                            "UIDVALIDITY").toLongLong();
        else if (text.contains(QStringLiteral("UIDNEXT")))
            status->uidNext = nodeValue(parseItemList(text.section('[', 1, 1).toLatin1()),
                                        "UIDNEXT").toLongLong();
        else if (text.contains(QStringLiteral("UNSEEN")))
            status->unseen = nodeValue(parseItemList(text.section('[', 1, 1).toLatin1()),
                                       "UNSEEN").toInt();
    }
    return true;
}

bool ImapClient::fetchHeaders(const QString& folder, qint64 sinceUid,
                               QVector<ImapHeader>* headers, QString* error)
{
    // A mailbox has to be selected before FETCH is allowed.
    if (!selectFolder(folder, nullptr, error))
        return false;

    QVector<Response> untagged;
    QByteArray range = "1:*";
    if (sinceUid > 0)
        range = QByteArray::number(sinceUid + 1) + ":*";
    const QByteArray cmd = QByteArrayLiteral("UID FETCH ") + range
                           + QByteArrayLiteral(" (UID FLAGS RFC822.SIZE "
                                               "BODY.PEEK[HEADER.FIELDS (FROM TO SUBJECT DATE MESSAGE-ID)] "
                                               "BODY.PEEK[1]<0.300>)");
    if (!command(cmd, &untagged, nullptr, error))
        return false;

    for (const Response& r : untagged) {
        if (!r.line.contains("FETCH"))
            continue;
        const int open = r.line.indexOf('(');
        const int close = r.line.lastIndexOf(')');
        if (open < 0 || close < 0)
            continue;
        const QVector<Node> items = parseItemList(r.line.mid(open + 1, close - open - 1));

        ImapHeader header;
        header.uid = nodeValue(items, "UID");
        header.flags = nodeFlags(items, "FLAGS").join(QChar(' '));
        header.size = nodeValue(items, "RFC822.SIZE").toInt();

        if (!r.literals.isEmpty()) {
            const QByteArray raw = r.literals.at(0);
            const QList<QByteArray> lines = raw.split('\n');
            for (const QByteArray& line : lines) {
                const int colon = line.indexOf(':');
                if (colon <= 0)
                    continue;
                const QString key = QString::fromLatin1(line.left(colon)).trimmed().toLower();
                const QString value = QString::fromLatin1(line.mid(colon + 1)).trimmed();
                if (key == QLatin1String("from")) header.from = value;
                else if (key == QLatin1String("to")) header.to = value;
                else if (key == QLatin1String("subject")) header.subject = value;
                else if (key == QLatin1String("date")) header.date = value;
                else if (key == QLatin1String("message-id")) header.messageId = value;
            }
        }
        // The second literal holds the first words of the mail.
        if (r.literals.size() > 1)
            header.snippet = snippetFromBody(r.literals.at(1));
        if (!header.uid.isEmpty())
            headers->append(header);
    }
    return true;
}

bool ImapClient::search(const QString& folder, const QString& criteria,
                         QStringList* uids, QString* error)
{
    if (!selectFolder(folder, nullptr, error))
        return false;

    QVector<Response> untagged;
    if (!command(QByteArrayLiteral("UID SEARCH ") + criteria.toUtf8(), &untagged, nullptr, error))
        return false;

    for (const Response& r : untagged) {
        if (!r.line.startsWith("* SEARCH"))
            continue;
        const QStringList parts = QString::fromLatin1(r.line.mid(8)).split(QChar(' '), Qt::SkipEmptyParts);
        for (const QString& p : parts)
            uids->append(p);
    }
    return true;
}

bool ImapClient::fetchMessage(const QString& folder, const QString& uid,
                               QByteArray* raw, QString* error)
{
    QVector<Response> untagged;
    const QByteArray cmd = QByteArrayLiteral("UID FETCH ") + uid.toUtf8()
                           + QByteArrayLiteral(" (BODY.PEEK[])");
    if (!command(cmd, &untagged, nullptr, error))
        return false;

    for (const Response& r : untagged) {
        if (r.line.contains("FETCH") && !r.literals.isEmpty()) {
            *raw = r.literals.last();
            return true;
        }
    }
    if (error) *error = QObject::tr("The server did not return the message.");
    return false;
}

bool ImapClient::storeFlag(const QString& folder, const QString& uid,
                           const QString& flag, bool set, QString* error)
{
    Q_UNUSED(folder)
    const QByteArray cmd = QByteArrayLiteral("UID STORE ") + uid.toUtf8()
                           + (set ? QByteArrayLiteral(" +FLAGS (") : QByteArrayLiteral(" -FLAGS ("))
                           + flag.toUtf8() + QByteArrayLiteral(")");
    return command(cmd, nullptr, nullptr, error);
}

void ImapClient::logout()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QString error;
        command(QByteArrayLiteral("LOGOUT"), nullptr, nullptr, &error);
    }
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->close();
    }
}

} // namespace mmc

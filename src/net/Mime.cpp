#include "net/Mime.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QUuid>

namespace mmc {
namespace {

QByteArray decodeBase64(const QByteArray& in)
{
    QByteArray compact;
    compact.reserve(in.size());
    for (const char ch : in) {
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t')
            continue;
        compact.append(ch);
    }
    return QByteArray::fromBase64(compact);
}

QByteArray decodeQuotedPrintable(const QByteArray& in)
{
    QByteArray out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); ++i) {
        const char ch = in.at(i);
        if (ch == '=' && i + 2 < in.size()) {
            const char a = in.at(i + 1);
            const char b = in.at(i + 2);
            const auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = hex(a);
            const int lo = hex(b);
            if (hi >= 0 && lo >= 0) {
                out.append(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.append(ch);
    }
    return out;
}

QString decodeBytes(const QByteArray& data, const QString& charset)
{
    const QString cs = charset.toLower();
    if (cs.contains("utf-8") || cs.isEmpty())
        return QString::fromUtf8(data);
    if (cs.contains("latin1") || cs.contains("iso-8859-1") || cs.contains("windows-1252")) {
        QString s = QString::fromLatin1(data);
        // Windows-1252 differs from latin1 in the 0x80..0x9f range.
        for (int i = 0; i < s.size(); ++i) {
            const ushort c = s.at(i).unicode();
            if (c >= 0x80 && c <= 0x9f) {
                static const ushort cp1252[] = {
                    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
                    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
                s[i] = QChar(cp1252[c - 0x80]);
            }
        }
        return s;
    }
    if (cs.contains("utf-16")) {
        const QString s = QString::fromUtf8(data);
        return s;
    }
    return QString::fromUtf8(data);
}

QString contentTypeOf(const QMap<QString, QString>& headers, QString* charset, QString* boundary)
{
    const QString raw = headers.value(QStringLiteral("content-type"));
    const QString lower = raw.toLower();
    *charset = QString();
    *boundary = QString();

    QString type = QStringLiteral("text/plain");
    if (!raw.isEmpty()) {
        const int semi = raw.indexOf(QChar(';'));
        type = (semi < 0 ? raw : raw.left(semi)).trimmed().toLower();
        if (type.isEmpty())
            type = QStringLiteral("text/plain");
    }

    static const QRegularExpression charsetRe(QStringLiteral("charset\\s*=\\s*\"?([^\";]+)\"?"),
                                              QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch cm = charsetRe.match(raw);
    if (cm.hasMatch())
        *charset = cm.captured(1).trimmed();

    static const QRegularExpression boundaryRe(QStringLiteral("boundary\\s*=\\s*\"?([^\";]+)\"?"),
                                               QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch bm = boundaryRe.match(raw);
    if (bm.hasMatch())
        *boundary = bm.captured(1).trimmed();

    Q_UNUSED(lower)
    return type;
}

void parsePart(const QByteArray& raw, QVector<MimePart>* parts);

void parseMultipart(const QByteArray& body, const QString& boundary, QVector<MimePart>* parts)
{
    const QByteArray marker = "--" + boundary.toUtf8();
    QByteArray current;
    bool inPart = false;

    auto flush = [&]() {
        if (inPart)
            parsePart(current, parts);
        current.clear();
    };

    const QList<QByteArray> lines = body.split('\n');
    for (QByteArray line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.startsWith(marker)) {
            flush();
            inPart = true;
            continue;
        }
        if (inPart) {
            current += line;
            current += '\n';
        }
    }
    flush();
}

void parsePart(const QByteArray& raw, QVector<MimePart>* parts)
{
    const int split = raw.indexOf("\r\n\r\n");
    const int split2 = (split < 0) ? raw.indexOf("\n\n") : -1;
    int headerEnd = -1;
    QByteArray headerBlock;
    QByteArray body;
    if (split >= 0) {
        headerEnd = split + 4;
        headerBlock = raw.left(split);
        body = raw.mid(headerEnd);
    } else if (split2 >= 0) {
        headerEnd = split2 + 2;
        headerBlock = raw.left(split2);
        body = raw.mid(headerEnd);
    } else {
        headerBlock = raw;
        body.clear();
    }

    // Unfold continuation lines.
    QByteArray unfolded;
    const QList<QByteArray> hlines = headerBlock.split('\n');
    for (QByteArray line : hlines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if ((line.startsWith(' ') || line.startsWith('\t')) && !unfolded.isEmpty())
            unfolded += ' ' + line.trimmed();
        else
            unfolded += line + '\n';
    }

    QMap<QString, QString> headers;
    for (const QByteArray& line : unfolded.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString key = QString::fromLatin1(line.left(colon)).trimmed().toLower();
        const QString value = QString::fromLatin1(line.mid(colon + 1)).trimmed();
        if (headers.contains(key))
            headers[key] += QChar(' ') + value;
        else
            headers.insert(key, value);
    }

    QString charset;
    QString boundary;
    const QString type = contentTypeOf(headers, &charset, &boundary);

    if (type.startsWith(QStringLiteral("multipart/")) && !boundary.isEmpty()) {
        parseMultipart(body, boundary, parts);
        return;
    }

    const QString encoding = headers.value(QStringLiteral("content-transfer-encoding")).toLower().trimmed();
    QByteArray data = body;
    if (encoding == QStringLiteral("base64"))
        data = decodeBase64(body);
    else if (encoding == QStringLiteral("quoted-printable"))
        data = decodeQuotedPrintable(body);

    MimePart part;
    part.contentType = type;
    part.charset = charset;
    part.data = data;

    const QString disp = headers.value(QStringLiteral("content-disposition")).toLower();
    if (disp.contains(QStringLiteral("attachment")) || disp.contains(QStringLiteral("filename"))) {
        part.isAttachment = true;
        static const QRegularExpression fnRe(QStringLiteral("filename\\s*=\\s*\"?([^\";]+)\"?"),
                                             QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = fnRe.match(headers.value(QStringLiteral("content-disposition")));
        if (m.hasMatch())
            part.filename = m.captured(1).trimmed();
    }
    if (part.filename.isEmpty() && part.isAttachment)
        part.filename = QStringLiteral("attachment");

    if (!parts)
        return;
    if (!part.isAttachment && type == QStringLiteral("text/plain") && !parts->isEmpty()) {
        // Keep the first plain text part only if it is empty so far.
        bool hasText = false;
        for (const MimePart& p : *parts)
            if (p.contentType == QStringLiteral("text/plain"))
                hasText = true;
        if (hasText)
            return;
    }
    parts->append(part);
}

} // namespace

QString ParsedMessage::header(const QString& name) const
{
    return headers.value(name.toLower());
}

QString ParsedMessage::bodyText() const
{
    for (const MimePart& p : parts)
        if (p.contentType == QStringLiteral("text/plain") && !p.isAttachment)
            return decodeBytes(p.data, p.charset);
    return {};
}

QString ParsedMessage::bodyHtml() const
{
    for (const MimePart& p : parts)
        if (p.contentType == QStringLiteral("text/html") && !p.isAttachment)
            return decodeBytes(p.data, p.charset);
    return {};
}

QVector<MimePart> ParsedMessage::attachments() const
{
    QVector<MimePart> out;
    for (const MimePart& p : parts)
        if (p.isAttachment)
            out << p;
    return out;
}

QString ParsedMessage::displayFrom() const
{
    return decodeHeaderValue(header(QStringLiteral("from")));
}

QString decodeHeaderValue(const QString& value)
{
    if (!value.contains(QStringLiteral("=?")))
        return value;

    QRegularExpression re(QStringLiteral("=\\?([^?]+)\\?([BbQq])\\?([^?]*)\\?="));
    QString out = value;
    int offset = 0;
    auto it = re.globalMatch(value);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString charset = m.captured(1);
        QByteArray payload = m.captured(3).toLatin1();
        QByteArray decoded;
        if (m.captured(2).compare(QStringLiteral("b"), Qt::CaseInsensitive) == 0)
            decoded = decodeBase64(payload);
        else
            decoded = decodeQuotedPrintable(payload.replace('_', ' '));
        const QString text = decodeBytes(decoded, charset);
        out.replace(m.capturedStart(0) + offset, m.capturedLength(0), text);
        offset += text.size() - m.capturedLength(0);
    }
    return out;
}

QString senderName(const QString& from)
{
    const QString decoded = decodeHeaderValue(from);
    const int bracket = decoded.indexOf(QChar('<'));
    if (bracket < 0)
        return decoded.trimmed();
    QString name = decoded.left(bracket).trimmed();
    if (name.startsWith(QChar('"')) && name.endsWith(QChar('"')) && name.size() > 1)
        name = name.mid(1, name.size() - 2);
    return name.isEmpty() ? decoded.trimmed() : name;
}

QString sanitizeMailHtml(const QString& html, bool allowRemoteImages)
{
    QString out = html;

    // Scripts and the code that goes with them.
    static const QRegularExpression scriptRe(QStringLiteral("<script\\b[^>]*>.*?</script>"),
                                             QRegularExpression::CaseInsensitiveOption
                                                 | QRegularExpression::DotMatchesEverythingOption);
    out.remove(scriptRe);
    static const QRegularExpression styleRe(QStringLiteral("<style\\b[^>]*>.*?</style>"),
                                            QRegularExpression::CaseInsensitiveOption
                                                | QRegularExpression::DotMatchesEverythingOption);
    out.remove(styleRe);
    static const QRegularExpression eventRe(
        QStringLiteral("\\son[a-z]+\\s*=\\s*(\"[^\"]*\"|'[^']*'|[^\\s>]+)"),
        QRegularExpression::CaseInsensitiveOption);
    out.remove(eventRe);
    out.remove(QStringLiteral("javascript:"), Qt::CaseInsensitive);

    if (!allowRemoteImages) {
        // Pictures that are not in the mail are left out, so a mail cannot
        // tell the server that it was opened.
        static const QRegularExpression imageRe(
            QStringLiteral("<img\\b[^>]*\\bsrc\\s*=\\s*(\"https?://[^\"]*\"|'https?://[^']*')[^>]*>"),
            QRegularExpression::CaseInsensitiveOption);
        out.replace(imageRe, QStringLiteral("<span>[image]</span>"));
        // The same for the background of a table.
        static const QRegularExpression backgroundRe(
            QStringLiteral("background(-image)?\\s*=\\s*[^;\"']*url\\s*\\(\\s*['\"]?https?://[^)]*\\)"),
            QRegularExpression::CaseInsensitiveOption);
        out.remove(backgroundRe);
    }
    return out;
}

QString snippetFromBody(const QByteArray& body)
{
    QString text = QString::fromUtf8(body.left(400));
    if (text.contains(QChar('<'))) {
        // It is html, take the words out of it.
        static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));
        text.remove(tagRe);
        static const QRegularExpression spaceRe(QStringLiteral("&nbsp;"));
        text.replace(spaceRe, QStringLiteral(" "));
    }
    text.replace(QChar('\r'), QChar(' '));
    text.replace(QChar('\n'), QChar(' '));
    text.replace(QChar('\t'), QChar(' '));
    while (text.contains(QStringLiteral("  ")))
        text.replace(QStringLiteral("  "), QStringLiteral(" "));
    return text.trimmed().left(200);
}

ParsedMessage parseMime(const QByteArray& raw)
{    ParsedMessage msg;
    const int split = raw.indexOf("\r\n\r\n");
    const int split2 = (split < 0) ? raw.indexOf("\n\n") : -1;
    QByteArray headerBlock;
    QByteArray body;
    if (split >= 0) {
        headerBlock = raw.left(split);
        body = raw.mid(split + 4);
    } else if (split2 >= 0) {
        headerBlock = raw.left(split2);
        body = raw.mid(split2 + 2);
    } else {
        headerBlock = raw;
    }

    QByteArray unfolded;
    for (QByteArray line : headerBlock.split('\n')) {
        if (line.endsWith('\r'))
            line.chop(1);
        if ((line.startsWith(' ') || line.startsWith('\t')) && !unfolded.isEmpty())
            unfolded += ' ' + line.trimmed();
        else
            unfolded += line + '\n';
    }

    for (const QByteArray& line : unfolded.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString key = QString::fromLatin1(line.left(colon)).trimmed().toLower();
        const QString value = QString::fromLatin1(line.mid(colon + 1)).trimmed();
        if (msg.headers.contains(key))
            msg.headers[key] += QChar(' ') + value;
        else
            msg.headers.insert(key, value);
    }

    QVector<MimePart> parts;
    parsePart(raw, &parts);
    msg.parts = parts;
    return msg;
}

QByteArray buildMimeMessage(const QString& from,
                             const QStringList& to,
                             const QStringList& cc,
                             const QString& subject,
                             const QString& bodyText,
                             const QString& bodyHtml,
                             const QVector<OutgoingAttachment>& attachments)
{
    const QString boundary = QStringLiteral("mmc-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString altBoundary = QStringLiteral("mmc-alt-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    QByteArray out;
    out += "MIME-Version: 1.0\r\n";
    out += QStringLiteral("From: %1\r\n").arg(from).toUtf8();
    if (!to.isEmpty())
        out += QStringLiteral("To: %1\r\n").arg(to.join(QStringLiteral(", "))).toUtf8();
    if (!cc.isEmpty())
        out += QStringLiteral("Cc: %1\r\n").arg(cc.join(QStringLiteral(", "))).toUtf8();
    out += QStringLiteral("Subject: %1\r\n").arg(subject).toUtf8();
    out += QStringLiteral("Date: %1\r\n").arg(QDateTime::currentDateTime().toString(Qt::RFC2822Date)).toUtf8();
    out += QStringLiteral("Message-ID: <%1@mmc>\r\n").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)).toUtf8();
    out += QByteArrayLiteral("User-Agent: MMC/0.1\r\n");

    if (attachments.isEmpty()) {
        out += QStringLiteral("Content-Type: multipart/alternative; boundary=\"%1\"\r\n\r\n").arg(altBoundary).toUtf8();
        out += QStringLiteral("--%1\r\n").arg(altBoundary).toUtf8();
        out += "Content-Type: text/plain; charset=utf-8\r\n";
        out += "Content-Transfer-Encoding: base64\r\n\r\n";
        out += bodyText.toUtf8().toBase64() + "\r\n";
        if (!bodyHtml.isEmpty()) {
            out += QStringLiteral("--%1\r\n").arg(altBoundary).toUtf8();
            out += "Content-Type: text/html; charset=utf-8\r\n";
            out += "Content-Transfer-Encoding: base64\r\n\r\n";
            out += bodyHtml.toUtf8().toBase64() + "\r\n";
        }
        out += QStringLiteral("--%1--\r\n").arg(altBoundary).toUtf8();
        return out;
    }

    out += QStringLiteral("Content-Type: multipart/mixed; boundary=\"%1\"\r\n\r\n").arg(boundary).toUtf8();

    out += QStringLiteral("--%1\r\n").arg(boundary).toUtf8();
    out += QStringLiteral("Content-Type: multipart/alternative; boundary=\"%1\"\r\n\r\n").arg(altBoundary).toUtf8();
    out += QStringLiteral("--%1\r\n").arg(altBoundary).toUtf8();
    out += "Content-Type: text/plain; charset=utf-8\r\n";
    out += "Content-Transfer-Encoding: base64\r\n\r\n";
    out += bodyText.toUtf8().toBase64() + "\r\n";
    if (!bodyHtml.isEmpty()) {
        out += QStringLiteral("--%1\r\n").arg(altBoundary).toUtf8();
        out += "Content-Type: text/html; charset=utf-8\r\n";
        out += "Content-Transfer-Encoding: base64\r\n\r\n";
        out += bodyHtml.toUtf8().toBase64() + "\r\n";
    }
    out += QStringLiteral("--%1--\r\n").arg(altBoundary).toUtf8();

    for (const OutgoingAttachment& att : attachments) {
        out += QStringLiteral("--%1\r\n").arg(boundary).toUtf8();
        out += QStringLiteral("Content-Type: %1; name=\"%2\"\r\n").arg(att.contentType, att.filename).toUtf8();
        out += QStringLiteral("Content-Disposition: attachment; filename=\"%2\"\r\n").arg(boundary, att.filename).toUtf8();
        out += "Content-Transfer-Encoding: base64\r\n\r\n";
        out += att.data.toBase64() + "\r\n";
    }
    out += QStringLiteral("--%1--\r\n").arg(boundary).toUtf8();
    return out;
}

} // namespace mmc

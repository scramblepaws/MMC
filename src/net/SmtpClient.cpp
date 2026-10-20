#include "net/SmtpClient.h"

#include <QAbstractSocket>
#include <QSslSocket>

namespace mmc {
namespace {
constexpr int kTimeoutMs = 60000;
}

SmtpClient::SmtpClient() = default;

SmtpClient::~SmtpClient()
{
    disconnect();
    delete m_socket;
}

bool SmtpClient::readResponse(QByteArray* line, QString* error)
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
        if (!m_socket->waitForReadyRead(kTimeoutMs)) {
            if (error) *error = QObject::tr("Timed out while waiting for the mail server.");
            return false;
        }
        m_buffer += m_socket->readAll();
    }
}

bool SmtpClient::expect(int code, QByteArray* line, QString* error)
{
    for (;;) {
        QByteArray response;
        if (!readResponse(&response, error))
            return false;
        // Multi line replies use "250-" for every line but the last.
        if (response.size() >= 4 && response.at(3) == '-')
            continue;
        const int got = response.left(3).toInt();
        if (line)
            *line = response;
        if (got != code) {
            if (error)
                *error = QObject::tr("Mail server said: %1").arg(QString::fromUtf8(response));
            return false;
        }
        return true;
    }
}

bool SmtpClient::command(const QByteArray& cmd, int expectedCode, QByteArray* response, QString* error)
{
    m_socket->write(cmd + "\r\n");
    if (!m_socket->waitForBytesWritten(kTimeoutMs)) {
        if (error) *error = QObject::tr("Could not send to the mail server.");
        return false;
    }
    return expect(expectedCode, response, error);
}

bool SmtpClient::connect(const QString& host, int port, bool implicitTls,
                         const QString& user, const QString& authToken,
                         const QString& secret, const QString& userAgent, QString* error)
{
    delete m_socket;
    m_socket = new QSslSocket();
    m_buffer.clear();

    m_socket->connectToHost(host, static_cast<quint16>(port));
    if (!m_socket->waitForConnected(kTimeoutMs)) {
        if (error) *error = QObject::tr("Cannot reach %1: %2").arg(host, m_socket->errorString());
        return false;
    }
    if (implicitTls) {
        m_socket->startClientEncryption();
        if (!m_socket->waitForEncrypted(kTimeoutMs)) {
            if (error) *error = QObject::tr("TLS handshake with %1 failed: %2").arg(host, m_socket->errorString());
            return false;
        }
    }

    if (!expect(220, nullptr, error))
        return false;

    QByteArray ehlo = "EHLO " + userAgent.toLatin1();
    if (ehlo.size() < 5)
        ehlo = "EHLO mmc.local";
    QByteArray response;
    if (!command(ehlo, 250, &response, error)) {
        // Some servers only speak HELO.
        if (!command("HELO " + userAgent.toLatin1(), 250, &response, error))
            return false;
    }

    return authenticate(user, authToken, secret, error);
}

bool SmtpClient::authenticate(const QString& user, const QString& authToken,
                              const QString& secret, QString* error)
{
    if (!authToken.isEmpty()) {
        QByteArray initial;
        initial += "user=" + user.toUtf8() + "\x01";
        initial += "auth=Bearer " + authToken.toUtf8() + "\x01\x01";
        const QByteArray encoded = initial.toBase64();
        m_socket->write("AUTH XOAUTH2 " + encoded + "\r\n");
        if (!m_socket->waitForBytesWritten(kTimeoutMs)) {
            if (error) *error = QObject::tr("Could not send to the mail server.");
            return false;
        }
        QByteArray response;
        if (expect(235, &response, error))
            return true;
    }

    if (!secret.isEmpty()) {
        if (command("AUTH PLAIN", 334, nullptr, error)) {
            QByteArray initial;
            initial += '\0';
            initial += user.toUtf8();
            initial += '\0';
            initial += secret.toUtf8();
            m_socket->write(initial.toBase64() + "\r\n");
            if (!m_socket->waitForBytesWritten(kTimeoutMs)) {
                if (error) *error = QObject::tr("Could not send to the mail server.");
                return false;
            }
            if (expect(235, nullptr, error))
                return true;
        }
    }

    if (error && error->isEmpty())
        *error = QObject::tr("The mail server rejected the login.");
    return false;
}

bool SmtpClient::send(const QString& from, const QStringList& to,
                       const QByteArray& raw, QString* error)
{
    if (to.isEmpty()) {
        if (error) *error = QObject::tr("There is no recipient.");
        return false;
    }
    if (!command(QStringLiteral("MAIL FROM:<%1>").arg(from).toUtf8(), 250, nullptr, error))
        return false;
    for (const QString& rcpt : to) {
        if (!command(QStringLiteral("RCPT TO:<%1>").arg(rcpt).toUtf8(), 250, nullptr, error))
            return false;
    }
    if (!command("DATA", 354, nullptr, error))
        return false;

    // Dot stuffing: a line that is only a dot ends the message.
    QByteArray body = raw;
    body.replace("\r\n.", "\r\n..");
    if (!body.endsWith("\r\n"))
        body += "\r\n";
    body += ".\r\n";
    m_socket->write(body);
    if (!m_socket->waitForBytesWritten(kTimeoutMs)) {
        if (error) *error = QObject::tr("Could not send the message.");
        return false;
    }
    return expect(250, nullptr, error);
}

void SmtpClient::disconnect()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write("QUIT\r\n");
        m_socket->waitForBytesWritten(2000);
    }
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->close();
    }
}

} // namespace mmc

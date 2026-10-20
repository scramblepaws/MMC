#pragma once

#include <QByteArray>
#include <QString>

class QSslSocket;

namespace mmc {

// Blocking SMTP submission client (one message at a time).
class SmtpClient
{
public:
    SmtpClient();
    ~SmtpClient();

    bool connect(const QString& host, int port, bool implicitTls,
                 const QString& user, const QString& authToken,
                 const QString& secret, const QString& userAgent, QString* error);

    // raw must be a complete message including the headers.
    bool send(const QString& from, const QStringList& to,
              const QByteArray& raw, QString* error);

    void disconnect();

private:
    bool readResponse(QByteArray* line, QString* error);
    bool expect(int code, QByteArray* line, QString* error);
    bool command(const QByteArray& cmd, int expectedCode, QByteArray* response, QString* error);
    bool authenticate(const QString& user, const QString& authToken,
                      const QString& secret, QString* error);

    QSslSocket* m_socket = nullptr;
    QByteArray m_buffer;
};

} // namespace mmc

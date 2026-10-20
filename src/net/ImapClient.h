#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

class QSslSocket;

namespace mmc {

struct ImapFolderInfo {
    QString name;
    QString delimiter = "/";
    QStringList flags;
};

struct ImapFolderStatus {
    int messages = 0;
    int unseen = 0;
    int recent = 0;
    qint64 uidValidity = 0;
    qint64 uidNext = 0;
};

struct ImapHeader {
    QString uid;
    QString from;
    QString to;
    QString subject;
    QString date;
    QString messageId;
    QString flags;
    int size = 0;
};

// A blocking IMAP client for one mailbox at a time. The UI runs it on a
// worker thread.
class ImapClient
{
public:
    ImapClient();
    ~ImapClient();

    // authToken is used for XOAUTH2, secret (app password) for PLAIN/LOGIN.
    bool connect(const QString& host, int port, bool implicitTls,
                 const QString& user, const QString& authToken,
                 const QString& secret, const QString& userAgent, QString* error);

    bool listFolders(QVector<ImapFolderInfo>* folders, QString* error);
    bool status(const QString& folder, ImapFolderStatus* status, QString* error);
    bool selectFolder(const QString& folder, ImapFolderStatus* status, QString* error);
    bool fetchHeaders(const QString& folder, qint64 sinceUid,
                      QVector<ImapHeader>* headers, QString* error);
    bool search(const QString& folder, const QString& criteria,
                QStringList* uids, QString* error);
    bool fetchMessage(const QString& folder, const QString& uid,
                      QByteArray* raw, QString* error);
    bool storeFlag(const QString& folder, const QString& uid,
                   const QString& flag, bool set, QString* error);
    void logout();

private:
    struct Response {
        QByteArray line;
        QVector<QByteArray> literals;
    };

    bool sendLine(const QByteArray& line, QString* error);
    bool readResponse(Response* response, QString* error);
    bool readRawLine(QByteArray* line, QString* error);
    bool readExact(int count, QByteArray* data, QString* error);
    bool command(const QByteArray& cmd, QVector<Response>* untagged,
                 QByteArray* tagged, QString* error);
    bool authenticate(const QString& user, const QString& authToken,
                      const QString& secret, QString* error);

    QSslSocket* m_socket = nullptr;
    QByteArray m_buffer;
    int m_tagCounter = 0;
    QByteArray m_userAgent = QByteArrayLiteral("MMC/0.1");
};

} // namespace mmc

#include <QSemaphore>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include "net/ImapClient.h"
#include "test_harness.h"

using namespace mmc;

namespace {

// A tiny IMAP server that answers with canned data, enough to exercise the
// client without touching a real mail server.
class FakeImapServer : public QTcpServer
{
public:
    FakeImapServer() { listen(QHostAddress::LocalHost, 0); }

protected:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket* socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        socket->write("* OK [CAPABILITY IMAP4rev1 AUTH=XOAUTH2] fake server ready\r\n");
    }

private:
    void onReadyRead(QTcpSocket* socket)
    {
        m_buffer += socket->readAll();
        int index;
        while ((index = m_buffer.indexOf('\n')) >= 0) {
            QByteArray line = m_buffer.left(index);
            m_buffer.remove(0, index + 1);
            if (line.endsWith('\r'))
                line.chop(1);
            if (line.isEmpty())
                continue;
            respond(socket, line);
        }
    }

    void respond(QTcpSocket* socket, const QByteArray& line)
    {
        const QList<QByteArray> parts = line.split(' ');
        const QByteArray tag = parts.value(0);
        // Commands can be prefixed with UID, for example "UID FETCH 1:*".
        int commandIndex = 1;
        if (parts.value(1).toUpper() == "UID")
            commandIndex = 2;
        const QByteArray command = parts.value(commandIndex).toUpper();

        if (command == "CAPABILITY") {
            socket->write("* CAPABILITY IMAP4rev1 AUTH=XOAUTH2\r\n" + tag + " OK capability\r\n");
            return;
        }
        if (command == "AUTHENTICATE") {
            socket->write(tag + " OK logged in\r\n");
            return;
        }
        if (command == "LIST") {
            socket->write("* LIST (\\HasNoChildren) \"/\" \"INBOX\"\r\n"
                          "* LIST (\\HasNoChildren) \"/\" \"Sent\"\r\n"
                          + tag + " OK list done\r\n");
            return;
        }
        if (command == "SELECT") {
            socket->write("* 2 EXISTS\r\n"
                          "* 0 RECENT\r\n"
                          "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
                          "* OK [UIDVALIDITY 7] UIDs valid\r\n"
                          "* OK [UIDNEXT 3] Predicted next UID\r\n"
                          + tag + " OK select done\r\n");
            return;
        }
        if (command == "STATUS") {
            socket->write("* STATUS \"INBOX\" (MESSAGES 2 UNSEEN 1 RECENT 0 UIDNEXT 3 UIDVALIDITY 7)\r\n"
                          + tag + " OK status done\r\n");
            return;
        }
        if (command == "FETCH") {
            if (line.contains("BODY.PEEK[]")) {
                const QByteArray message =
                    "From: Someone <someone@gmail.com>\r\n"
                    "Subject: Hello there\r\n"
                    "\r\n"
                    "The body of the message.\r\n";
                socket->write("* 1 FETCH (UID 1 BODY[] {" + QByteArray::number(message.size()) + "}\r\n"
                              + message + ")\r\n" + tag + " OK fetch done\r\n");
            } else {
                const QByteArray headers =
                    "From: Someone <someone@gmail.com>\r\n"
                    "To: me@example.com\r\n"
                    "Subject: Hello there\r\n"
                    "Date: Mon, 1 Jan 2024 10:00:00 +0000\r\n"
                    "Message-ID: <abc@example.com>\r\n";
                socket->write("* 1 FETCH (UID 1 FLAGS (\\Seen) RFC822.SIZE 42 "
                              "BODY[HEADER.FIELDS (FROM TO SUBJECT DATE MESSAGE-ID)] {"
                              + QByteArray::number(headers.size()) + "}\r\n"
                              + headers + ")\r\n" + tag + " OK fetch done\r\n");
            }
            return;
        }
        if (command == "LOGOUT") {
            socket->write("* BYE\r\n" + tag + " OK logout\r\n");
            return;
        }
        socket->write(tag + " BAD unknown command\r\n");
    }

    QByteArray m_buffer;
};

// The server needs an event loop of its own to accept connections, so it runs
// on a thread.
class FakeImapServerThread : public QThread
{
public:
    int port() const { return m_server ? m_server->serverPort() : 0; }

    bool waitForServer()
    {
        m_ready.acquire();
        return m_server != nullptr;
    }

    void stop()
    {
        quit();
        wait(2000);
    }

protected:
    void run() override
    {
        m_server = new FakeImapServer();
        m_ready.release();
        exec();
    }

private:
    FakeImapServer* m_server = nullptr;
    QSemaphore m_ready;
};

} // namespace

void test_imap(TestRunner& runner)
{
    FakeImapServerThread serverThread;
    serverThread.start();
    CHECK(serverThread.waitForServer());
    CHECK(serverThread.port() > 0);
    if (serverThread.port() <= 0) {
        serverThread.stop();
        return;
    }

    ImapClient client;
    QString error;
    const bool connected =
        client.connect(QStringLiteral("127.0.0.1"), serverThread.port(), false,
                       QStringLiteral("someone@gmail.com"),
                       QStringLiteral("fake-token"), QString(),
                       QStringLiteral("MMC/test"), &error);
    CHECK(connected);
    if (!connected) {
        std::printf("   connect error: %s\n", qPrintable(error));
        serverThread.stop();
        return;
    }

    QVector<ImapFolderInfo> folders;
    CHECK(client.listFolders(&folders, &error));
    CHECK_EQ(folders.size(), 2);
    CHECK_EQ(folders.at(0).name, QStringLiteral("INBOX"));
    CHECK_EQ(folders.at(0).delimiter, QStringLiteral("/"));

    ImapFolderStatus status;
    CHECK(client.status(QStringLiteral("INBOX"), &status, &error));
    CHECK_EQ(status.messages, 2);
    CHECK_EQ(status.unseen, 1);
    CHECK_EQ(status.uidNext, 3);

    QVector<ImapHeader> headers;
    CHECK(client.fetchHeaders(QStringLiteral("INBOX"), 0, &headers, &error));
    CHECK_EQ(headers.size(), 1);
    CHECK_EQ(headers.at(0).uid, QStringLiteral("1"));
    CHECK_EQ(headers.at(0).subject, QStringLiteral("Hello there"));
    CHECK_EQ(headers.at(0).from, QStringLiteral("Someone <someone@gmail.com>"));
    CHECK_EQ(headers.at(0).size, 42);
    CHECK(headers.at(0).flags.contains(QStringLiteral("\\Seen")));

    QByteArray raw;
    CHECK(client.fetchMessage(QStringLiteral("INBOX"), QStringLiteral("1"), &raw, &error));
    CHECK(raw.contains("The body of the message."));

    client.logout();
    serverThread.stop();
}

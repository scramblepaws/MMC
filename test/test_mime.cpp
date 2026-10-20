#include "net/Mime.h"
#include "test_harness.h"

using namespace mmc;

void test_mime(TestRunner& runner)
{
    const QByteArray plainRaw =
        "From: Someone <someone@gmail.com>\r\n"
        "To: me@example.com\r\n"
        "Subject: Hello\r\n"
        "Date: Mon, 1 Jan 2024 10:00:00 +0000\r\n"
        "\r\n"
        "This is the body.\r\n";

    const ParsedMessage plain = parseMime(plainRaw);
    CHECK_EQ(plain.header("from"), QStringLiteral("Someone <someone@gmail.com>"));
    CHECK_EQ(plain.header("subject"), QStringLiteral("Hello"));
    CHECK(plain.bodyText().startsWith(QStringLiteral("This is the body.")));
    CHECK(plain.bodyHtml().isEmpty());
    CHECK_EQ(plain.attachments().size(), 0);

    const QByteArray multipartRaw =
        "From: a@b.com\r\n"
        "Subject: With file\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=\"abc123\"\r\n"
        "\r\n"
        "--abc123\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "\r\n"
        "Hello there\r\n"
        "--abc123\r\n"
        "Content-Type: application/octet-stream; name=\"data.bin\"\r\n"
        "Content-Disposition: attachment; filename=\"data.bin\"\r\n"
        "Content-Transfer-Encoding: base64\r\n"
        "\r\n"
        "aGVsbG8gZmlsZQ==\r\n"
        "--abc123--\r\n";

    const ParsedMessage multipart = parseMime(multipartRaw);
    CHECK(multipart.bodyText().contains(QStringLiteral("Hello there")));
    CHECK_EQ(multipart.attachments().size(), 1);
    CHECK_EQ(multipart.attachments().first().filename, QStringLiteral("data.bin"));
    CHECK(multipart.attachments().first().data == QByteArray("hello file"));

    CHECK_EQ(decodeHeaderValue(QStringLiteral("=?UTF-8?B?SGVsbG8gV2VsdA==?=")),
             QStringLiteral("Hello Welt"));

    const QByteArray quotedRaw =
        "Subject: =?UTF-8?Q?Caf=C3=A9?=\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n"
        "\r\n"
        "Caf=C3=A9\r\n";
    const ParsedMessage quoted = parseMime(quotedRaw);
    CHECK(quoted.bodyText().trimmed() == QString::fromUtf8("Café"));

    QVector<OutgoingAttachment> attachments;
    OutgoingAttachment att;
    att.filename = QStringLiteral("note.txt");
    att.data = QByteArray("file content");
    attachments << att;

    const QByteArray built = buildMimeMessage(QStringLiteral("me@gmail.com"),
                                               {QStringLiteral("you@gmail.com")},
                                               {},
                                               QStringLiteral("Hi"),
                                               QStringLiteral("Body text"),
                                               QString(),
                                               attachments);
    CHECK(built.contains("Subject: Hi"));
    CHECK(built.contains("To: you@gmail.com"));
    CHECK(built.contains("multipart/mixed"));

    const ParsedMessage reparsed = parseMime(built);
    CHECK_EQ(reparsed.attachments().size(), 1);
    CHECK(reparsed.attachments().first().data == QByteArray("file content"));
    CHECK(reparsed.bodyText().contains(QStringLiteral("Body text")));
}

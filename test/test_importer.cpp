#include "core/Importer.h"
#include "test_harness.h"

using namespace mmc;

void test_importer(TestRunner& runner)
{
    const ImportResult simple = importMailboxesFromText("a@gmail.com\nb@outlook.com\n");
    CHECK_EQ(simple.mailboxes.size(), 2);
    CHECK_EQ(simple.mailboxes.at(0).address, QStringLiteral("a@gmail.com"));
    CHECK_EQ(simple.mailboxes.at(0).providerId, QStringLiteral("google"));
    CHECK_EQ(simple.mailboxes.at(1).providerId, QStringLiteral("microsoft"));

    const ImportResult semicolons =
        importMailboxesFromText("a@gmail.com;Alpha;pass1\nb@gmail.com;Beta;pass2\n");
    CHECK_EQ(semicolons.mailboxes.size(), 2);
    CHECK_EQ(semicolons.mailboxes.at(0).displayName, QStringLiteral("Alpha"));
    CHECK(semicolons.mailboxes.at(0).secret == QByteArray("pass1"));

    const ImportResult tabs = importMailboxesFromText("a@gmail.com\tAlpha\n");
    CHECK_EQ(tabs.mailboxes.size(), 1);
    CHECK_EQ(tabs.mailboxes.at(0).displayName, QStringLiteral("Alpha"));

    const ImportResult yandex = importMailboxesFromText("someone@yandex.ru\n");
    CHECK_EQ(yandex.mailboxes.size(), 1);
    CHECK_EQ(yandex.mailboxes.at(0).providerId, QStringLiteral("yandex"));

    const ImportResult broken = importMailboxesFromText("a@gmail.com\nnot an address\nb@gmail.com\n");
    CHECK_EQ(broken.mailboxes.size(), 2);
    CHECK_EQ(broken.skipped, 1);
    CHECK_EQ(broken.errors.size(), 1);

    const ImportResult withHeader = importMailboxesFromText("address,provider\na@gmail.com,google\n");
    CHECK_EQ(withHeader.mailboxes.size(), 1);

    const ImportResult empty = importMailboxesFromText("");
    CHECK_EQ(empty.mailboxes.size(), 0);
    CHECK_EQ(empty.errors.size(), 1);
}

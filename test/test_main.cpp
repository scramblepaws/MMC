#include <QCoreApplication>
#include <QDateTime>
#include <QUuid>
#include <cstdio>

#include "core/Vault.h"
#include "test_harness.h"

void test_crypto(TestRunner& runner);
void test_vault(TestRunner& runner);
void test_mime(TestRunner& runner);
void test_importer(TestRunner& runner);
void test_imap(TestRunner& runner);

int main(int argc, char** argv)
{
    // Unbuffered, so the output survives a crash.
    setvbuf(stdout, nullptr, _IONBF, 0);

    // A small helper for making a vault from the command line, used to try the
    // program out with a vault that already exists.
    if (argc >= 4 && QString::fromLatin1(argv[1]) == QLatin1String("--make-vault")) {
        mmc::Vault vault;
        QString error;
        if (!vault.create(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]), &error)) {
            std::printf("%s\n", qPrintable(error));
            return 1;
        }
        std::printf("vault written to %s\n", argv[2]);
        return 0;
    }

    // A vault with made up accounts and mail, to look at the program without
    // signing in anywhere.
    if (argc >= 4 && QString::fromLatin1(argv[1]) == QLatin1String("--demo-vault")) {
        mmc::Vault vault;
        QString error;
        if (!vault.create(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]), &error)) {
            std::printf("%s\n", qPrintable(error));
            return 1;
        }

        struct Demo {
            const char* provider;
            const char* address;
            const char* name;
            int unread;
        };
        const Demo demos[] = {
            {"google", "steam1@gmail.com", "Steam 1", 3},
            {"google", "steam2@gmail.com", "Steam 2", 0},
            {"microsoft", "steam3@outlook.com", "Steam 3", 1},
            {"yandex", "steam4@yandex.ru", "Steam 4", 0},
        };

        const char* subjects[] = {
            "Your Steam Guard code",
            "Steam: you have a pending trade offer",
            "Your verification code is 418 552",
            "Welcome to the Steam Beta",
            "Password changed on your account",
            "Steam Community: Weekly survey",
            "Your account details were changed",
            "News from the Steam store",
        };
        const char* senders[] = {
            "Steam Support <help@steampowered.com>",
            "Steam <noreply@steampowered.com>",
            "Google <no-reply@accounts.google.com>",
            "Valve <support@valvesoftware.com>",
        };

        QDateTime base = QDateTime::currentDateTime().addDays(-2);
        for (const Demo& demo : demos) {
            mmc::Mailbox mailbox;
            mailbox.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            mailbox.providerId = QLatin1String(demo.provider);
            mailbox.address = QLatin1String(demo.address);
            mailbox.displayName = QLatin1String(demo.name);
            mailbox.label = mailbox.displayName.left(2).toUpper();
            vault.state().mailboxes.append(mailbox);
            vault.state().settings[QStringLiteral("unseen.%1").arg(mailbox.id)] =
                QString::number(demo.unread);

            for (int i = 0; i < 3; ++i) {
                mmc::Message message;
                message.id = QStringLiteral("%1|INBOX|%2").arg(mailbox.id).arg(i + 1);
                message.mailboxId = mailbox.id;
                message.folderId = QStringLiteral("%1|INBOX").arg(mailbox.id);
                message.uid = QString::number(i + 1);
                message.from = QLatin1String(senders[(i + demo.unread) % 4]);
                message.to = mailbox.address;
                message.subject = QLatin1String(subjects[(i + demo.unread) % 8]);
                message.date = base.addSecs(i * 5400 + demo.unread * 900);
                message.flags = i < demo.unread ? QString() : QStringLiteral("\\Seen");
                message.size = 4200 + i * 130;
                vault.state().messages.append(message);
            }
        }

        if (!vault.save(&error)) {
            std::printf("%s\n", qPrintable(error));
            return 1;
        }
        std::printf("demo vault written to %s\n", argv[2]);
        return 0;
    }

    QCoreApplication app(argc, argv);

    TestRunner runner;

    runner.begin("crypto");
    test_crypto(runner);

    runner.begin("vault");
    test_vault(runner);

    runner.begin("mime");
    test_mime(runner);

    runner.begin("importer");
    test_importer(runner);

    runner.begin("imap");
    test_imap(runner);

    std::printf("\n%d checks, %d failures\n", runner.checks, runner.failures);
    return runner.failures == 0 ? 0 : 1;
}

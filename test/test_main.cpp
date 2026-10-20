#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QUuid>
#include <cstdio>

#include "core/Vault.h"
#include "test_harness.h"

void test_crypto(TestRunner& runner);
void test_vault(TestRunner& runner);
void test_mime(TestRunner& runner);
void test_importer(TestRunner& runner);
void test_imap(TestRunner& runner);
void test_icons(TestRunner& runner);

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

        // The demo vault gets a recovery passphrase, so the way back in can
        // be tried out.
        const QString recovery = QStringLiteral("DEMO RECOVERY PASSPHRASE 12345");
        if (!vault.createRecovery(recovery, &error)) {
            std::printf("%s\n", qPrintable(error));
            return 1;
        }
        std::printf("demo vault written to %s\n", argv[2]);
        std::printf("master password: %s\n", argv[3]);
        std::printf("recovery passphrase: %s\n", qPrintable(recovery));
        return 0;
    }

    // Check whether a vault opens with a password.
    if (argc >= 4 && QString::fromLatin1(argv[1]) == QLatin1String("--check-vault")) {
        mmc::Vault vault;
        QString error;
        const bool ok = vault.open(QString::fromLocal8Bit(argv[2]),
                                   QString::fromLocal8Bit(argv[3]), &error);
        std::printf("open: %s %s\n", ok ? "yes" : "no", qPrintable(error));
        std::printf("recovery: %s\n",
                    mmc::Vault::vaultHasRecovery(QString::fromLocal8Bit(argv[2])) ? "yes" : "no");
        if (ok)
            std::printf("mailboxes: %d\n", int(vault.state().mailboxes.size()));
        return ok ? 0 : 1;
    }

    // Show what is inside the header of a vault file.
    if (argc >= 3 && QString::fromLatin1(argv[1]) == QLatin1String("--dump-vault")) {
        QFile file(QString::fromLocal8Bit(argv[2]));
        if (!file.open(QIODevice::ReadOnly)) {
            std::printf("cannot read the file\n");
            return 1;
        }
        QDataStream in(&file);
        in.setVersion(QDataStream::Qt_6_0);
        QByteArray magic;
        quint16 version = 0, flags = 0;
        quint32 iterations = 0;
        QByteArray salt, pwNonce, pwWrap;
        quint16 pwWrapLength = 0;
        quint8 hasRecovery = 0;
        quint32 wrapIterations = 0;
        QByteArray wrapSalt, wrapNonce, wrappedKey;
        quint16 wrapLength = 0;
        QByteArray nonce;
        quint64 cipherLen = 0;
        in >> magic >> version >> flags >> iterations >> salt;
        in >> pwNonce >> pwWrapLength >> pwWrap;
        in >> hasRecovery >> wrapIterations >> wrapSalt >> wrapNonce >> wrapLength >> wrappedKey;
        in >> nonce >> cipherLen;
        std::printf("magic=%s version=%u iterations=%u\n", magic.constData(), version, iterations);
        std::printf("salt=%s\n", salt.toHex().constData());
        std::printf("pwNonce=%s pwWrapLen=%d pwWrap=%s\n", pwNonce.toHex().constData(),
                    int(pwWrap.size()), pwWrap.toHex().constData());
        std::printf("hasRecovery=%d wrapIter=%u wrapSalt=%s wrapNonce=%s wrapLen=%d\n",
                    int(hasRecovery), wrapIterations, wrapSalt.toHex().constData(),
                    wrapNonce.toHex().constData(), int(wrappedKey.size()));
        std::printf("nonce=%s cipherLen=%llu\n", nonce.toHex().constData(),
                    (unsigned long long)cipherLen);
        return 0;
    }

    // Reset the master password of a vault with its recovery passphrase.
    if (argc >= 5 && QString::fromLatin1(argv[1]) == QLatin1String("--reset-vault")) {
        QString error;
        const bool ok = mmc::Vault::resetMasterPassword(QString::fromLocal8Bit(argv[2]),
                                                          QString::fromLocal8Bit(argv[3]),
                                                          QString::fromLocal8Bit(argv[4]), &error);
        std::printf("reset: %s %s\n", ok ? "yes" : "no", qPrintable(error));
        return ok ? 0 : 1;
    }

    QCoreApplication app(argc, argv);
    Q_INIT_RESOURCE(icons);

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

    runner.begin("icons");
    test_icons(runner);

    std::printf("\n%d checks, %d failures\n", runner.checks, runner.failures);
    return runner.failures == 0 ? 0 : 1;
}

#include <QTemporaryDir>

#include "core/Vault.h"
#include "test_harness.h"

using namespace mmc;

void test_vault(TestRunner& runner)
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("test.mmcv"));

    Vault vault;
    QString error;
    CHECK(vault.create(path, "correct horse", &error));

    Mailbox mailbox;
    mailbox.id = QStringLiteral("id-1");
    mailbox.providerId = QStringLiteral("google");
    mailbox.address = QStringLiteral("someone@gmail.com");
    mailbox.displayName = QStringLiteral("Someone");
    mailbox.cooldownDays = 30;
    mailbox.cooldownUntil = QDateTime::currentDateTime().addDays(30);
    vault.state().mailboxes.append(mailbox);

    OAuthToken token;
    token.mailboxId = mailbox.id;
    token.accessToken = QStringLiteral("secret-token");
    token.refreshToken = QStringLiteral("refresh-token");
    vault.state().tokens.append(token);

    CHECK(vault.save(&error));
    vault.close();

    Vault reopened;
    CHECK(reopened.open(path, "correct horse", &error));
    CHECK_EQ(reopened.state().mailboxes.size(), 1);
    CHECK_EQ(reopened.state().mailboxes.first().address, QStringLiteral("someone@gmail.com"));
    CHECK_EQ(reopened.state().mailboxes.first().cooldownDays, 30);
    CHECK_EQ(reopened.state().tokens.size(), 1);
    CHECK_EQ(reopened.state().tokens.first().accessToken, QStringLiteral("secret-token"));

    // The wrong password must not open the vault.
    Vault wrong;
    CHECK(!wrong.open(path, "wrong horse", &error));
    CHECK(!error.isEmpty());

    // A damaged file must not open either.
    QFile file(path);
    CHECK(file.open(QIODevice::ReadWrite));
    QByteArray head = file.read(64);
    head[40] = head[40] ^ 0xFF;
    file.seek(0);
    file.write(head);
    file.close();

    Vault damaged;
    CHECK(!damaged.open(path, "correct horse", &error));

    // Changing the master password.
    Vault other;
    CHECK(other.create(dir.filePath(QStringLiteral("other.mmcv")), "old password", &error));
    CHECK(!other.changeMasterPassword("wrong", "new password", &error));
    CHECK(other.changeMasterPassword("old password", "new password", &error));

    Vault afterChange;
    CHECK(afterChange.open(dir.filePath(QStringLiteral("other.mmcv")), "new password", &error));
    CHECK(!afterChange.open(dir.filePath(QStringLiteral("other.mmcv")), "old password", &error));

    // Removing an account takes its tokens, folders and messages with it.
    VaultState state;
    Mailbox m;
    m.id = QStringLiteral("id-1");
    state.mailboxes.append(m);
    OAuthToken t;
    t.mailboxId = m.id;
    state.tokens.append(t);
    Folder folder;
    folder.id = QStringLiteral("id-1|INBOX");
    folder.mailboxId = m.id;
    state.folders.append(folder);
    Message message;
    message.id = QStringLiteral("id-1|INBOX|1");
    message.mailboxId = m.id;
    state.messages.append(message);

    state.removeMailbox(QStringLiteral("id-1"));
    CHECK_EQ(state.mailboxes.size(), 0);
    CHECK_EQ(state.tokens.size(), 0);
    CHECK_EQ(state.folders.size(), 0);
    CHECK_EQ(state.messages.size(), 0);

    // --- the recovery passphrase ----------------------------------------
    const QString passphrase = Vault::makeRecoveryPassphrase();
    CHECK_EQ(passphrase.size(), 35);   // six groups of five with spaces
    CHECK_EQ(Vault::normalisePassphrase(passphrase.toLower()),
             Vault::normalisePassphrase(passphrase));
    CHECK_EQ(Vault::normalisePassphrase(QStringLiteral("abc def")).size(), 6);

    const QString recoveryPath = dir.filePath(QStringLiteral("recovery.mmcv"));
    Vault recoveryVault;
    CHECK(recoveryVault.create(recoveryPath, "master password", &error));
    CHECK(!recoveryVault.hasRecovery());
    CHECK(!Vault::vaultHasRecovery(recoveryPath));

    Mailbox account;
    account.id = QStringLiteral("id-1");
    account.address = QStringLiteral("someone@gmail.com");
    recoveryVault.state().mailboxes.append(account);
    CHECK(recoveryVault.save(&error));

    CHECK(recoveryVault.createRecovery(passphrase, &error));
    CHECK(recoveryVault.hasRecovery());
    CHECK(Vault::vaultHasRecovery(recoveryPath));

    // The vault still opens with the master password.
    Vault reopened2;
    CHECK(reopened2.open(recoveryPath, "master password", &error));
    CHECK(reopened2.hasRecovery());
    CHECK_EQ(reopened2.state().mailboxes.size(), 1);

    // A wrong recovery passphrase changes nothing.
    CHECK(!Vault::resetMasterPassword(recoveryPath, "wrong passphrase",
                                      "new master password", &error));
    Vault stillOld;
    CHECK(stillOld.open(recoveryPath, "master password", &error));

    // With the recovery passphrase the vault opens and gets a new password.
    CHECK(Vault::resetMasterPassword(recoveryPath, passphrase, "new master password", &error));
    Vault afterReset;
    CHECK(afterReset.open(recoveryPath, "new master password", &error));
    CHECK_EQ(afterReset.state().mailboxes.size(), 1);
    CHECK_EQ(afterReset.state().mailboxes.first().address, QStringLiteral("someone@gmail.com"));
    CHECK(afterReset.hasRecovery());

    // The old master password does not work any more.
    Vault oldPassword;
    CHECK(!oldPassword.open(recoveryPath, "master password", &error));

    // The recovery passphrase still works after a reset.
    CHECK(Vault::resetMasterPassword(recoveryPath, passphrase, "third password", &error));
    Vault third;
    CHECK(third.open(recoveryPath, "third password", &error));
    CHECK_EQ(third.state().mailboxes.size(), 1);

    // Changing the master password keeps the recovery passphrase working.
    CHECK(third.changeMasterPassword("third password", "fourth password", &error));
    CHECK(Vault::resetMasterPassword(recoveryPath, passphrase, "fifth password", &error));
    Vault fifth;
    CHECK(fifth.open(recoveryPath, "fifth password", &error));
    CHECK_EQ(fifth.state().mailboxes.size(), 1);
}

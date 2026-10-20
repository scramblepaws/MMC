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
}

#pragma once

#include "core/Model.h"

#include <QString>
#include <QStringList>

namespace mmc {

// An encrypted vault: a single file that holds every mailbox, token and cached
// message header. The whole file is encrypted with a key derived from the
// master password, so the file on the USB stick is unreadable without it.
//
// The file holds one random master key. That key is wrapped twice: once with
// the master password and once with the recovery passphrase. The mail itself
// is encrypted with the master key, so the master password can be changed
// without touching the mail, and the recovery passphrase keeps working.
class Vault
{
public:
    static constexpr int kKdfIterations = 600000;   // PBKDF2-HMAC-SHA256
    static constexpr quint16 kFormatVersion = 3;
    static const char* kMagic;                      // "MMCVAULT"

    Vault() = default;
    ~Vault();

    static QString defaultVaultDir();
    static QStringList listVaults(const QString& dir = QString());

    bool create(const QString& path, const QString& masterPassword, QString* error);
    bool open(const QString& path, const QString& masterPassword, QString* error);
    void close();

    bool save(QString* error);
    bool changeMasterPassword(const QString& currentPassword,
                               const QString& newPassword,
                               QString* error);

    // A recovery passphrase is a second way in. It wraps the master key of
    // the vault, so the master password can be set again without the accounts
    // being lost. It is as powerful as the master password, so it has to be
    // written down and kept somewhere safe.
    static QString makeRecoveryPassphrase();
    static QString normalisePassphrase(const QString& passphrase);
    bool createRecovery(const QString& passphrase, QString* error);
    bool hasRecovery() const { return m_hasRecovery; }

    // Opens a vault that cannot be opened with the master password, but with
    // the recovery passphrase, and gives it a new master password.
    static bool resetMasterPassword(const QString& path,
                                     const QString& recoveryPassphrase,
                                     const QString& newMasterPassword,
                                     QString* error);
    static bool vaultHasRecovery(const QString& path);

    bool isOpen() const { return !m_path.isEmpty(); }
    QString path() const { return m_path; }
    QString name() const;
    VaultState& state() { return m_state; }
    const VaultState& state() const { return m_state; }

    // Serialisation used by the tests and by save()/open().
    static QByteArray serialise(const VaultState& state);
    static bool deserialise(const QByteArray& blob, VaultState* state);

private:
    struct Header {
        quint16 version = kFormatVersion;
        quint32 iterations = 0;
        QByteArray salt;            // 16, for the master password
        QByteArray pwNonce;         // 12
        QByteArray pwWrap;          // 48, the master key under the password
        bool hasRecovery = false;
        quint32 wrapIterations = 0;
        QByteArray wrapSalt;        // 16, for the recovery passphrase
        QByteArray wrapNonce;       // 12
        QByteArray wrappedKey;      // 48, the master key under the passphrase
        QByteArray nonce;           // 12, for the mail
        QByteArray cipher;          // the encrypted state
        QByteArray tag;             // 16, of the mail
    };

    bool writeFile(QString* error);
    static bool readRaw(const QString& path, Header* header, QString* error);
    static bool readRecoveryBlock(QDataStream& in, Header* header, QString* error);
    static bool writeRaw(const QString& path, const Header& header, QString* error);
    bool wrapMasterKey(const QString& passphrase, Header* header) const;

    QString m_path;
    QByteArray m_key;         // 32 byte master key, the mail is encrypted with it
    QByteArray m_passwordKey; // 32 byte key from the master password
    QByteArray m_salt;        // PBKDF2 salt of the master password
    int m_iterations = kKdfIterations;
    bool m_hasRecovery = false;
    Header m_recovery;        // the recovery block, kept to write it back
    VaultState m_state;
};

} // namespace mmc

#pragma once

#include "core/Model.h"

#include <QString>
#include <QStringList>

namespace mmc {

// An encrypted vault: a single file that holds every mailbox, token and cached
// message header. The whole file is encrypted with a key derived from the
// master password, so the file on the USB stick is unreadable without it.
class Vault
{
public:
    static constexpr int kKdfIterations = 600000;   // PBKDF2-HMAC-SHA256
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

    bool isOpen() const { return !m_path.isEmpty(); }
    QString path() const { return m_path; }
    QString name() const;
    VaultState& state() { return m_state; }
    const VaultState& state() const { return m_state; }

    // Serialisation used by the tests and by save()/open().
    static QByteArray serialise(const VaultState& state);
    static bool deserialise(const QByteArray& blob, VaultState* state);

private:
    bool writeFile(const QString& path, const QByteArray& key, QString* error) const;
    bool readFile(const QString& path, QByteArray* salt, int* iterations,
                  QByteArray* nonce, QByteArray* cipher, QByteArray* tag,
                  QString* error) const;

    QString m_path;
    QByteArray m_key;      // 32 byte AES key
    QByteArray m_salt;     // PBKDF2 salt of the current key
    int m_iterations = kKdfIterations;
    VaultState m_state;
};

} // namespace mmc

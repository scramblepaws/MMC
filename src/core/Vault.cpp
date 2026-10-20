#include "core/Vault.h"

#include "core/Crypto.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace mmc {

const char* Vault::kMagic = "MMCVAULT";

namespace {
constexpr quint16 kOldVersion = 1;
constexpr quint16 kRecoveryVersion = 2;
constexpr int kSaltLength = 16;
constexpr int kNonceLength = 12;
constexpr int kTagLength = 16;
constexpr int kWrapLength = 32 + kTagLength;
constexpr int kWrapIterations = 200000;
const char* kStateMagic = "MMCSTATE";
constexpr quint32 kStateVersion = 1;
const char* kPassphraseAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
} // namespace

QString Vault::defaultVaultDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("MMC"));
}

QStringList Vault::listVaults(const QString& dir)
{
    const QString base = dir.isEmpty() ? defaultVaultDir() : dir;
    QDir d(base);
    if (!d.exists())
        return {};
    QStringList out;
    const QFileInfoList entries = d.entryInfoList({QStringLiteral("*.mmcv")}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries)
        out << fi.absoluteFilePath();
    return out;
}

Vault::~Vault()
{
    close();
}

QString Vault::name() const
{
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).completeBaseName();
}

QByteArray Vault::serialise(const VaultState& state)
{
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << QByteArray(kStateMagic) << quint32(kStateVersion);
    out << state;
    return blob;
}

bool Vault::deserialise(const QByteArray& blob, VaultState* state)
{
    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    QByteArray magic;
    quint32 version = 0;
    in >> magic >> version;
    if (magic != kStateMagic || version != kStateVersion)
        return false;
    in >> *state;
    return in.status() == QDataStream::Ok;
}

bool Vault::create(const QString& path, const QString& masterPassword, QString* error)
{
    close();
    QDir().mkpath(QFileInfo(path).absolutePath());

    m_path = path;
    m_salt = randomBytes(kSaltLength);
    m_iterations = kKdfIterations;
    m_passwordKey = pbkdf2Sha256(masterPassword.toUtf8(), m_salt, m_iterations, 32);
    if (m_passwordKey.isEmpty()) {
        m_path.clear();
        if (error) *error = QObject::tr("Could not derive an encryption key from the master password.");
        return false;
    }
    m_key = randomBytes(32);   // the master key of the vault
    m_state = VaultState();
    return writeFile(error);
}

bool Vault::open(const QString& path, const QString& masterPassword, QString* error)
{
    close();
    Header header;
    if (!readRaw(path, &header, error))
        return false;

    const QByteArray passwordKey =
        pbkdf2Sha256(masterPassword.toUtf8(), header.salt, static_cast<int>(header.iterations), 32);
    if (passwordKey.isEmpty()) {
        if (error) *error = QObject::tr("Could not derive an encryption key from the master password.");
        return false;
    }

    QByteArray masterKey;
    if (header.version >= kFormatVersion) {
        // The master key is wrapped with the key from the password.
        masterKey = aesGcmDecrypt(passwordKey, header.pwNonce,
                                  header.pwWrap.left(32), header.pwWrap.mid(32));
        if (masterKey.isEmpty()) {
            if (error)
                *error = QObject::tr("Wrong master password, or the vault file has been modified.");
            return false;
        }
    } else {
        // An older vault: the key from the password was the master key.
        masterKey = passwordKey;
    }

    const QByteArray plain = aesGcmDecrypt(masterKey, header.nonce, header.cipher, header.tag);
    if (plain.isEmpty() && !header.cipher.isEmpty()) {
        if (error) *error = QObject::tr("Wrong master password, or the vault file has been modified.");
        return false;
    }

    VaultState state;
    if (!deserialise(plain, &state)) {
        if (error) *error = QObject::tr("The vault file is not a valid MMC vault.");
        return false;
    }

    m_path = path;
    m_key = masterKey;
    m_passwordKey = passwordKey;
    m_salt = header.salt;
    m_iterations = static_cast<int>(header.iterations);
    m_hasRecovery = header.hasRecovery;
    m_recovery = header;
    m_state = state;
    return true;
}

void Vault::close()
{
    m_path.clear();
    m_key.clear();
    m_passwordKey.clear();
    m_salt.clear();
    m_hasRecovery = false;
    m_recovery = Header();
    m_state = VaultState();
}

bool Vault::save(QString* error)
{
    if (!isOpen()) {
        if (error) *error = QObject::tr("No vault is open.");
        return false;
    }
    return writeFile(error);
}

bool Vault::writeFile(QString* error)
{
    Header header;
    header.version = kFormatVersion;
    header.iterations = static_cast<quint32>(m_iterations);
    header.salt = m_salt;
    header.pwNonce = randomBytes(kNonceLength);
    header.hasRecovery = m_hasRecovery;
    if (m_hasRecovery) {
        header.wrapIterations = m_recovery.wrapIterations;
        header.wrapSalt = m_recovery.wrapSalt;
        header.wrapNonce = m_recovery.wrapNonce;
        header.wrappedKey = m_recovery.wrappedKey;
    }

    // The master key, wrapped with the key from the master password.
    QByteArray pwTag;
    const QByteArray pwWrap = aesGcmEncrypt(m_passwordKey, header.pwNonce, m_key, &pwTag);
    if (pwWrap.isEmpty()) {
        if (error) *error = QObject::tr("Encryption failed.");
        return false;
    }
    header.pwWrap = pwWrap + pwTag;

    // The mail itself.
    header.nonce = randomBytes(kNonceLength);
    QByteArray tag;
    const QByteArray cipher = aesGcmEncrypt(m_key, header.nonce, serialise(m_state), &tag);
    if (cipher.isEmpty()) {
        if (error) *error = QObject::tr("Encryption failed.");
        return false;
    }
    header.cipher = cipher;
    header.tag = tag;

    return writeRaw(m_path, header, error);
}

bool Vault::writeRaw(const QString& path, const Header& header, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QObject::tr("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);
    out << QByteArray(kMagic) << quint16(kFormatVersion) << quint16(0)
        << header.iterations << header.salt
        << header.pwNonce << quint16(header.pwWrap.size()) << header.pwWrap
        << quint8(header.hasRecovery ? 1 : 0) << header.wrapIterations
        << header.wrapSalt << header.wrapNonce << quint16(header.wrappedKey.size())
        << header.wrappedKey
        << header.nonce << quint64(header.cipher.size());
    out.writeRawData(header.cipher.constData(), header.cipher.size());
    out.writeRawData(header.tag.constData(), header.tag.size());
    if (out.status() != QDataStream::Ok) {
        file.cancelWriting();
        if (error) *error = QObject::tr("Writing the vault failed.");
        return false;
    }
    if (!file.commit()) {
        if (error) *error = QObject::tr("Cannot commit the vault file: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool Vault::readRecoveryBlock(QDataStream& in, Header* header, QString* error)
{
    quint8 hasRecovery = 0;
    quint16 wrapLength = 0;
    in >> hasRecovery >> header->wrapIterations >> header->wrapSalt
       >> header->wrapNonce >> wrapLength >> header->wrappedKey;
    header->hasRecovery = hasRecovery != 0;
    if (header->hasRecovery
        && (header->wrapSalt.size() != kSaltLength
            || header->wrapNonce.size() != kNonceLength
            || header->wrappedKey.size() != kWrapLength)) {
        if (error) *error = QObject::tr("The vault file is damaged (bad recovery block).");
        return false;
    }
    return true;
}

bool Vault::readRaw(const QString& path, Header* header, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QObject::tr("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);

    QByteArray magic;
    quint16 version = 0, flags = 0;
    quint32 iterations = 0;
    QByteArray salt, nonce, cipher;
    quint64 cipherLen = 0;
    in >> magic >> version >> flags >> iterations >> salt;
    if (magic != kMagic || version < kOldVersion || version > kFormatVersion) {
        if (error) *error = QObject::tr("%1 is not an MMC vault file.").arg(path);
        return false;
    }
    if (salt.size() != kSaltLength) {
        if (error) *error = QObject::tr("The vault file is damaged (bad header).");
        return false;
    }

    header->version = version;
    header->iterations = iterations;
    header->salt = salt;

    // The layout grew over time, so every version is read on its own terms.
    if (version >= kFormatVersion) {
        quint16 pwWrapLength = 0;
        in >> header->pwNonce >> pwWrapLength >> header->pwWrap;
        if (header->pwNonce.size() != kNonceLength || header->pwWrap.size() != kWrapLength) {
            if (error) *error = QObject::tr("The vault file is damaged (bad key block).");
            return false;
        }
        if (!readRecoveryBlock(in, header, error))
            return false;
        in >> header->nonce >> cipherLen;
    } else if (version >= kRecoveryVersion) {
        in >> header->nonce >> cipherLen;
        if (!readRecoveryBlock(in, header, error))
            return false;
    } else {
        in >> header->nonce >> cipherLen;
    }

    if (header->nonce.size() != kNonceLength) {
        if (error) *error = QObject::tr("The vault file is damaged (bad header).");
        return false;
    }

    cipher = file.read(static_cast<qint64>(cipherLen));
    const QByteArray tag = file.read(kTagLength);
    if (cipher.size() != static_cast<int>(cipherLen) || tag.size() != kTagLength) {
        if (error) *error = QObject::tr("The vault file is truncated.");
        return false;
    }
    header->cipher = cipher;
    header->tag = tag;
    return true;
}

bool Vault::changeMasterPassword(const QString& currentPassword,
                                 const QString& newPassword,
                                 QString* error)
{
    if (!isOpen()) {
        if (error) *error = QObject::tr("No vault is open.");
        return false;
    }
    // The current password has to be right, otherwise re-deriving would lock
    // the user out of their own data.
    const QByteArray check = pbkdf2Sha256(currentPassword.toUtf8(), m_salt, m_iterations, 32);
    if (check != m_passwordKey) {
        if (error) *error = QObject::tr("The current master password is wrong.");
        return false;
    }
    m_salt = randomBytes(kSaltLength);
    m_passwordKey = pbkdf2Sha256(newPassword.toUtf8(), m_salt, m_iterations, 32);
    if (m_passwordKey.isEmpty()) {
        if (error) *error = QObject::tr("Could not derive an encryption key from the new master password.");
        return false;
    }
    // The master key of the vault does not change, so the recovery
    // passphrase keeps working.
    return writeFile(error);
}

QString Vault::makeRecoveryPassphrase()
{
    const QByteArray bytes = randomBytes(20);
    const char* alphabet = kPassphraseAlphabet;
    QString out;
    quint32 value = 0;
    int bits = 0;
    for (const char byte : bytes) {
        value = (value << 8) | static_cast<unsigned char>(byte);
        bits += 8;
        while (bits >= 5 && out.size() < 30) {
            bits -= 5;
            out += QLatin1Char(alphabet[(value >> bits) & 31]);
        }
        if (out.size() >= 30)
            break;
    }
    // In groups of five, so it can be written down without mistakes.
    QString grouped;
    for (int i = 0; i < out.size(); i += 5)
        grouped += out.mid(i, 5) + QLatin1Char(' ');
    return grouped.trimmed();
}

QString Vault::normalisePassphrase(const QString& passphrase)
{
    QString out;
    for (const QChar ch : passphrase) {
        if (ch.isLetterOrNumber())
            out += ch.toUpper();
    }
    return out;
}

bool Vault::wrapMasterKey(const QString& passphrase, Header* header) const
{
    if (m_key.isEmpty())
        return false;
    const QByteArray salt = randomBytes(kSaltLength);
    const QByteArray nonce = randomBytes(kNonceLength);
    const QByteArray kek = pbkdf2Sha256(normalisePassphrase(passphrase).toUtf8(), salt,
                                         kWrapIterations, 32);
    if (kek.isEmpty())
        return false;
    QByteArray tag;
    const QByteArray wrapped = aesGcmEncrypt(kek, nonce, m_key, &tag);
    if (wrapped.isEmpty())
        return false;
    header->hasRecovery = true;
    header->wrapIterations = kWrapIterations;
    header->wrapSalt = salt;
    header->wrapNonce = nonce;
    header->wrappedKey = wrapped + tag;
    return true;
}

bool Vault::createRecovery(const QString& passphrase, QString* error)
{
    if (!isOpen()) {
        if (error) *error = QObject::tr("No vault is open.");
        return false;
    }
    Header header = m_recovery;
    if (!wrapMasterKey(passphrase, &header)) {
        if (error) *error = QObject::tr("Could not set up the recovery passphrase.");
        return false;
    }
    m_recovery = header;
    m_hasRecovery = true;
    return writeFile(error);
}

bool Vault::vaultHasRecovery(const QString& path)
{
    Header header;
    QString error;
    if (!readRaw(path, &header, &error))
        return false;
    return header.hasRecovery;
}

bool Vault::resetMasterPassword(const QString& path,
                                 const QString& recoveryPassphrase,
                                 const QString& newMasterPassword,
                                 QString* error)
{
    Header header;
    if (!readRaw(path, &header, error))
        return false;
    if (!header.hasRecovery) {
        if (error)
            *error = QObject::tr("This vault has no recovery passphrase, the master password "
                                 "cannot be reset.");
        return false;
    }

    // The recovery passphrase gives the master key of the vault.
    const QByteArray kek = pbkdf2Sha256(normalisePassphrase(recoveryPassphrase).toUtf8(),
                                         header.wrapSalt,
                                         static_cast<int>(header.wrapIterations), 32);
    if (kek.isEmpty()) {
        if (error) *error = QObject::tr("Could not derive a key from the recovery passphrase.");
        return false;
    }
    const QByteArray masterKey = aesGcmDecrypt(kek, header.wrapNonce,
                                               header.wrappedKey.left(32),
                                               header.wrappedKey.mid(32));
    if (masterKey.isEmpty()) {
        if (error) *error = QObject::tr("The recovery passphrase is not right.");
        return false;
    }

    // Check that the vault really opens with it.
    const QByteArray plain = aesGcmDecrypt(masterKey, header.nonce, header.cipher, header.tag);
    if (plain.isEmpty() && !header.cipher.isEmpty()) {
        if (error) *error = QObject::tr("The vault file is damaged.");
        return false;
    }
    VaultState state;
    if (!deserialise(plain, &state)) {
        if (error) *error = QObject::tr("The vault file is not a valid MMC vault.");
        return false;
    }

    // The same master key, a new password around it. The mail does not have
    // to be encrypted again and the recovery passphrase keeps working.
    Header updated = header;
    updated.version = kFormatVersion;
    updated.iterations = static_cast<quint32>(kKdfIterations);
    updated.salt = randomBytes(kSaltLength);
    updated.pwNonce = randomBytes(kNonceLength);

    const QByteArray passwordKey = pbkdf2Sha256(newMasterPassword.toUtf8(), updated.salt,
                                                 kKdfIterations, 32);
    if (passwordKey.isEmpty()) {
        if (error) *error = QObject::tr("Could not derive a key from the new master password.");
        return false;
    }
    QByteArray pwTag;
    const QByteArray pwWrap = aesGcmEncrypt(passwordKey, updated.pwNonce, masterKey, &pwTag);
    if (pwWrap.isEmpty()) {
        if (error) *error = QObject::tr("Encryption failed.");
        return false;
    }
    updated.pwWrap = pwWrap + pwTag;
    updated.hasRecovery = true;

    return writeRaw(path, updated, error);
}

// --- VaultState helpers ---------------------------------------------------

Mailbox* VaultState::mailboxById(const QString& id)
{
    for (Mailbox& m : mailboxes)
        if (m.id == id) return &m;
    return nullptr;
}

const Mailbox* VaultState::mailboxById(const QString& id) const
{
    for (const Mailbox& m : mailboxes)
        if (m.id == id) return &m;
    return nullptr;
}

OAuthToken* VaultState::tokenFor(const QString& mailboxId)
{
    for (OAuthToken& t : tokens)
        if (t.mailboxId == mailboxId) return &t;
    return nullptr;
}

const OAuthToken* VaultState::tokenFor(const QString& mailboxId) const
{
    for (const OAuthToken& t : tokens)
        if (t.mailboxId == mailboxId) return &t;
    return nullptr;
}

Folder* VaultState::folderById(const QString& id)
{
    for (Folder& f : folders)
        if (f.id == id) return &f;
    return nullptr;
}

const Folder* VaultState::folderById(const QString& id) const
{
    for (const Folder& f : folders)
        if (f.id == id) return &f;
    return nullptr;
}

void VaultState::removeMailbox(const QString& id)
{
    mailboxes.removeIf([&](const Mailbox& m) { return m.id == id; });
    tokens.removeIf([&](const OAuthToken& t) { return t.mailboxId == id; });
    folders.removeIf([&](const Folder& f) { return f.mailboxId == id; });
    messages.removeIf([&](const Message& m) { return m.mailboxId == id; });
}

// --- QDataStream operators -------------------------------------------------

QDataStream& operator<<(QDataStream& out, const Mailbox& m)
{
    out << m.id << m.providerId << m.address << m.displayName << m.label
        << m.secret << m.cooldownUntil << m.cooldownDays << m.cooldownNote;
    return out;
}

QDataStream& operator>>(QDataStream& in, Mailbox& m)
{
    in >> m.id >> m.providerId >> m.address >> m.displayName >> m.label
       >> m.secret >> m.cooldownUntil >> m.cooldownDays >> m.cooldownNote;
    return in;
}

QDataStream& operator<<(QDataStream& out, const OAuthToken& t)
{
    out << t.mailboxId << t.accessToken << t.refreshToken << t.expiry << t.scope;
    return out;
}

QDataStream& operator>>(QDataStream& in, OAuthToken& t)
{
    in >> t.mailboxId >> t.accessToken >> t.refreshToken >> t.expiry >> t.scope;
    return in;
}

QDataStream& operator<<(QDataStream& out, const Folder& f)
{
    out << f.id << f.mailboxId << f.name << f.delimiter << f.uidValidity << f.uidNext;
    return out;
}

QDataStream& operator>>(QDataStream& in, Folder& f)
{
    in >> f.id >> f.mailboxId >> f.name >> f.delimiter >> f.uidValidity >> f.uidNext;
    return in;
}

QDataStream& operator<<(QDataStream& out, const Message& m)
{
    out << m.id << m.mailboxId << m.folderId << m.uid << m.messageId
        << m.from << m.to << m.subject << m.date << m.size << m.flags << m.hasAttachment;
    return out;
}

QDataStream& operator>>(QDataStream& in, Message& m)
{
    in >> m.id >> m.mailboxId >> m.folderId >> m.uid >> m.messageId
       >> m.from >> m.to >> m.subject >> m.date >> m.size >> m.flags >> m.hasAttachment;
    return in;
}

QDataStream& operator<<(QDataStream& out, const VaultState& s)
{
    out << s.mailboxes << s.tokens << s.folders << s.messages << s.settings;
    return out;
}

QDataStream& operator>>(QDataStream& in, VaultState& s)
{
    in >> s.mailboxes >> s.tokens >> s.folders >> s.messages >> s.settings;
    return in;
}

} // namespace mmc

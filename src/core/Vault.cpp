#include "core/Vault.h"

#include "core/Crypto.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace mmc {

const char* Vault::kMagic = "MMCVAULT";

namespace {
constexpr quint16 kFormatVersion = 1;
constexpr int kSaltLength = 16;
constexpr int kNonceLength = 12;
constexpr int kTagLength = 16;
const char* kStateMagic = "MMCSTATE";
constexpr quint32 kStateVersion = 1;
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
    m_key = pbkdf2Sha256(masterPassword.toUtf8(), m_salt, m_iterations, 32);
    if (m_key.isEmpty()) {
        m_path.clear();
        if (error) *error = QObject::tr("Could not derive an encryption key from the master password.");
        return false;
    }
    m_state = VaultState();
    return writeFile(m_path, m_key, error);
}

bool Vault::open(const QString& path, const QString& masterPassword, QString* error)
{
    close();
    QByteArray salt, nonce, cipher, tag;
    int iterations = 0;
    if (!readFile(path, &salt, &iterations, &nonce, &cipher, &tag, error))
        return false;

    const QByteArray key = pbkdf2Sha256(masterPassword.toUtf8(), salt, iterations, 32);
    if (key.isEmpty()) {
        if (error) *error = QObject::tr("Could not derive an encryption key from the master password.");
        return false;
    }

    const QByteArray plain = aesGcmDecrypt(key, nonce, cipher, tag);
    if (plain.isEmpty() && !cipher.isEmpty()) {
        if (error) *error = QObject::tr("Wrong master password, or the vault file has been modified.");
        return false;
    }

    VaultState state;
    if (!deserialise(plain, &state)) {
        if (error) *error = QObject::tr("The vault file is not a valid MMC vault.");
        return false;
    }

    m_path = path;
    m_key = key;
    m_salt = salt;
    m_iterations = iterations;
    m_state = state;
    return true;
}

void Vault::close()
{
    m_path.clear();
    m_key.clear();
    m_state = VaultState();
}

bool Vault::save(QString* error)
{
    if (!isOpen()) {
        if (error) *error = QObject::tr("No vault is open.");
        return false;
    }
    return writeFile(m_path, m_key, error);
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
    if (check != m_key) {
        if (error) *error = QObject::tr("The current master password is wrong.");
        return false;
    }
    m_salt = randomBytes(kSaltLength);
    m_key = pbkdf2Sha256(newPassword.toUtf8(), m_salt, m_iterations, 32);
    if (m_key.isEmpty()) {
        if (error) *error = QObject::tr("Could not derive an encryption key from the new master password.");
        return false;
    }
    return writeFile(m_path, m_key, error);
}

bool Vault::writeFile(const QString& path, const QByteArray& key, QString* error) const
{
    const QByteArray plain = serialise(m_state);
    const QByteArray nonce = randomBytes(kNonceLength);
    QByteArray tag;
    const QByteArray cipher = aesGcmEncrypt(key, nonce, plain, &tag);
    if (cipher.isEmpty() && !plain.isEmpty()) {
        if (error) *error = QObject::tr("Encryption failed.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QObject::tr("Cannot write %1: %2").arg(path, file.errorString());
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);
    out << QByteArray(kMagic) << quint16(kFormatVersion) << quint16(0)
        << quint32(m_iterations) << m_salt << nonce << quint64(cipher.size());
    out.writeRawData(cipher.constData(), cipher.size());
    out.writeRawData(tag.constData(), tag.size());
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

bool Vault::readFile(const QString& path,
                     QByteArray* salt, int* iterations,
                     QByteArray* nonce, QByteArray* cipher, QByteArray* tag,
                     QString* error) const
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
    quint32 storedIterations = 0;
    QByteArray storedSalt, n, c;
    quint64 cipherLen = 0;
    in >> magic >> version >> flags >> storedIterations >> storedSalt >> n >> cipherLen;
    if (magic != kMagic || version != kFormatVersion) {
        if (error) *error = QObject::tr("%1 is not an MMC vault file.").arg(path);
        return false;
    }
    if (storedSalt.size() != kSaltLength || n.size() != kNonceLength) {
        if (error) *error = QObject::tr("The vault file is damaged (bad header).");
        return false;
    }
    c = file.read(static_cast<qint64>(cipherLen));
    QByteArray t = file.read(kTagLength);
    if (c.size() != static_cast<int>(cipherLen) || t.size() != kTagLength) {
        if (error) *error = QObject::tr("The vault file is truncated.");
        return false;
    }

    *salt = storedSalt;
    *iterations = static_cast<int>(storedIterations);
    *nonce = n;
    *cipher = c;
    *tag = t;
    return true;
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

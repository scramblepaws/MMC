#include "core/Crypto.h"

#include <windows.h>
#include <bcrypt.h>

namespace mmc {
namespace {

constexpr int kKeyLength = 32;   // AES-256
constexpr int kNonceLength = 12; // GCM standard nonce
constexpr int kTagLength = 16;   // GCM standard tag

struct AlgHandle {
    BCRYPT_ALG_HANDLE h = nullptr;
    ~AlgHandle() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

} // namespace

QByteArray randomBytes(int count)
{
    QByteArray out(count, Qt::Uninitialized);
    if (count <= 0)
        return out;
    const NTSTATUS st = BCryptGenRandom(nullptr,
                                        reinterpret_cast<PUCHAR>(out.data()),
                                        static_cast<ULONG>(count),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (st < 0)
        out.clear();
    return out;
}

QByteArray pbkdf2Sha256(const QByteArray& password,
                        const QByteArray& salt,
                        int iterations,
                        int keyLength)
{
    if (iterations <= 0 || keyLength <= 0)
        return {};

    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
        return {};

    QByteArray out(keyLength, Qt::Uninitialized);
    // BCrypt wants a non null pointer even for an empty password.
    const char* pwd = password.constData();
    const NTSTATUS st = BCryptDeriveKeyPBKDF2(alg.h,
                                              reinterpret_cast<PUCHAR>(const_cast<char*>(pwd)),
                                              static_cast<ULONG>(password.size()),
                                              reinterpret_cast<PUCHAR>(const_cast<char*>(salt.constData())),
                                              static_cast<ULONG>(salt.size()),
                                              static_cast<ULONG>(iterations),
                                              reinterpret_cast<PUCHAR>(out.data()),
                                              static_cast<ULONG>(keyLength),
                                              0);
    if (st < 0)
        return {};
    return out;
}

namespace {

struct KeyHandle {
    BCRYPT_KEY_HANDLE h = nullptr;
    ~KeyHandle() { if (h) BCryptDestroyKey(h); }
};

bool makeKey(BCRYPT_KEY_HANDLE* out, const QByteArray& key)
{
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0)
        return false;
    static const wchar_t kMode[] = BCRYPT_CHAIN_MODE_GCM;
    if (BCryptSetProperty(alg.h, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(kMode)),
                          static_cast<ULONG>(sizeof(kMode)), 0) < 0)
        return false;
    if (BCryptGenerateSymmetricKey(alg.h, out, nullptr, 0,
                                   reinterpret_cast<PUCHAR>(const_cast<char*>(key.constData())),
                                   static_cast<ULONG>(key.size()), 0) < 0)
        return false;
    return true;
}

} // namespace

QByteArray aesGcmEncrypt(const QByteArray& key,
                          const QByteArray& nonce,
                          const QByteArray& plaintext,
                          QByteArray* tagOut)
{
    if (key.size() != kKeyLength || nonce.size() != kNonceLength)
        return {};

    KeyHandle keyHandle;
    if (!makeKey(&keyHandle.h, key))
        return {};

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.dwInfoVersion = BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION;
    info.pbNonce = reinterpret_cast<PUCHAR>(const_cast<char*>(nonce.constData()));
    info.cbNonce = static_cast<ULONG>(nonce.size());
    QByteArray tag(kTagLength, Qt::Uninitialized);
    info.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    info.cbTag = static_cast<ULONG>(tag.size());

    QByteArray out(plaintext.size() + kTagLength, Qt::Uninitialized);
    DWORD written = 0;
    const NTSTATUS st = BCryptEncrypt(keyHandle.h,
                                      reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.constData())),
                                      static_cast<ULONG>(plaintext.size()),
                                      &info,
                                      nullptr, 0,
                                      reinterpret_cast<PUCHAR>(out.data()),
                                      static_cast<ULONG>(plaintext.size()),
                                      &written,
                                      0);
    if (st < 0)
        return {};
    out.resize(static_cast<int>(written));
    if (tagOut)
        *tagOut = tag;
    return out;
}

QByteArray aesGcmDecrypt(const QByteArray& key,
                          const QByteArray& nonce,
                          const QByteArray& ciphertext,
                          const QByteArray& tag)
{
    if (key.size() != kKeyLength || nonce.size() != kNonceLength || tag.size() != kTagLength)
        return {};

    KeyHandle keyHandle;
    if (!makeKey(&keyHandle.h, key))
        return {};

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.dwInfoVersion = BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION;
    info.pbNonce = reinterpret_cast<PUCHAR>(const_cast<char*>(nonce.constData()));
    info.cbNonce = static_cast<ULONG>(nonce.size());
    info.pbTag = reinterpret_cast<PUCHAR>(const_cast<char*>(tag.constData()));
    info.cbTag = static_cast<ULONG>(tag.size());

    QByteArray out(ciphertext.size(), Qt::Uninitialized);
    DWORD written = 0;
    const NTSTATUS st = BCryptDecrypt(keyHandle.h,
                                      reinterpret_cast<PUCHAR>(const_cast<char*>(ciphertext.constData())),
                                      static_cast<ULONG>(ciphertext.size()),
                                      &info,
                                      nullptr, 0,
                                      reinterpret_cast<PUCHAR>(out.data()),
                                      static_cast<ULONG>(ciphertext.size()),
                                      &written,
                                      0);
    if (st < 0)
        return {};
    out.resize(static_cast<int>(written));
    return out;
}

} // namespace mmc

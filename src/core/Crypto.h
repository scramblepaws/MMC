#pragma once

#include <QByteArray>

namespace mmc {

// Windows CNG (bcrypt.dll) primitives. No external crypto dependency.
//
// - AES-256-GCM for the vault payload (authenticated, so a wrong master
//   password or a tampered file is detected instead of silently decrypted).
// - PBKDF2-HMAC-SHA256 to turn the master password into a 256 bit key.
QByteArray randomBytes(int count);

QByteArray pbkdf2Sha256(const QByteArray& password,
                        const QByteArray& salt,
                        int iterations,
                        int keyLength);

// Returns an empty array on failure.
QByteArray aesGcmEncrypt(const QByteArray& key,
                          const QByteArray& nonce,
                          const QByteArray& plaintext,
                          QByteArray* tagOut);

QByteArray aesGcmDecrypt(const QByteArray& key,
                          const QByteArray& nonce,
                          const QByteArray& ciphertext,
                          const QByteArray& tag);

} // namespace mmc

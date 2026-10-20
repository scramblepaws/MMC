#include "core/Crypto.h"
#include "test_harness.h"

using namespace mmc;

void test_crypto(TestRunner& runner)
{
    const QByteArray a = randomBytes(32);
    const QByteArray b = randomBytes(32);
    CHECK_EQ(a.size(), 32);
    CHECK(a != b);

    const QByteArray salt = QByteArrayLiteral("0123456789abcdef");
    const QByteArray first = pbkdf2Sha256("hunter2", salt, 1000, 32);
    const QByteArray second = pbkdf2Sha256("hunter2", salt, 1000, 32);
    const QByteArray other = pbkdf2Sha256("hunter3", salt, 1000, 32);
    CHECK_EQ(first.size(), 32);
    CHECK(first == second);
    CHECK(first != other);

    const QByteArray key = randomBytes(32);
    const QByteArray nonce = randomBytes(12);
    const QByteArray plain = QByteArrayLiteral("the quick brown fox jumps over the lazy dog");

    QByteArray tag;
    const QByteArray cipher = aesGcmEncrypt(key, nonce, plain, &tag);
    CHECK_EQ(cipher.size(), plain.size());
    CHECK_EQ(tag.size(), 16);
    CHECK(cipher != plain);
    CHECK(aesGcmDecrypt(key, nonce, cipher, tag) == plain);

    // A changed byte must not decrypt.
    QByteArray damaged = cipher;
    damaged[0] = damaged[0] ^ 0x01;
    CHECK(aesGcmDecrypt(key, nonce, damaged, tag).isEmpty());

    // The wrong key must not decrypt.
    CHECK(aesGcmDecrypt(randomBytes(32), nonce, cipher, tag).isEmpty());

    // An empty payload round trips too.
    QByteArray emptyTag;
    const QByteArray emptyCipher = aesGcmEncrypt(key, nonce, QByteArray(), &emptyTag);
    CHECK(aesGcmDecrypt(key, nonce, emptyCipher, emptyTag).isEmpty());
}

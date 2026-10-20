# Windows CNG for the crypto, no SQLCipher

The vault is encrypted with the crypto in Windows itself, bcrypt.dll, which
gives AES-256-GCM and PBKDF2 without a third party library. SQLCipher was
the other option and was dropped because it has no ready made Windows
package for a MinGW build and building it from source needs the autotools
flow.

Consequence: MMC is Windows only for the crypto part. The rest of the code
is portable, and a Unix build would swap the four functions in
`src/core/Crypto.cpp` for a library of the same shape.

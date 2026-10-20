# The vault is one encrypted file

The accounts of a user live in a single file that is encrypted as a whole
with AES-256-GCM, keyed by PBKDF2-HMAC-SHA256 over the master password. We
chose this over storing secrets column by column, because the user carries
the file on a USB stick and wants it to be unreadable for anyone who finds
it, and over SQLCipher, because the whole file is one blob we can write and
read without a database library.

Consequence: the vault holds headers and tokens, not mail bodies. Bodies are
fetched on demand and kept in memory only, so the file stays small and the
master password is the only thing standing between a lost USB stick and the
accounts on it.

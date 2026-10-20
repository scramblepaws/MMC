# MMC

A desktop mail client for a lot of accounts at once. One window, many
mailboxes, one inbox.

MMC was built for the situation where a hundred Steam accounts each have
their own email address and signing in to each of them one by one is a
chore. It keeps the accounts in an encrypted vault, shows the mail of every
account in one list, and remembers that a Steam account is on cooldown and
its details must not be changed yet.

- Windows first, portable build available
- Qt 6, C++20, no runtime dependencies beyond Qt itself
- Google (Gmail), Microsoft (Outlook) and Yandex Mail, over IMAP with OAuth2
- Read, search, attachments, write and send
- The accounts live in one encrypted file you can carry on a USB stick

## Build

Needs Qt 6.9 (mingw_64 kit), CMake and a MinGW compiler.

```powershell
.\tools\build.ps1
```

The binary lands in `build\bin\MMC.exe`. The tests run with:

```powershell
cd build
ctest --output-on-failure
```

To make a portable build that runs from a USB stick:

```powershell
.\tools\package_portable.ps1 -OutputDir D:\MMC
```

## Getting started

1. Start MMC and create a vault. Pick a master password and remember it:
   the vault cannot be opened without it and nobody can reset it for you.
2. Open **Settings** and fill in the client id and client secret of the
   provider you want to use. `docs/PROVIDER-SETUP.md` explains how to get
   them, it takes about ten minutes per provider.
3. **Add** an account, then press **Sign in with the browser** to connect it.
4. Press **Refresh all** to fetch the mail of every account.

A vault is a single file. By default it lives in `Documents\MMC`. Copy it to
an encrypted USB stick, or keep several vaults (home, travel, work) and pick
one at startup.

## Importing accounts

Accounts can be added one by one, or imported from a text file with one
account per line:

```
address,provider,display name,app password
steam1@gmail.com,google,Steam 1,
steam2@outlook.com,microsoft,Steam 2,
```

The provider is guessed from the address when it is left out, and the app
password can be left empty for accounts that sign in with the browser.

## Where the data is

- The vault file is encrypted with AES-256-GCM, the key comes from the master
  password through PBKDF2-HMAC-SHA256. The file is useless without it.
- The vault is never written to the repository and is in `.gitignore`.
- Mail bodies are fetched on demand and are not stored in the vault, only the
  headers are, so the file stays small.
- Nothing is sent anywhere except to the mail provider you configured.

## What the first version does not do

- No background refresh. Mail arrives when you press Refresh or Refresh all.
  That also means no notifications and no unread count that updates by
  itself.
- No reading of two factor codes out of the mail. The mail is shown, you read
  the code yourself.
- Html mail is shown with the simple renderer of Qt, so a mail that leans on
  css looks plainer than it should. **Open webmail** hands the account to the
  browser for that.
- Search looks in the mail that was loaded, not on the server.

## Layout

```
src/core    crypto, vault file, model, import
src/net     IMAP, SMTP, OAuth2, MIME, provider list
src/ui      windows and dialogs
test        the checks, run with ctest
```

`CONTEXT.md` holds the vocabulary of the project, `docs/adr` the decisions
that were worth writing down.

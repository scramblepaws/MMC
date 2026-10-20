# MMC

MMC is a mail client that keeps many accounts in one place. It exists so that
a person with a hundred accounts does not have to sign in to each mail
provider one at a time.

## Language

**Vault**:
The encrypted file that holds every account, token and cached message
header. It is the unit that travels, copies and backups.
_Avoid_: Profile, database, save file

**Mailbox**:
One email account that MMC can read from and send with. A mailbox belongs to
one provider and may carry a cooldown note.
_Avoid_: Account, Profile, Mailbox account

**Provider**:
The mail service behind a mailbox, for example Gmail or Yandex Mail. A
provider decides where the mail lives and how to log in.
_Avoid_: Server, Service, Backend

**Master password**:
The password that unlocks a vault. It is never stored and cannot be
recovered.
_Avoid_: Key, Passphrase

**Unified Inbox**:
The single list that shows the mail of every mailbox at once, with the
mailbox each message came from on it.
_Avoid_: Combined view, All mail

**Badge**:
The short tag on a message that says which mailbox it belongs to.
_Avoid_: Tag, Label, Chip

**Cooldown Note**:
A note on a mailbox saying that the account behind it must not be changed
until a given date. It exists because some providers lock an account when
its details are changed too often.
_Avoid_: Timer, Lock, Freeze

**Refresh**:
A single fetch of the newest mail of one or all mailboxes. MMC never
refreshes on its own.
_Avoid_: Sync, Poll, Update

**Body Cache**:
The mail bodies that were downloaded and are still in memory. Bodies are not
written to the vault.
_Avoid_: Offline store, Message store

**Webmail Escape Hatch**:
The button that opens the provider's own web mail in the browser, for the
things a mail client should not do.
_Avoid_: Fallback, Web view

**Import Batch**:
A set of accounts read from a text file in one go.
_Avoid_: Bulk import, CSV import

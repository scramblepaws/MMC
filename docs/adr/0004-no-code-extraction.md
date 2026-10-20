# No code extraction in this version

MMC does not look for two factor codes in the mail and does not show them in
a panel. The first version reads and shows mail and nothing else.

Reason: the user asked for a plain mail client, and every code extractor
guesses wrong sooner or later, which is worse than reading the mail. The
unified inbox already removes the hunting through many webmail logins, and
the Steam Guard codes come from the Steam side, not from the mail.

Consequence: if code extraction comes back, it belongs as a small, separate
layer that looks at a message and offers what it found, with the raw message
always one click away.

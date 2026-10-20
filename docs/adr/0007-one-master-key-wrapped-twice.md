# The vault has one master key, wrapped twice

The vault file holds one random master key. The mail is encrypted with that
key, and the key itself is stored wrapped: once with a key derived from the
master password, and once with a key derived from the recovery passphrase.

The first version encrypted the mail directly with the key from the master
password. That made two things impossible: changing the master password
meant encrypting everything again, and a recovery passphrase could only wrap
the password derived key, so it broke as soon as the password was changed.

Consequence: the master password can be changed at any time, it only wraps
the master key again, and the recovery passphrase keeps working. A vault
without a recovery passphrase cannot be reset, because the key is only
reachable through the master password, and that is the point of encrypting
it.

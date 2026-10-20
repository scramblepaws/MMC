# The IMAP and SMTP clients are written by hand

MMC talks IMAP and SMTP itself over QSslSocket instead of using a mail
library. There is no maintained C++ IMAP library that covers what we need
without dragging in half of a mail client, and the part of IMAP we use
(CAPABILITY, AUTHENTICATE, LIST, SELECT, STATUS, SEARCH, FETCH, STORE) is
small and testable against a fake server.

Consequence: the client only speaks the subset of IMAP it needs. It cannot
use IDLE, so there is no push, and the server has to be selected before a
FETCH. Both limits are in the code, marked as such.

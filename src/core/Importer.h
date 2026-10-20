#pragma once

#include "core/Model.h"

#include <QString>
#include <QVector>

namespace mmc {

struct ImportResult {
    QVector<Mailbox> mailboxes;
    QStringList errors;
    int skipped = 0;
};

// Reads a delimited text file (comma, semicolon or tab separated) with one
// mailbox per line. Columns, in any order after the address:
//   address, provider, display name, app password
// Only the address is required. The provider is guessed from the domain when
// it is missing.
ImportResult importMailboxesFromText(const QByteArray& text);

} // namespace mmc

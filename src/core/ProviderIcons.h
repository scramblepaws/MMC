#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

namespace mmc {

// The logo of the mail provider, taken from the provider itself. Empty when
// there is no logo for the provider.
QPixmap providerPixmap(const QString& providerId, int size = 0);
QIcon providerIcon(const QString& providerId, int size = 0);

} // namespace mmc

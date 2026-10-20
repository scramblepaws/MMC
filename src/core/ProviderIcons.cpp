#include "core/ProviderIcons.h"

#include <QFile>
#include <QHash>

namespace mmc {
namespace {

// The file name of the logo of a provider.
QString iconNameFor(const QString& providerId)
{
    if (providerId == QLatin1String("google"))
        return QStringLiteral("gmail");
    if (providerId == QLatin1String("microsoft"))
        return QStringLiteral("outlook");
    return providerId;
}

} // namespace

QPixmap providerPixmap(const QString& providerId, int size)
{
    static QHash<QString, QPixmap> cache;
    auto it = cache.constFind(providerId);
    if (it == cache.constEnd()) {
        QPixmap pixmap;
        const QString path = QStringLiteral(":/icons/%1.png").arg(iconNameFor(providerId));
        if (QFile::exists(path))
            pixmap.load(path);
        it = cache.insert(providerId, pixmap);
    }
    QPixmap pixmap = it.value();
    if (pixmap.isNull())
        return pixmap;
    if (size > 0 && !pixmap.size().isEmpty())
        return pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return pixmap;
}

QIcon providerIcon(const QString& providerId, int size)
{
    const QPixmap pixmap = providerPixmap(providerId, size);
    if (pixmap.isNull())
        return {};
    return QIcon(pixmap);
}

} // namespace mmc

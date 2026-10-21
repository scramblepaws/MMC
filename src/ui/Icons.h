#pragma once

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QStringList>

namespace mmc {

// The outline icons of MMC. They are drawn by MMC itself, so they can be
// tinted and scaled to any size.
class Icons
{
public:
    // An empty colour draws the icon in the colour of the text.
    static QIcon icon(const QString& name, const QColor& colour = QColor(), int size = 18);
    static QPixmap pixmap(const QString& name, const QColor& colour, int size = 18);
    // The same drawing on a picture, for the places that have no window system.
    static QImage image(const QString& name, const QColor& colour, int size = 18);
    static bool has(const QString& name);
    static QStringList names();
};

} // namespace mmc

#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QApplication;

namespace mmc {

// The look of MMC: the colours, the spacing, the corners and the type. Every
// part of the program takes its values from here, nothing is made up on the
// spot.
class Theme
{
public:
    // Sets the style, the font and the colours of the program.
    static void apply(QApplication& app);

    // Makes the window frame of Windows dark, so the title bar does not
    // stand out in a light colour.
    static void makeFrameDark(QWidget* widget);

    // --- colours ---------------------------------------------------------
    static QColor background();
    static QColor surface();
    static QColor surfaceSecondary();
    static QColor surfaceHover();
    static QColor surfaceActive();
    static QColor border();
    static QColor textPrimary();
    static QColor textSecondary();
    static QColor textMuted();
    static QColor accent();
    static QColor accentHover();
    static QColor accentSubtle();
    static QColor success();
    static QColor warning();
    static QColor error();
    static QColor information();

    // --- spacing ---------------------------------------------------------
    static int space(int step);   // 4, 8, 12, 16, 24, 32

    // --- corners ---------------------------------------------------------
    static int radiusPanel();     // 12
    static int radiusButton();    // 8
    static int radiusInput();     // 8
    static int radiusRow();       // 8
    static int radiusDialog();    // 12
    static int radiusBadge();     // 999

    // --- type ------------------------------------------------------------
    static QString fontFamily();
    static QFont font(int size, QFont::Weight weight = QFont::Normal);

    // The stylesheet of the program.
    static QString styleSheet();
};

} // namespace mmc

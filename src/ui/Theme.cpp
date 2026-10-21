#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace mmc {
namespace {

const QColor kBackground(0x0D, 0x0F, 0x14);
const QColor kSurface(0x14, 0x17, 0x1E);
const QColor kSurfaceSecondary(0x1A, 0x1E, 0x27);
const QColor kSurfaceHover(0x24, 0x29, 0x36);
const QColor kSurfaceActive(0x29, 0x24, 0x3D);
const QColor kBorder(0x2B, 0x2F, 0x3A);
const QColor kTextPrimary(0xF2, 0xF3, 0xF7);
const QColor kTextSecondary(0xA0, 0xA6, 0xB7);
const QColor kTextMuted(0x71, 0x79, 0x8B);
const QColor kAccent(0x87, 0x4B, 0xFA);
const QColor kAccentHover(0x9C, 0x69, 0xFF);
const QColor kAccentSubtle(0x34, 0x26, 0x4F);
const QColor kSuccess(0x43, 0xC9, 0x97);
const QColor kWarning(0xF2, 0xB6, 0x5D);
const QColor kError(0xEF, 0x66, 0x75);
const QColor kInformation(0x6D, 0xA9, 0xFF);

QString colour(const QColor& c)
{
    return c.name(QColor::HexRgb);
}

// Loads the font of the program, the one that is shipped with it. When it is
// not there the font of the system is used.
void loadFont()
{
    static const QStringList files = {
        QStringLiteral(":/fonts/Inter-Variable.ttf"),
    };
    for (const QString& file : files) {
        if (QFile::exists(file))
            QFontDatabase::addApplicationFont(file);
    }
}

} // namespace

QColor Theme::background() { return kBackground; }
QColor Theme::surface() { return kSurface; }
QColor Theme::surfaceSecondary() { return kSurfaceSecondary; }
QColor Theme::surfaceHover() { return kSurfaceHover; }
QColor Theme::surfaceActive() { return kSurfaceActive; }
QColor Theme::border() { return kBorder; }
QColor Theme::textPrimary() { return kTextPrimary; }
QColor Theme::textSecondary() { return kTextSecondary; }
QColor Theme::textMuted() { return kTextMuted; }
QColor Theme::accent() { return kAccent; }
QColor Theme::accentHover() { return kAccentHover; }
QColor Theme::accentSubtle() { return kAccentSubtle; }
QColor Theme::success() { return kSuccess; }
QColor Theme::warning() { return kWarning; }
QColor Theme::error() { return kError; }
QColor Theme::information() { return kInformation; }

int Theme::space(int step)
{
    return step * 4;
}

int Theme::radiusPanel() { return 12; }
int Theme::radiusButton() { return 8; }
int Theme::radiusInput() { return 8; }
int Theme::radiusRow() { return 8; }
int Theme::radiusDialog() { return 12; }
int Theme::radiusBadge() { return 999; }

QString Theme::fontFamily()
{
    if (QFontDatabase::families().contains(QStringLiteral("Inter")))
        return QStringLiteral("Inter");
    if (QFontDatabase::families().contains(QStringLiteral("Segoe UI Variable Text")))
        return QStringLiteral("Segoe UI Variable Text");
    return QStringLiteral("Segoe UI");
}

QFont Theme::font(int size, QFont::Weight weight)
{
    QFont f(fontFamily());
    f.setPixelSize(size);
    f.setWeight(weight);
    f.setHintingPreference(QFont::PreferFullHinting);

    // The font that is shipped is a variable one, so the weight and the size
    // are set on its own axes and every weight looks right.
    if (const auto weightTag = QFont::Tag::fromString(QAnyStringView(u"wght")))
        f.setVariableAxis(*weightTag, static_cast<float>(weight));
    if (const auto sizeTag = QFont::Tag::fromString(QAnyStringView(u"opsz")))
        f.setVariableAxis(*sizeTag, static_cast<float>(qBound(14, size, 32)));
    return f;
}

void Theme::apply(QApplication& app)
{
    loadFont();

    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    palette.setColor(QPalette::Window, kBackground);
    palette.setColor(QPalette::WindowText, kTextPrimary);
    palette.setColor(QPalette::Base, kSurface);
    palette.setColor(QPalette::AlternateBase, kSurfaceSecondary);
    palette.setColor(QPalette::Text, kTextPrimary);
    palette.setColor(QPalette::Button, kSurfaceSecondary);
    palette.setColor(QPalette::ButtonText, kTextPrimary);
    palette.setColor(QPalette::Highlight, kAccent);
    palette.setColor(QPalette::HighlightedText, kTextPrimary);
    palette.setColor(QPalette::ToolTipBase, kSurfaceSecondary);
    palette.setColor(QPalette::ToolTipText, kTextPrimary);
    palette.setColor(QPalette::Link, kAccentHover);
    palette.setColor(QPalette::Disabled, QPalette::Text, kTextMuted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, kTextMuted);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, kTextMuted);
    app.setPalette(palette);

    QFont base = font(13, QFont::Normal);
    app.setFont(base);
    app.setStyleSheet(styleSheet());
}

void Theme::makeFrameDark(QWidget* widget)
{
#ifdef Q_OS_WIN
    // The window of Windows is drawn by the system, the dark mode of it is
    // asked for with this attribute.
    const HWND handle = reinterpret_cast<HWND>(widget->winId());
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(handle, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
#endif
}

QString Theme::styleSheet()
{
    const QString accent = colour(kAccent);
    const QString accentHover = colour(kAccentHover);
    const QString accentSubtle = colour(kAccentSubtle);
    const QString surface = colour(kSurface);
    const QString surfaceSecondary = colour(kSurfaceSecondary);
    const QString surfaceHover = colour(kSurfaceHover);
    const QString border = colour(kBorder);
    const QString text = colour(kTextPrimary);
    const QString textSecondary = colour(kTextSecondary);
    const QString textMuted = colour(kTextMuted);
    const QString background = colour(kBackground);
    const QString error = colour(kError);

    return QStringLiteral(R"(
QWidget {
    color: %1;
    font-size: 13px;
}
QToolTip {
    background: %2;
    color: %1;
    border: 1px solid %3;
    border-radius: 6px;
    padding: 5px 8px;
}

/* Buttons ------------------------------------------------------------- */
QPushButton {
    background: %2;
    color: %1;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 7px 14px;
    font-weight: 500;
}
QPushButton:hover { background: %4; border-color: %5; }
QPushButton:pressed { background: %6; }
QPushButton:disabled { color: %7; border-color: %3; background: %2; }
QPushButton:focus { outline: none; border-color: %8; }

QPushButton[accent="true"] {
    background: %8;
    color: #FFFFFF;
    border: 1px solid %8;
    font-weight: 600;
}
QPushButton[accent="true"]:hover { background: %9; border-color: %9; }
QPushButton[accent="true"]:pressed { background: %8; }
QPushButton[accent="true"]:disabled { background: %3; border-color: %3; color: %7; }

QPushButton[flat="true"] {
    background: transparent;
    border: none;
    color: %10;
    padding: 6px 10px;
}
QPushButton[flat="true"]:hover { background: %4; color: %1; }

QPushButton[danger="true"] { color: %11; border-color: %11; background: transparent; }
QPushButton[danger="true"]:hover { background: %11; color: #FFFFFF; }

/* Inputs -------------------------------------------------------------- */
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDateTimeEdit {
    background: %2;
    color: %1;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 7px 10px;
    selection-background-color: %8;
    selection-color: #FFFFFF;
}
QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QComboBox:hover { border-color: %5; }
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus {
    border-color: %8;
}
QLineEdit::placeholder, QTextEdit::placeholder, QPlainTextEdit::placeholder { color: %7; }

QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid %10;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: %2;
    border: 1px solid %3;
    border-radius: 10px;
    selection-background-color: %12;
    outline: none;
    padding: 4px;
}

/* Lists and tables ---------------------------------------------------- */
QListWidget, QTreeWidget, QTableWidget, QTableView {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget::item, QTreeWidget::item, QTableWidget::item { border-radius: 8px; }
QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected {
    background: %12;
    color: %1;
}
QListWidget::item:hover, QTreeWidget::item:hover, QTableWidget::item:hover { background: %4; }

QHeaderView::section {
    background: transparent;
    color: %7;
    border: none;
    border-bottom: 1px solid %3;
    padding: 8px 10px;
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
}

/* Scroll bars --------------------------------------------------------- */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: %5; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: %3; border-radius: 4px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: %5; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* Menus and popups ---------------------------------------------------- */
QMenu {
    background: %2;
    border: 1px solid %3;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item {
    padding: 7px 26px 7px 12px;
    border-radius: 6px;
    color: %1;
}
QMenu::item:selected { background: %12; }
QMenu::item:disabled { color: %7; }
QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }

/* Dialogs ------------------------------------------------------------- */
QDialog { background: %13; }
QMessageBox { background: %13; }
QMessageBox QLabel { color: %1; }
QMessageBox QPushButton { min-width: 84px; }

/* Tabs ---------------------------------------------------------------- */
QTabWidget::pane { border: none; }
QTabBar::tab {
    background: transparent;
    color: %10;
    padding: 8px 12px;
    border: none;
    font-weight: 500;
}
QTabBar::tab:selected { color: %1; }
QTabBar::tab:hover { color: %1; }

/* Progress and sliders ------------------------------------------------ */
QProgressBar {
    background: %2;
    border: none;
    border-radius: 3px;
    height: 6px;
    text-align: center;
}
QProgressBar::chunk { background: %8; border-radius: 3px; }

QSlider::groove:horizontal { background: %3; height: 4px; border-radius: 2px; }
QSlider::handle:horizontal {
    background: %8; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;
}
QSlider::sub-page:horizontal { background: %8; border-radius: 2px; }

QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }
QCheckBox, QRadioButton { spacing: 8px; color: %1; }
QCheckBox::indicator {
    border: 1px solid %5; border-radius: 4px; background: %2;
}
QCheckBox::indicator:hover { border-color: %8; }

QGroupBox {
    border: 1px solid %3;
    border-radius: 8px;
    margin-top: 16px;
    padding-top: 12px;
    color: %10;
}
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }

QSplitter::handle { background: transparent; }
QSplitter::handle:horizontal { width: 6px; }
QSplitter::handle:vertical { height: 6px; }
)")
        .arg(text, surfaceSecondary, border, surfaceHover, textMuted, kSurfaceActive.name(),
             textMuted, accent, accentHover, textSecondary, error, accentSubtle,
             kBackground.name());
}

} // namespace mmc

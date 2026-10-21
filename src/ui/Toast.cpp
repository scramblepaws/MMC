#include "ui/Toast.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace mmc {
namespace {

QColor colourFor(Toast::Kind kind)
{
    switch (kind) {
    case Toast::Kind::Success: return Theme::success();
    case Toast::Kind::Warning: return Theme::warning();
    case Toast::Kind::Error: return Theme::error();
    default: return Theme::information();
    }
}

QString iconFor(Toast::Kind kind)
{
    switch (kind) {
    case Toast::Kind::Success: return QStringLiteral("success");
    case Toast::Kind::Warning: return QStringLiteral("alert");
    case Toast::Kind::Error: return QStringLiteral("error");
    default: return QStringLiteral("info");
    }
}

} // namespace

Toast::Toast(Kind kind, const QString& title, const QString& text, QWidget* parent)
    : QWidget(parent), m_kind(kind)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 12, 12, 12);
    layout->setSpacing(12);

    auto* icon = new QLabel(this);
    icon->setPixmap(Icons::pixmap(iconFor(kind), colourFor(kind), 20));
    icon->setFixedWidth(20);
    icon->setAlignment(Qt::AlignTop);
    layout->addWidget(icon);

    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    m_title = new QLabel(title, this);
    m_title->setFont(Theme::font(13, QFont::DemiBold));
    textLayout->addWidget(m_title);

    m_text = new QLabel(text, this);
    m_text->setFont(Theme::font(12));
    m_text->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textSecondary().name()));
    m_text->setWordWrap(true);
    m_text->setMaximumWidth(320);
    textLayout->addWidget(m_text);

    m_actionButton = new QPushButton(this);
    m_actionButton->setProperty("flat", true);
    m_actionButton->setStyleSheet(QStringLiteral("color: %1;").arg(colourFor(kind).name()));
    m_actionButton->setVisible(false);
    textLayout->addWidget(m_actionButton, 0, Qt::AlignLeft);

    layout->addLayout(textLayout, 1);

    auto* close = new QPushButton(this);
    close->setProperty("flat", true);
    close->setIcon(Icons::icon(QStringLiteral("close"), Theme::textMuted(), 14));
    close->setFixedSize(24, 24);
    close->setToolTip(tr("Dismiss"));
    connect(close, &QPushButton::clicked, this, &QWidget::deleteLater);
    layout->addWidget(close, 0, Qt::AlignTop);

    setStyleSheet(QStringLiteral("QWidget { background: %1; border: 1px solid %2; border-radius: 10px; }")
                      .arg(Theme::surfaceSecondary().name(), Theme::border().name()));
}

void Toast::setAction(const QString& label, const std::function<void()>& action)
{
    m_actionButton->setText(label);
    m_actionButton->setVisible(!label.isEmpty());
    if (action) {
        connect(m_actionButton, &QPushButton::clicked, this, [action]() { action(); });
    }
}

void Toast::showFor(int milliseconds)
{
    QTimer::singleShot(milliseconds, this, &QWidget::deleteLater);
}

} // namespace mmc

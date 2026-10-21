#include "ui/EmptyState.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace mmc {

EmptyState::EmptyState(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);
    layout->addStretch(1);

    m_icon = new QLabel(this);
    m_icon->setAlignment(Qt::AlignCenter);
    m_icon->setPixmap(Icons::pixmap(QStringLiteral("mail"), Theme::textMuted(), 40));
    layout->addWidget(m_icon);

    m_title = new QLabel(this);
    QFont titleFont = Theme::font(15, QFont::DemiBold);
    m_title->setFont(titleFont);
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    m_text = new QLabel(this);
    m_text->setFont(Theme::font(13));
    m_text->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textSecondary().name()));
    m_text->setAlignment(Qt::AlignCenter);
    m_text->setWordWrap(true);
    layout->addWidget(m_text);

    m_button = new QPushButton(this);
    m_button->setProperty("accent", true);
    m_button->setVisible(false);
    layout->addWidget(m_button, 0, Qt::AlignCenter);

    layout->addStretch(1);
}

void EmptyState::setTitle(const QString& title)
{
    m_title->setText(title);
    m_title->setVisible(!title.isEmpty());
}

void EmptyState::setText(const QString& text)
{
    m_text->setText(text);
    m_text->setVisible(!text.isEmpty());
}

void EmptyState::setIconName(const QString& name)
{
    m_icon->setPixmap(Icons::pixmap(name, Theme::textMuted(), 40));
}

void EmptyState::setAction(const QString& label, const std::function<void()>& action)
{
    m_button->setText(label);
    m_button->setVisible(!label.isEmpty());
    if (action) {
        connect(m_button, &QPushButton::clicked, this, [action]() { action(); });
    }
}

void EmptyState::setVariant(const QString& variant)
{
    const bool isError = variant == QLatin1String("error");
    m_icon->setPixmap(Icons::pixmap(isError ? QStringLiteral("alert") : QStringLiteral("mail"),
                                    isError ? Theme::error() : Theme::textMuted(), 40));
}

} // namespace mmc

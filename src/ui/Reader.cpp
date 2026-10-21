#include "ui/Reader.h"

#include "core/ProviderIcons.h"
#include "ui/EmptyState.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QPalette>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

namespace mmc {

Reader::Reader(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_pages = new QStackedWidget(this);
    layout->addWidget(m_pages, 1);

    // --- the message ------------------------------------------------------
    m_messagePage = new QWidget(m_pages);
    auto* pageLayout = new QVBoxLayout(m_messagePage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    m_header = new QWidget(m_messagePage);
    buildHeader();
    pageLayout->addWidget(m_header);

    m_body = new QTextBrowser(m_messagePage);
    m_body->setOpenExternalLinks(true);
    m_body->setOpenLinks(false);
    m_body->setFrameShape(QFrame::NoFrame);
    m_body->setStyleSheet(QStringLiteral("QTextBrowser { background: %1; border: none; }")
                               .arg(Theme::surface().name()));
    pageLayout->addWidget(m_body, 1);

    m_attachments = new QListWidget(m_messagePage);
    m_attachments->setMaximumHeight(84);
    m_attachments->setVisible(false);
    m_attachments->setStyleSheet(QStringLiteral("QListWidget { background: %1; border-top: 1px solid %2; }")
                                     .arg(Theme::surfaceSecondary().name(), Theme::border().name()));
    pageLayout->addWidget(m_attachments);
    connect(m_attachments, &QListWidget::itemDoubleClicked, this, [this]() {
        const auto items = m_attachments->selectedItems();
        if (items.isEmpty())
            return;
        const QString path = QFileDialog::getSaveFileName(this, tr("Save attachment"),
                                                          items.first()->text());
        if (path.isEmpty())
            return;
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write(m_lastBody);
    });

    m_pages->addWidget(m_messagePage);

    // --- nothing picked ---------------------------------------------------
    m_emptyPage = new EmptyState(m_pages);
    m_emptyPage->setIconName(QStringLiteral("mail"));
    m_emptyPage->setTitle(tr("No message selected"));
    m_emptyPage->setText(tr("Choose a mail to see it here."));
    m_pages->addWidget(m_emptyPage);

    // --- something went wrong ---------------------------------------------
    m_errorPage = new EmptyState(m_pages);
    m_errorPage->setVariant(QStringLiteral("error"));
    m_errorPage->setIconName(QStringLiteral("alert"));
    m_errorPage->setTitle(tr("Unable to load this message"));
    m_errorPage->setText(tr("We could not get this mail. Check the connection of the account and try again."));
    m_errorPage->setAction(tr("Try again"), [this]() { emit retryRequested(); });
    m_pages->addWidget(m_errorPage);

    m_pages->setCurrentWidget(m_emptyPage);
}

void Reader::buildHeader()
{
    auto* layout = new QVBoxLayout(m_header);
    layout->setContentsMargins(24, 18, 24, 14);
    layout->setSpacing(10);

    m_subject = new QLabel(m_header);
    m_subject->setFont(Theme::font(19, QFont::DemiBold));
    m_subject->setWordWrap(true);
    m_subject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_subject);

    auto* row = new QHBoxLayout();
    row->setSpacing(10);

    m_account = new QLabel(m_header);
    m_account->setFixedSize(32, 32);
    m_account->setScaledContents(true);
    row->addWidget(m_account);

    auto* names = new QVBoxLayout();
    names->setSpacing(1);
    m_sender = new QLabel(m_header);
    m_sender->setFont(Theme::font(13, QFont::DemiBold));
    names->addWidget(m_sender);
    m_recipient = new QLabel(m_header);
    m_recipient->setFont(Theme::font(12));
    m_recipient->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textSecondary().name()));
    names->addWidget(m_recipient);
    m_date = new QLabel(m_header);
    m_date->setFont(Theme::font(11));
    m_date->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textMuted().name()));
    names->addWidget(m_date);
    row->addLayout(names);
    row->addStretch(1);

    auto makeButton = [&](const QString& icon, const QString& tip) {
        auto* button = new QToolButton(m_header);
        button->setAccessibleName(tip);
        button->setIcon(Icons::icon(icon, Theme::textSecondary(), 17));
        button->setIconSize(QSize(17, 17));
        button->setFixedSize(32, 32);
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral("QToolButton { border: none; border-radius: 8px; }"
                                            "QToolButton:hover { background: %1; }")
                                  .arg(Theme::surfaceHover().name()));
        return button;
    };

    auto* reply = makeButton(QStringLiteral("reply"), tr("Reply"));
    connect(reply, &QToolButton::clicked, this, &Reader::replyRequested);
    row->addWidget(reply);

    auto* forward = makeButton(QStringLiteral("forward"), tr("Forward"));
    connect(forward, &QToolButton::clicked, this, &Reader::forwardRequested);
    row->addWidget(forward);

    m_starButton = makeButton(QStringLiteral("star"), tr("Star"));
    connect(m_starButton, &QToolButton::clicked, this, [this]() {
        const bool starred = m_starButton->property("starred").toBool();
        emit starToggled(!starred);
    });
    row->addWidget(m_starButton);

    auto* remove = makeButton(QStringLiteral("trash"), tr("Delete"));
    connect(remove, &QToolButton::clicked, this, &Reader::deleteRequested);
    row->addWidget(remove);

    layout->addLayout(row);

    auto* line = new QFrame(m_header);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("background: %1; max-height: 1px; border: none;")
                             .arg(Theme::border().name()));
    layout->addWidget(line);
}

void Reader::showMessage(const Message& message, const ParsedMessage& parsed)
{
    m_subject->setText(decodeHeaderValue(parsed.header(QStringLiteral("subject"))));

    const QString from = decodeHeaderValue(parsed.header(QStringLiteral("from")));
    m_sender->setText(senderName(from));
    m_sender->setToolTip(from);

    const QString to = decodeHeaderValue(parsed.header(QStringLiteral("to")));
    m_recipient->setText(to.isEmpty() ? tr("To: unknown") : tr("To: %1").arg(to));

    const QString date = decodeHeaderValue(parsed.header(QStringLiteral("date")));
    m_date->setText(date);

    const QString html = parsed.bodyHtml();
    if (!html.isEmpty()) {
        // Html mail keeps its own look, on a light sheet, so a mail that was
        // made for a white background stays readable.
        m_body->setStyleSheet(QStringLiteral("QTextBrowser { background: #FFFFFF; border: none; }"));
        QPalette light = m_body->palette();
        light.setColor(QPalette::Text, QColor(0x1A, 0x1A, 0x1A));
        light.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
        m_body->setPalette(light);
        m_body->setHtml(QStringLiteral("<div style='margin:0'>%1</div>")
                            .arg(sanitizeMailHtml(html, m_remoteImages)));
    } else {
        m_body->setStyleSheet(QStringLiteral("QTextBrowser { background: %1; border: none; }")
                                  .arg(Theme::surface().name()));
        QPalette dark = m_body->palette();
        dark.setColor(QPalette::Text, Theme::textPrimary());
        dark.setColor(QPalette::Base, Theme::surface());
        m_body->setPalette(dark);
        const QString text = parsed.bodyText();
        m_body->setPlainText(text.isEmpty() ? tr("This message has no text.") : text);
    }

    m_attachments->clear();
    const QVector<MimePart> attachments = parsed.attachments();
    for (const MimePart& part : attachments)
        m_attachments->addItem(part.filename.isEmpty() ? tr("attachment") : part.filename);
    m_attachments->setVisible(!attachments.isEmpty());

    m_emptyPage->setVisible(false);
    m_errorPage->setVisible(false);
    m_messagePage->setVisible(true);
}

void Reader::showEmpty()
{
    m_emptyPage->setVisible(true);
    m_errorPage->setVisible(false);
    m_messagePage->setVisible(false);
}

void Reader::showError(const QString& title, const QString& text, const std::function<void()>& retry)
{
    m_errorPage->setVisible(true);
    m_emptyPage->setVisible(false);
    m_messagePage->setVisible(false);
    m_errorPage->setTitle(title);
    m_errorPage->setText(text);
    if (retry)
        m_errorPage->setAction(tr("Try again"), retry);
}

void Reader::setRemoteImagesAllowed(bool allowed)
{
    m_remoteImages = allowed;
}

void Reader::clear()
{
    m_body->clear();
    m_attachments->clear();
}

} // namespace mmc

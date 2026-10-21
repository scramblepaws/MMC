#include "ui/MessageList.h"

#include "core/ProviderIcons.h"
#include "net/Mime.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QMouseEvent>
#include <QVariant>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QApplication>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace mmc {
namespace {

constexpr int kRowHeight = 88;
constexpr int kStarSize = 16;

QString previewOf(const Message& message)
{
    return message.subject.isEmpty() ? QString() : QString();
}

// Draws one mail row: the logo of the provider, the sender, the subject, a
// line of the mail itself, the time and a star.
class MessageDelegate : public QStyledItemDelegate
{
public:
    explicit MessageDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.font = Theme::font(13);

        QWidget* widget = const_cast<QWidget*>(opt.widget);
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const Message message = index.data(Qt::UserRole).value<Message>();
        const Mailbox mailbox = index.data(Qt::UserRole + 1).value<Mailbox>();
        const bool starred = index.data(Qt::UserRole + 2).toBool();
        const bool unread = index.data(Qt::UserRole + 3).toBool();
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hovered = opt.state & QStyle::State_MouseOver;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect rect = opt.rect;
        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::surfaceActive());
            painter->drawRoundedRect(QRectF(rect.adjusted(4, 2, -4, -2)),
                                     Theme::radiusRow(), Theme::radiusRow());
            // A small violet mark on the left of the picked row.
            painter->setBrush(Theme::accent());
            painter->drawRoundedRect(QRectF(rect.left() + 6, rect.top() + 18, 3, rect.height() - 36),
                                     1.5, 1.5);
        } else if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::surfaceHover());
            painter->drawRoundedRect(QRectF(rect.adjusted(4, 2, -4, -2)),
                                     Theme::radiusRow(), Theme::radiusRow());
        }

        int left = rect.left() + 16;
        if (selected || hovered) {
            left += 6;
        }

        // The logo of the provider.
        const int avatarSize = 30;
        const QRect avatar(left, rect.top() + 14, avatarSize, avatarSize);
        const QPixmap logo = providerPixmap(mailbox.providerId, avatarSize);
        if (!logo.isNull()) {
            painter->drawPixmap(avatar, logo);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::surfaceSecondary());
            painter->drawRoundedRect(avatar, 8, 8);
        }
        left = avatar.right() + 12;

        // The time on the right.
        QFont metaFont = Theme::font(11);
        painter->setFont(metaFont);
        const QString date = index.data(Qt::UserRole + 4).toString();
        const int dateWidth = QFontMetrics(metaFont).horizontalAdvance(date) + 8;
        const QRect dateRect(rect.right() - dateWidth - 12, rect.top() + 12, dateWidth, 16);
        painter->setPen(Theme::textMuted());
        painter->drawText(dateRect, Qt::AlignRight | Qt::AlignVCenter, date);

        // The star.
        const QRect starRect(rect.right() - 32, rect.top() + 38, kStarSize, kStarSize);
        painter->setPen(Qt::NoPen);
        if (starred) {
            painter->setBrush(Theme::warning());
            const QPolygonF star = starPoints(starRect);
            painter->drawPolygon(star);
        } else {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(Theme::textMuted(), 1.6));
            painter->drawPolygon(starPoints(starRect));
        }

        const int rightEdge = starRect.left() - 12;
        const int width = rightEdge - left;

        // The sender.
        QFont senderFont = Theme::font(14, unread ? QFont::DemiBold : QFont::Medium);
        painter->setFont(senderFont);
        painter->setPen(unread ? Theme::textPrimary() : Theme::textSecondary());
        QString sender = index.data(Qt::UserRole + 5).toString();
        const QFontMetrics senderMetrics(senderFont);
        const QString senderText = senderMetrics.elidedText(sender, Qt::ElideRight, width);
        QRect senderRect(left, rect.top() + 12, width, 18);
        painter->drawText(senderRect, Qt::AlignLeft | Qt::AlignVCenter, senderText);

        // The violet dot of an unread mail.
        if (unread) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::accent());
            painter->drawEllipse(QRectF(senderRect.left() - 15, senderRect.center().y() - 3.5, 7, 7));
        }

        // The subject.
        QFont subjectFont = Theme::font(13, unread ? QFont::DemiBold : QFont::Normal);
        painter->setFont(subjectFont);
        painter->setPen(unread ? Theme::textPrimary() : Theme::textSecondary());
        const QString subject = index.data(Qt::UserRole + 6).toString();
        QRect subjectRect(left, rect.top() + 32, width, 18);
        painter->drawText(subjectRect, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(subjectFont).elidedText(subject, Qt::ElideRight, width));

        // A line of the mail.
        QFont previewFont = Theme::font(12);
        painter->setFont(previewFont);
        painter->setPen(Theme::textMuted());
        const QString preview = index.data(Qt::UserRole + 7).toString();
        QRect previewRect(left, rect.top() + 52, width, 18);
        painter->drawText(previewRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(previewFont).elidedText(preview, Qt::ElideRight, width));

        painter->restore();
    }

    static QPolygonF starPoints(const QRect& rect)
    {
        QPolygonF points;
        const QPointF centre = rect.center();
        const double outer = rect.width() / 2.0;
        const double inner = outer * 0.45;
        for (int i = 0; i < 10; ++i) {
            const double angle = -M_PI / 2.0 + i * M_PI / 5.0;
            const double radius = (i % 2 == 0) ? outer : inner;
            points << QPointF(centre.x() + std::cos(angle) * radius,
                              centre.y() + std::sin(angle) * radius);
        }
        return points;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(320, kRowHeight);
    }
};

} // namespace

MessageList::MessageList(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- the small toolbar ------------------------------------------------
    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(14, 10, 10, 6);
    toolbar->setSpacing(4);

    m_tabs = new QTabBar(this);
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_tabs->addTab(tr("All"));
    m_tabs->addTab(tr("Unread"));
    m_tabs->addTab(tr("Starred"));
    m_tabs->addTab(tr("Attachments"));
    m_tabs->setCurrentIndex(0);
    m_tabs->setStyleSheet(QStringLiteral("QTabBar::tab { color: %1; padding: 6px 10px; font-size: 12px; }"
                                         "QTabBar::tab:selected { color: %2; border-bottom: 2px solid %2; }"
                                         "QTabBar::tab:hover { color: %2; }")
                            .arg(Theme::textMuted().name(), Theme::accent().name()));
    toolbar->addWidget(m_tabs);
    toolbar->addStretch(1);

    auto makeButton = [&](const QString& icon, const QString& tip, const std::function<void()>& slot) {
        auto* button = new QToolButton(this);
        button->setAccessibleName(tip);
        button->setIcon(Icons::icon(icon, Theme::textSecondary(), 16));
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(30, 30);
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral("QToolButton { border: none; border-radius: 6px; }"
                                            "QToolButton:hover { background: %1; }")
                                  .arg(Theme::surfaceHover().name()));
        connect(button, &QToolButton::clicked, this, slot);
        return button;
    };

    m_refreshButton = makeButton(QStringLiteral("refresh"), tr("Refresh (F5)"),
                                 [this]() { emit refreshRequested(); });
    toolbar->addWidget(m_refreshButton);
    m_readButton = makeButton(QStringLiteral("check"), tr("Mark as read"),
                              [this]() { emit markReadRequested(true); });
    toolbar->addWidget(m_readButton);
    m_deleteButton = makeButton(QStringLiteral("trash"), tr("Delete"),
                                [this]() { emit deleteRequested(); });
    toolbar->addWidget(m_deleteButton);

    layout->addLayout(toolbar);

    // --- the mail ---------------------------------------------------------
    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setItemDelegate(new MessageDelegate(this));
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->viewport()->installEventFilter(this);
    layout->addWidget(m_list, 1);

    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        m_filter = static_cast<Filter>(index);
        emit filterChanged(m_filter);
        setMessages(m_messages, m_mailboxes);
    });
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        const auto items = m_list->selectedItems();
        if (!items.isEmpty())
            emit messageActivated(items.first()->data(Qt::UserRole).value<Message>().id);
    });
}

bool MessageList::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QModelIndex index = m_list->indexAt(mouse->position().toPoint());
        if (index.isValid()) {
            // The star sits on the right of the row.
            const QRect row = m_list->visualRect(index);
            const QRect star(row.right() - 34, row.top() + 36, 20, 20);
            if (star.contains(mouse->position().toPoint())) {
                const Message message = index.data(Qt::UserRole).value<Message>();
                const bool starred = !m_starred.value(message.id, false);
                m_starred.insert(message.id, starred);
                emit starToggled(message.id, starred);
                m_list->viewport()->update();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MessageList::setMessages(const QVector<Message>& messages, const QHash<QString, Mailbox>& mailboxes)
{
    m_messages = messages;
    m_mailboxes = mailboxes;

    const QString selected = selectedId();
    m_list->clear();

    QFont metaFont = Theme::font(11);
    QFont senderFont = Theme::font(14, QFont::DemiBold);

    for (const Message& message : messages) {
        if (!matches(message))
            continue;

        const Mailbox mailbox = m_mailboxes.value(message.mailboxId);
        const bool unread = !message.flags.contains(QStringLiteral("\\Seen"));
        const bool starred = m_starred.value(message.id, false);

        auto* item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, QVariant::fromValue(message));
        item->setData(Qt::UserRole + 1, QVariant::fromValue(mailbox));
        item->setData(Qt::UserRole + 2, starred);
        item->setData(Qt::UserRole + 3, unread);
        item->setData(Qt::UserRole + 4, message.date.isValid()
                                               ? message.date.toString(QStringLiteral("HH:mm"))
                                               : QString());
        item->setData(Qt::UserRole + 5, senderName(message.from));
        item->setData(Qt::UserRole + 6, message.subject);
        item->setData(Qt::UserRole + 7, previewOf(message));
        item->setSizeHint(QSize(0, kRowHeight));
        if (message.id == selected)
            m_list->setCurrentItem(item);
    }
    m_list->viewport()->update();
}

bool MessageList::matches(const Message& message) const
{
    if (!m_accountFilter.isEmpty() && message.mailboxId != m_accountFilter)
        return false;
    if (m_filter == Filter::Unread && message.flags.contains(QStringLiteral("\\Seen")))
        return false;
    if (m_filter == Filter::Starred && !m_starred.value(message.id, false))
        return false;
    if (m_filter == Filter::Attachments && !message.hasAttachment)
        return false;
    if (!m_search.isEmpty()) {
        const bool hit = message.subject.contains(m_search, Qt::CaseInsensitive)
                         || message.from.contains(m_search, Qt::CaseInsensitive)
                         || message.to.contains(m_search, Qt::CaseInsensitive);
        if (!hit)
            return false;
    }
    return true;
}

void MessageList::setFilter(Filter filter)
{
    m_filter = filter;
    m_tabs->setCurrentIndex(static_cast<int>(filter));
}

void MessageList::setAccountFilter(const QString& mailboxId)
{
    m_accountFilter = mailboxId;
    setMessages(m_messages, m_mailboxes);
}

void MessageList::setSearchText(const QString& text)
{
    m_search = text;
    setMessages(m_messages, m_mailboxes);
}

QString MessageList::selectedId() const
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
        return {};
    return items.first()->data(Qt::UserRole).value<Message>().id;
}

void MessageList::selectId(const QString& id)
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item->data(Qt::UserRole).value<Message>().id == id) {
            m_list->setCurrentRow(i);
            m_list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

void MessageList::setBusy(bool busy)
{
    m_refreshButton->setEnabled(!busy);
    if (busy) {
        m_refreshButton->setIcon(Icons::icon(QStringLiteral("spinner"), Theme::accent(), 16));
        m_refreshButton->setToolTip(tr("Refreshing..."));
    } else {
        m_refreshButton->setIcon(Icons::icon(QStringLiteral("refresh"), Theme::textSecondary(), 16));
        m_refreshButton->setToolTip(tr("Refresh (F5)"));
    }
}

} // namespace mmc

#include "ui/Sidebar.h"

#include "core/Model.h"
#include "core/ProviderIcons.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QApplication>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace mmc {
namespace {

// Draws the accounts: the logo of the provider, the name, the address and the
// number of unread messages.
class AccountDelegate : public QStyledItemDelegate
{
public:
    explicit AccountDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();

        QWidget* widget = const_cast<QWidget*>(opt.widget);
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const QString providerId = index.data(Qt::UserRole + 1).toString();
        const int unread = index.data(Qt::UserRole + 2).toInt();
        const QString name = index.data(Qt::UserRole + 3).toString();
        const QString address = index.data(Qt::UserRole + 4).toString();

        const bool selected = opt.state & QStyle::State_Selected;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect rect = opt.rect.adjusted(4, 3, -4, -3);
        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::surfaceActive());
            painter->drawRoundedRect(rect, Theme::radiusRow(), Theme::radiusRow());
        }

        const int iconSize = 22;
        const QRect iconRect(rect.left() + 8, rect.top() + 8, iconSize, iconSize);
        const QPixmap logo = providerPixmap(providerId, iconSize);
        if (!logo.isNull()) {
            painter->drawPixmap(iconRect, logo);
        } else {
            painter->setBrush(Theme::surfaceHover());
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(iconRect, 6, 6);
        }

        QFont nameFont = Theme::font(13, unread > 0 ? QFont::DemiBold : QFont::Medium);
        painter->setFont(nameFont);
        painter->setPen(selected ? Theme::textPrimary() : Theme::textPrimary());
        const int badgeWidth = unread > 0 ? 34 : 12;
        QRect nameRect(iconRect.right() + 10, rect.top() + 6, rect.width() - iconSize - 30 - badgeWidth, 18);
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                          opt.fontMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

        QFont addressFont = Theme::font(11);
        painter->setFont(addressFont);
        painter->setPen(Theme::textMuted());
        QRect addressRect(nameRect);
        addressRect.setTop(rect.top() + 24);
        addressRect.setHeight(16);
        painter->drawText(addressRect, Qt::AlignVCenter | Qt::AlignLeft,
                          opt.fontMetrics.elidedText(address, Qt::ElideRight, addressRect.width()));

        if (unread > 0) {
            const QString text = QString::number(unread);
            QFontMetrics metrics(painter->font());
            const int width = metrics.horizontalAdvance(text) + 12;
            const QRect badge(rect.right() - width - 10, rect.center().y() - 9, width, 18);
            painter->setPen(Qt::NoPen);
            painter->setBrush(Theme::accent());
            painter->drawRoundedRect(badge, Theme::radiusBadge(), Theme::radiusBadge());
            painter->setPen(Qt::white);
            painter->drawText(badge, Qt::AlignCenter, text);
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(200, 48);
    }
};

// The logo of the program: a violet square with a white envelope in it.
QPixmap logoMark(int size)
{
    QPixmap canvas(size, size);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0.0, Theme::accentHover());
    gradient.setColorAt(1.0, Theme::accent().darker(118));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(QRectF(0, 0, size, size), size * 0.28, size * 0.28);

    const QPixmap envelope = Icons::pixmap(QStringLiteral("mail"), Qt::white, size * 0.58);
    painter.drawPixmap(QPoint((size - envelope.width()) / 2, (size - envelope.height()) / 2), envelope);
    return canvas;
}

} // namespace

Sidebar::Sidebar(const QString& vaultName, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sidebar"));
    setFixedWidth(264);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 14, 12, 12);
    layout->setSpacing(12);

    // --- the name of the program -----------------------------------------
    auto* brand = new QHBoxLayout();
    brand->setSpacing(10);
    auto* mark = new QLabel(this);
    mark->setPixmap(logoMark(30));
    mark->setFixedSize(30, 30);
    brand->addWidget(mark);

    auto* names = new QVBoxLayout();
    names->setSpacing(0);
    auto* title = new QLabel(QStringLiteral("MMC"), this);
    title->setFont(Theme::font(18, QFont::Bold));
    names->addWidget(title);
    auto* subtitle = new QLabel(tr("Multiple Mail Client"), this);
    subtitle->setFont(Theme::font(11));
    subtitle->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textMuted().name()));
    names->addWidget(subtitle);
    brand->addLayout(names);
    brand->addStretch(1);
    layout->addLayout(brand);

    // --- write a message --------------------------------------------------
    auto* compose = new QPushButton(tr("Write message"), this);
    compose->setProperty("accent", true);
    compose->setIcon(Icons::icon(QStringLiteral("compose"), Qt::white, 16));
    compose->setIconSize(QSize(16, 16));
    compose->setMinimumHeight(38);
    compose->setCursor(Qt::PointingHandCursor);
    connect(compose, &QPushButton::clicked, this, &Sidebar::composeClicked);
    layout->addWidget(compose);

    // --- where to look ----------------------------------------------------
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("nav"));
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setFocusPolicy(Qt::NoFocus);
    m_nav->setSpacing(2);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setMouseTracking(true);

    struct Entry {
        View view;
        const char* icon;
        const char* label;
    };
    const Entry entries[] = {
        {View::AllAccounts, "all", QT_TR_NOOP("All accounts")},
        {View::Inbox, "inbox", QT_TR_NOOP("Inbox")},
        {View::Starred, "star", QT_TR_NOOP("Starred")},
        {View::Sent, "sent", QT_TR_NOOP("Sent")},
        {View::Drafts, "drafts", QT_TR_NOOP("Drafts")},
        {View::Spam, "spam", QT_TR_NOOP("Spam")},
        {View::Trash, "trash", QT_TR_NOOP("Trash")},
    };
    for (const Entry& entry : entries) {
        auto* item = new QListWidgetItem(Icons::icon(QString::fromLatin1(entry.icon),
                                                     Theme::textSecondary(), 18),
                                         tr(entry.label), m_nav);
        item->setData(Qt::UserRole, static_cast<int>(entry.view));
        item->setData(Qt::UserRole + 1, QString::fromLatin1(entry.icon));
        item->setSizeHint(QSize(0, 36));
    }
    m_nav->setFixedHeight(7 * 36 + 4);
    layout->addWidget(m_nav);

    // --- the accounts -----------------------------------------------------
    auto* accountsHeader = new QHBoxLayout();
    accountsHeader->setSpacing(6);
    m_accountsTitle = new QLabel(tr("Accounts"), this);
    QFont heading = Theme::font(11, QFont::DemiBold);
    m_accountsTitle->setFont(heading);
    m_accountsTitle->setStyleSheet(QStringLiteral("color: %1; text-transform: uppercase;")
                                       .arg(Theme::textMuted().name()));
    accountsHeader->addWidget(m_accountsTitle);
    accountsHeader->addStretch(1);

    auto* addButton = new QPushButton(this);
    addButton->setProperty("flat", true);
    addButton->setIcon(Icons::icon(QStringLiteral("add"), Theme::textMuted(), 16));
    addButton->setFixedSize(24, 24);
    addButton->setToolTip(tr("Add an account"));
    addButton->setAccessibleName(tr("Add an account"));
    connect(addButton, &QPushButton::clicked, this, &Sidebar::addAccountClicked);
    accountsHeader->addWidget(addButton);

    m_collapseButton = new QPushButton(this);
    m_collapseButton->setProperty("flat", true);
    m_collapseButton->setIcon(Icons::icon(QStringLiteral("chevron-down"), Theme::textMuted(), 16));
    m_collapseButton->setFixedSize(24, 24);
    m_collapseButton->setToolTip(tr("Hide the accounts"));
    m_collapseButton->setAccessibleName(tr("Hide the accounts"));
    accountsHeader->addWidget(m_collapseButton);
    layout->addLayout(accountsHeader);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Filter accounts"));
    m_filter->setClearButtonEnabled(true);
    m_filter->hide();
    layout->addWidget(m_filter);

    m_accounts = new QListWidget(this);
    m_accounts->setObjectName(QStringLiteral("accounts"));
    m_accounts->setFrameShape(QFrame::NoFrame);
    m_accounts->setFocusPolicy(Qt::NoFocus);
    m_accounts->setItemDelegate(new AccountDelegate(this));
    m_accounts->setMouseTracking(true);
    m_accounts->setContextMenuPolicy(Qt::CustomContextMenu);
    m_accounts->setIconSize(QSize(22, 22));
    layout->addWidget(m_accounts, 1);

    connect(m_collapseButton, &QPushButton::clicked, this, [this]() {
        const bool hidden = m_accounts->isVisible();
        m_accounts->setVisible(!hidden);
        m_filter->setVisible(!hidden && !m_accounts->count());
        m_collapseButton->setIcon(Icons::icon(QStringLiteral("chevron-down"),
                                               Theme::textMuted(), 16));
    });
    connect(m_nav, &QListWidget::itemSelectionChanged, this, [this]() {
        const auto items = m_nav->selectedItems();
        if (items.isEmpty())
            return;
        setView(static_cast<View>(items.first()->data(Qt::UserRole).toInt()));
    });
    connect(m_accounts, &QListWidget::itemSelectionChanged, this, [this]() {
        const auto items = m_accounts->selectedItems();
        if (items.isEmpty())
            return;
        emit accountSelected(items.first()->data(Qt::UserRole).toString());
    });
    connect(m_accounts, &QListWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        const auto items = m_accounts->selectedItems();
        if (items.isEmpty())
            return;
        emit accountContextMenuRequested(items.first()->data(Qt::UserRole).toString(),
                                        m_accounts->mapToGlobal(position));
    });

    setView(View::AllAccounts);
}

QString Sidebar::viewName(View view)
{
    switch (view) {
    case View::Inbox: return QObject::tr("Inbox");
    case View::Starred: return QObject::tr("Starred");
    case View::Sent: return QObject::tr("Sent");
    case View::Drafts: return QObject::tr("Drafts");
    case View::Spam: return QObject::tr("Spam");
    case View::Trash: return QObject::tr("Trash");
    default: return QObject::tr("All accounts");
    }
}

void Sidebar::setAccounts(const QList<Mailbox*>& accounts)
{
    const QString current = selectedAccountId();
    m_accounts->clear();
    for (const Mailbox* mailbox : accounts) {
        auto* item = new QListWidgetItem(m_accounts);
        item->setData(Qt::UserRole, mailbox->id);
        item->setData(Qt::UserRole + 1, mailbox->providerId);
        item->setData(Qt::UserRole + 2, 0);
        item->setData(Qt::UserRole + 3, mailbox->displayName.isEmpty() ? mailbox->address
                                                                       : mailbox->displayName);
        item->setData(Qt::UserRole + 4, mailbox->address);
        item->setToolTip(mailbox->address);
    }
    // No account is picked at the start, the list shows the mail of all of them.
    m_accounts->clearSelection();
}

void Sidebar::setUnread(const QString& mailboxId, int count)
{
    for (int i = 0; i < m_accounts->count(); ++i) {
        QListWidgetItem* item = m_accounts->item(i);
        if (item->data(Qt::UserRole).toString() == mailboxId) {
            item->setData(Qt::UserRole + 2, count);
            m_accounts->viewport()->update();
            return;
        }
    }
}

void Sidebar::setView(View view)
{
    m_view = view;
    for (int i = 0; i < m_nav->count(); ++i) {
        QListWidgetItem* item = m_nav->item(i);
        const bool active = item->data(Qt::UserRole).toInt() == static_cast<int>(view);
        const QString name = item->text();
        const QString iconName = item->data(Qt::UserRole + 1).toString();
        item->setIcon(Icons::icon(iconName, active ? Theme::accent() : Theme::textSecondary(), 18));
        item->setForeground(active ? Theme::textPrimary() : Theme::textSecondary());
        if (active && m_nav->currentItem() != item)
            m_nav->setCurrentItem(item);
    }
    if (m_nav->currentRow() < 0)
        m_nav->setCurrentRow(0);
    emit viewChanged(view);
}

QString Sidebar::selectedAccountId() const
{
    const QListWidgetItem* item = m_accounts->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void Sidebar::setSelectedAccountId(const QString& id)
{
    for (int i = 0; i < m_accounts->count(); ++i) {
        if (m_accounts->item(i)->data(Qt::UserRole).toString() == id) {
            m_accounts->setCurrentRow(i);
            return;
        }
    }
}

} // namespace mmc

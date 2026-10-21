#include "ui/AccountManager.h"

#include "core/ProviderIcons.h"
#include "core/Vault.h"
#include "net/Providers.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include "ui/MailboxDialog.h"
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace mmc {
namespace {

QString providerLabelFor(const QString& providerId)
{
    const Provider* provider = providerById(providerId);
    return provider ? provider->name : providerId;
}

// One row per account: the logo, the name and the address.
class AccountRowDelegate : public QStyledItemDelegate
{
public:
    explicit AccountRowDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {
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
        const QString name = index.data(Qt::UserRole + 2).toString();
        const QString address = index.data(Qt::UserRole + 3).toString();
        const QString status = index.data(Qt::UserRole + 4).toString();
        const QColor statusColour = index.data(Qt::UserRole + 5).value<QColor>();

        painter->save();
        QRect rect = opt.rect;
        const int iconSize = 26;
        const QRect iconRect(rect.left() + 8, rect.center().y() - iconSize / 2, iconSize, iconSize);
        const QPixmap logo = providerPixmap(providerId, iconSize);
        if (!logo.isNull())
            painter->drawPixmap(iconRect, logo);

        QFont nameFont = Theme::font(13, QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(Theme::textPrimary());
        const QRect nameRect(iconRect.right() + 10, rect.top() + 6, rect.width() - iconSize - 130, 18);
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                          opt.fontMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

        QFont addressFont = Theme::font(11);
        painter->setFont(addressFont);
        painter->setPen(Theme::textMuted());
        QRect addressRect(nameRect);
        addressRect.setTop(rect.top() + 24);
        painter->drawText(addressRect, Qt::AlignVCenter | Qt::AlignLeft,
                          opt.fontMetrics.elidedText(address, Qt::ElideRight, addressRect.width()));

        // The state of the connection, with a colour and with a word.
        painter->setPen(Qt::NoPen);
        painter->setBrush(statusColour);
        painter->drawEllipse(QPointF(rect.right() - 108, rect.center().y()), 4, 4);
        painter->setFont(Theme::font(11));
        painter->setPen(statusColour);
        painter->drawText(QRect(rect.right() - 98, rect.top() + 6, 90, 18),
                          Qt::AlignVCenter | Qt::AlignLeft, status);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(320, 48);
    }
};

} // namespace

AccountManager::AccountManager(Vault* vault, QWidget* parent)
    : QDialog(parent), m_vault(vault)
{
    setWindowTitle(tr("Accounts"));
    resize(720, 520);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    auto* heading = new QLabel(tr("Accounts"), this);
    heading->setFont(Theme::font(19, QFont::DemiBold));
    layout->addWidget(heading);

    auto* description = new QLabel(
        tr("Every account lives in the vault. Removing an account here only removes it from MMC, "
           "the account itself stays at the provider."), this);
    description->setFont(Theme::font(12));
    description->setWordWrap(true);
    description->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textSecondary().name()));
    layout->addWidget(description);

    auto* body = new QHBoxLayout();
    body->setSpacing(16);

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new AccountRowDelegate(this));
    m_list->setMouseTracking(true);
    m_list->setMinimumWidth(360);
    body->addWidget(m_list, 2);

    auto* side = new QVBoxLayout();
    side->setSpacing(10);
    m_details = new QLabel(this);
    m_details->setFont(Theme::font(12));
    m_details->setWordWrap(true);
    m_details->setTextFormat(Qt::RichText);
    side->addWidget(m_details);
    side->addStretch(1);

    auto* signInButton = new QPushButton(tr("Sign in again"), this);
    connect(signInButton, &QPushButton::clicked, this, [this]() {
        const auto items = m_list->selectedItems();
        if (items.isEmpty())
            return;
        const QString id = items.first()->data(Qt::UserRole).toString();
        const Mailbox* mailbox = m_vault->state().mailboxById(id);
        if (!mailbox)
            return;
        MailboxDialog dialog(m_vault, *mailbox, MailboxDialog::EditAccount, true, this);
        if (dialog.exec() == QDialog::Accepted) {
            rebuild();
            emit accountChanged();
        }
    });
    side->addWidget(signInButton);

    auto* removeButton = new QPushButton(tr("Remove account"), this);
    removeButton->setProperty("danger", true);
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        const auto items = m_list->selectedItems();
        if (items.isEmpty())
            return;
        const QString id = items.first()->data(Qt::UserRole).toString();
        const Mailbox* mailbox = m_vault->state().mailboxById(id);
        if (!mailbox)
            return;
        if (QMessageBox::question(this, tr("Remove the account"),
                                  tr("Remove %1 from MMC? The account itself is not deleted at "
                                     "the provider.").arg(mailbox->address))
            != QMessageBox::Yes)
            return;
        m_vault->state().removeMailbox(id);
        QString error;
        m_vault->save(&error);
        rebuild();
        emit accountChanged();
    });
    side->addWidget(removeButton);
    body->addLayout(side, 1);

    layout->addLayout(body, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &AccountManager::refreshSelected);
    rebuild();
}

void AccountManager::rebuild()
{
    m_list->clear();
    for (const Mailbox& mailbox : m_vault->state().mailboxes) {
        const bool connected = m_vault->state().tokenFor(mailbox.id) != nullptr
                               || !mailbox.secret.isEmpty();
        const int unseen = m_vault->state()
                               .settings.value(QStringLiteral("unseen.%1").arg(mailbox.id))
                               .toInt();
        auto* item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, mailbox.id);
        item->setData(Qt::UserRole + 1, mailbox.providerId);
        item->setData(Qt::UserRole + 2, mailbox.displayName.isEmpty() ? mailbox.address
                                                                    : mailbox.displayName);
        item->setData(Qt::UserRole + 3, mailbox.address);
        item->setData(Qt::UserRole + 4, connected ? tr("Connected") : tr("Not connected"));
        item->setData(Qt::UserRole + 5, connected ? Theme::success() : Theme::textMuted());
        item->setToolTip(tr("%1 unread").arg(unseen));
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    else
        m_details->setText(tr("There are no accounts yet."));
}

void AccountManager::refreshSelected()
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
        return;
    const QString id = items.first()->data(Qt::UserRole).toString();
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox)
        return;
    const int unseen = m_vault->state()
                           .settings.value(QStringLiteral("unseen.%1").arg(id))
                           .toInt();
    const bool connected = m_vault->state().tokenFor(id) != nullptr || !mailbox->secret.isEmpty();

    QString html;
    html += QStringLiteral("<b>%1</b><br>").arg(mailbox->displayName.toHtmlEscaped());
    html += QStringLiteral("%1<br>").arg(mailbox->address.toHtmlEscaped());
    html += QStringLiteral("Provider: %1<br>").arg(providerLabelFor(mailbox->providerId).toHtmlEscaped());
    html += QStringLiteral("State: %1<br>").arg(connected ? tr("Connected") : tr("Not connected"));
    html += QStringLiteral("Unread: %1").arg(unseen);
    if (mailbox->cooldownUntil.isValid()) {
        html += QStringLiteral("<br>Cooldown until %1")
                    .arg(mailbox->cooldownUntil.toString(QStringLiteral("dd MMM yyyy")));
    }
    m_details->setText(html);
}

} // namespace mmc

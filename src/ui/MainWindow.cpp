#include "ui/MainWindow.h"

#include "core/Importer.h"
#include "core/Vault.h"
#include "net/MailService.h"
#include "net/Mime.h"
#include "net/Providers.h"
#include "ui/ComposeDialog.h"
#include "ui/MailboxDialog.h"
#include "core/ProviderIcons.h"
#include "ui/VaultDialogs.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace mmc {
namespace {

QString webmailUrlFor(const QString& providerId)
{
    if (providerId == QLatin1String("google"))
        return QStringLiteral("https://mail.google.com/");
    if (providerId == QLatin1String("microsoft"))
        return QStringLiteral("https://outlook.live.com/mail/0/");
    if (providerId == QLatin1String("yandex"))
        return QStringLiteral("https://mail.yandex.ru/");
    return QString();
}

QString providerLabelFor(const QString& providerId)
{
    const Provider* provider = providerById(providerId);
    return provider ? provider->name : providerId;
}

QColor providerColor(const QString& providerId)
{
    if (providerId == QLatin1String("google"))
        return QColor(0xE8, 0x77, 0x22);
    if (providerId == QLatin1String("microsoft"))
        return QColor(0x2F, 0x7C, 0xC4);
    if (providerId == QLatin1String("yandex"))
        return QColor(0xE0, 0x4B, 0x3A);
    return QColor(0x6E, 0x77, 0x83);
}

QString providerInitial(const QString& providerId)
{
    if (providerId == QLatin1String("google"))
        return QStringLiteral("G");
    if (providerId == QLatin1String("microsoft"))
        return QStringLiteral("M");
    if (providerId == QLatin1String("yandex"))
        return QStringLiteral("Y");
    return QStringLiteral("?");
}

QString formatDate(const QDateTime& date)
{
    if (!date.isValid())
        return {};
    if (date.date() == QDate::currentDate())
        return date.toString(QStringLiteral("HH:mm"));
    if (date.date().addDays(1) == QDate::currentDate())
        return QObject::tr("yesterday");
    if (date.date().year() == QDate::currentDate().year())
        return date.toString(QStringLiteral("dd MMM HH:mm"));
    return date.toString(QStringLiteral("dd MMM yyyy"));
}

// "Someone <someone@gmail.com>" becomes "Someone".
QString senderName(const QString& from)
{
    const QString decoded = decodeHeaderValue(from);
    const int bracket = decoded.indexOf(QChar('<'));
    if (bracket < 0)
        return decoded.trimmed();
    QString name = decoded.left(bracket).trimmed();
    if (name.startsWith(QChar('"')) && name.endsWith(QChar('"')) && name.size() > 1)
        name = name.mid(1, name.size() - 2);
    return name.isEmpty() ? decoded.trimmed() : name;
}

// Draws the account list: a coloured letter for the provider, the name, the
// number of unread messages and a mark for an account on cooldown.
class AccountDelegate : public QStyledItemDelegate
{
public:
    explicit AccountDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();

        QWidget* widget = const_cast<QWidget*>(opt.widget);
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const QString providerId = index.data(Qt::UserRole + 1).toString();
        const int unread = index.data(Qt::UserRole + 2).toInt();
        const bool cooldown = index.data(Qt::UserRole + 3).toBool();
        const QString name = index.data(Qt::UserRole + 4).toString();
        const bool isAll = index.data(Qt::UserRole + 5).toBool();

        const QRect rect = opt.rect;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const int chipSize = 20;
        const QRect chip(rect.left() + 6, rect.center().y() - chipSize / 2, chipSize, chipSize);
        const QPixmap logo = providerPixmap(providerId, chipSize - 2);
        if (!logo.isNull()) {
            painter->drawPixmap(chip, logo);
        } else if (isAll) {
            // A small envelope for the unified inbox.
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(QColor(0x9A, 0xA4, 0xB0), 1.5));
            painter->drawRoundedRect(chip.adjusted(1, 3, -1, -3), 3, 3);
            painter->drawLine(chip.left() + 2, chip.top() + 4, chip.center().x(), chip.top() + 9);
            painter->drawLine(chip.right() - 2, chip.top() + 4, chip.center().x(), chip.top() + 9);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(providerColor(providerId));
            painter->drawRoundedRect(chip, 5, 5);
            painter->setPen(Qt::white);
            QFont letterFont = painter->font();
            letterFont.setBold(true);
            painter->setFont(letterFont);
            painter->drawText(chip, Qt::AlignCenter, providerInitial(providerId));
        }

        QFont nameFont = painter->font();
        nameFont.setBold(unread > 0 || isAll);
        painter->setFont(nameFont);
        if (opt.state & QStyle::State_Selected)
            painter->setPen(opt.palette.color(QPalette::HighlightedText));
        else
            painter->setPen(opt.palette.color(QPalette::Text));
        const int rightSpace = unread > 0 ? 44 : (cooldown ? 26 : 10);
        QRect textRect(chip.right() + 8, rect.top(), rect.right() - chip.right() - 8 - rightSpace,
                       rect.height());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          opt.fontMetrics.elidedText(name, Qt::ElideRight, textRect.width()));

        if (unread > 0) {
            const QString text = QString::number(unread);
            const QFontMetrics metrics(painter->font());
            const int width = metrics.horizontalAdvance(text) + 14;
            const QRect pill(rect.right() - width - 8, rect.center().y() - 9, width, 18);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0x5A, 0x6B, 0x7D));
            painter->drawRoundedRect(pill, 9, 9);
            painter->setPen(Qt::white);
            painter->drawText(pill, Qt::AlignCenter, text);
        }

        if (cooldown) {
            painter->setPen(QColor(0xE0, 0xA8, 0x00));
            painter->drawText(QRect(rect.right() - 22, rect.center().y() - 10, 18, 20),
                              Qt::AlignCenter, QStringLiteral("*"));
        }
        painter->restore();
    }
};

} // namespace

MainWindow::MainWindow(Vault* vault, QWidget* parent)
    : QMainWindow(parent), m_vault(vault)
{
    setWindowTitle(tr("MMC - %1").arg(vault->name()));
    resize(1360, 840);
    buildUi();

    m_service = new MailService(m_vault, this);
    connect(m_service, &MailService::syncFinished, this, &MainWindow::onSyncFinished);
    connect(m_service, &MailService::bodyReady, this, &MainWindow::onBodyReady);
    connect(m_service, &MailService::bodyFailed, this, &MainWindow::onBodyFailed);
    connect(m_service, &MailService::sendFinished, this, &MainWindow::onSendFinished);
    connect(m_service, &MailService::logMessage, this, &MainWindow::log);

    rebuildAccountList();
    rebuildMessageList();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* refreshAll = new QAction(tr("Refresh all"), this);
    refreshAll->setToolTip(tr("Fetch the newest mail of every account (Ctrl+R)"));
    refreshAll->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(refreshAll, &QAction::triggered, this, &MainWindow::onRefreshAll);
    toolbar->addAction(refreshAll);

    auto* refreshOne = new QAction(tr("Refresh"), this);
    refreshOne->setToolTip(tr("Fetch the newest mail of the account that is picked (F5)"));
    refreshOne->setShortcut(QKeySequence(Qt::Key_F5));
    connect(refreshOne, &QAction::triggered, this, &MainWindow::onRefreshAccount);
    toolbar->addAction(refreshOne);

    toolbar->addSeparator();

    auto* compose = new QAction(tr("New message"), this);
    compose->setToolTip(tr("Write a message (Ctrl+N)"));
    compose->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(compose, &QAction::triggered, this, &MainWindow::onCompose);
    toolbar->addAction(compose);

    auto* webmail = new QAction(tr("Webmail"), this);
    webmail->setToolTip(tr("Open the web mail of the account that is picked"));
    connect(webmail, &QAction::triggered, this, &MainWindow::onOpenWebmail);
    toolbar->addAction(webmail);

    toolbar->addSeparator();

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search the loaded mail (Ctrl+F)"));
    m_search->setMaximumWidth(300);
    m_search->setClearButtonEnabled(true);
    m_search->setToolTip(tr("Search the mail that was loaded, not the mail on the server"));
    connect(m_search, &QLineEdit::returnPressed, this, &MainWindow::onSearch);
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onSearch);
    toolbar->addWidget(m_search);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    auto* settings = new QAction(tr("Settings"), this);
    connect(settings, &QAction::triggered, this, &MainWindow::onSettings);
    toolbar->addAction(settings);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);

    auto* splitter = new QSplitter(Qt::Horizontal, central);

    // --- left: the accounts ---------------------------------------------
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    auto* accountsTitle = new QLabel(tr("Accounts"), left);
    QFont bold = accountsTitle->font();
    bold.setBold(true);
    accountsTitle->setFont(bold);
    leftLayout->addWidget(accountsTitle);

    m_accountFilter = new QLineEdit(left);
    m_accountFilter->setPlaceholderText(tr("Filter accounts"));
    m_accountFilter->setClearButtonEnabled(true);
    m_accountFilter->setToolTip(tr("Type part of a name or an address to narrow the list down"));
    leftLayout->addWidget(m_accountFilter);

    m_accounts = new QListWidget(left);
    m_accounts->setSelectionMode(QAbstractItemView::SingleSelection);
    m_accounts->setItemDelegate(new AccountDelegate(this));
    m_accounts->setUniformItemSizes(true);
    leftLayout->addWidget(m_accounts, 1);

    m_cooldown = new QLabel(left);
    m_cooldown->setWordWrap(true);
    m_cooldown->setTextFormat(Qt::PlainText);
    leftLayout->addWidget(m_cooldown);

    auto* accountButtons = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Add account"), left);
    auto* editButton = new QPushButton(tr("Edit"), left);
    auto* importButton = new QPushButton(tr("Import"), left);
    auto* removeButton = new QPushButton(tr("Remove"), left);
    accountButtons->addWidget(addButton);
    accountButtons->addWidget(editButton);
    accountButtons->addWidget(importButton);
    accountButtons->addStretch(1);
    accountButtons->addWidget(removeButton);
    leftLayout->addLayout(accountButtons);

    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddAccount);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::onEditAccount);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveAccount);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportAccounts);
    connect(m_accounts, &QListWidget::itemSelectionChanged, this, &MainWindow::onAccountSelectionChanged);
    connect(m_accountFilter, &QLineEdit::textChanged, this, &MainWindow::onAccountFilterChanged);
    m_accounts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_accounts, &QListWidget::customContextMenuRequested, this, &MainWindow::onAccountMenu);

    // --- middle: the mail ------------------------------------------------
    m_messageArea = new QStackedWidget(splitter);

    m_messages = new QTableWidget(m_messageArea);
    m_messages->setColumnCount(5);
    m_messages->setHorizontalHeaderLabels({tr("Account"), tr("From"), tr("Subject"), tr("Date"), QString()});
    m_messages->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_messages->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_messages->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_messages->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_messages->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_messages->horizontalHeader()->resizeSection(4, 26);
    m_messages->horizontalHeader()->setSectionsClickable(true);
    m_messages->verticalHeader()->setVisible(false);
    m_messages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_messages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_messages->setShowGrid(false);
    m_messages->setAlternatingRowColors(false);
    m_messages->setWordWrap(false);
    m_messageArea->addWidget(m_messages);

    m_messageHint = new QLabel(tr("No mail here yet.\n\nPress Refresh all (Ctrl+R) to fetch the mail of "
                                  "every account."), m_messageArea);
    m_messageHint->setAlignment(Qt::AlignCenter);
    m_messageHint->setWordWrap(true);
    m_messageArea->addWidget(m_messageHint);
    m_messageArea->setCurrentWidget(m_messageHint);

    connect(m_messages, &QTableWidget::itemSelectionChanged, this, &MainWindow::onMessageSelectionChanged);
    connect(m_messages->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onSortColumn);

    // --- right: the message ----------------------------------------------
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    m_readerHeader = new QLabel(right);
    m_readerHeader->setTextFormat(Qt::RichText);
    m_readerHeader->setWordWrap(true);
    m_readerHeader->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_readerHeader->setVisible(false);
    rightLayout->addWidget(m_readerHeader);

    m_reader = new QTextBrowser(right);
    m_reader->setOpenExternalLinks(true);
    m_reader->setOpenLinks(false);
    m_reader->setPlaceholderText(tr("Pick a message to read it."));
    rightLayout->addWidget(m_reader, 1);

    m_attachments = new QListWidget(right);
    m_attachments->setMaximumHeight(100);
    m_attachments->setToolTip(tr("Attachments of the selected message"));
    m_attachments->setVisible(false);
    rightLayout->addWidget(m_attachments);

    auto* saveAttachment = new QPushButton(tr("Save attachment"), right);
    saveAttachment->setVisible(false);
    rightLayout->addWidget(saveAttachment);
    connect(saveAttachment, &QPushButton::clicked, this, &MainWindow::onSaveAttachment);

    splitter->addWidget(left);
    splitter->addWidget(m_messageArea);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 4);
    splitter->setSizes({270, 620, 700});
    layout->addWidget(splitter);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status);

    // Ctrl+F puts the cursor in the search box.
    auto* findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_search->setFocus();
        m_search->selectAll();
    });
}

void MainWindow::log(const QString& text)
{
    m_status->setText(text);
}

QString MainWindow::selectedMailboxId() const
{
    const QListWidgetItem* item = m_accounts->currentItem();
    if (!item)
        return {};
    return item->data(Qt::UserRole).toString();
}

void MainWindow::rebuildAccountList()
{
    const QString current = selectedMailboxId();
    m_accounts->clear();

    // The unified inbox: every account at once.
    auto* all = new QListWidgetItem(tr("All accounts"), m_accounts);
    all->setData(Qt::UserRole, QString());
    all->setData(Qt::UserRole + 4, tr("All accounts"));
    all->setData(Qt::UserRole + 5, true);
    all->setToolTip(tr("Show the mail of every account in one list"));

    int totalUnread = 0;
    for (const Mailbox& mailbox : m_vault->state().mailboxes) {
        const int unseen = m_vault->state()
                               .settings.value(QStringLiteral("unseen.%1").arg(mailbox.id))
                               .toInt();
        totalUnread += unseen;
        const bool cooldown = mailbox.cooldownUntil.isValid()
                              && mailbox.cooldownUntil > QDateTime::currentDateTime();

        auto* item = new QListWidgetItem(mailbox.displayName.isEmpty() ? mailbox.address
                                                                       : mailbox.displayName,
                                         m_accounts);
        item->setData(Qt::UserRole, mailbox.id);
        item->setData(Qt::UserRole + 1, mailbox.providerId);
        item->setData(Qt::UserRole + 2, unseen);
        item->setData(Qt::UserRole + 3, cooldown);
        item->setData(Qt::UserRole + 4, mailbox.displayName.isEmpty() ? mailbox.address
                                                                     : mailbox.displayName);
        item->setToolTip(QStringLiteral("%1\n%2").arg(mailbox.address, providerLabelFor(mailbox.providerId)));
    }
    all->setData(Qt::UserRole + 2, totalUnread);

    // Keep the selection where it was, the unified inbox is the default.
    int rowToSelect = 0;
    for (int i = 0; i < m_accounts->count(); ++i) {
        if (m_accounts->item(i)->data(Qt::UserRole).toString() == current) {
            rowToSelect = i;
            break;
        }
    }
    m_accounts->setCurrentRow(rowToSelect);
    onAccountFilterChanged();
    showCooldownStatus();
}

void MainWindow::onAccountFilterChanged()
{
    const QString filter = m_accountFilter->text().trimmed();
    for (int i = 0; i < m_accounts->count(); ++i) {
        QListWidgetItem* item = m_accounts->item(i);
        if (i == 0)
            continue;   // the unified inbox is always there
        const QString name = item->data(Qt::UserRole + 4).toString();
        const QString address = item->toolTip().section(QChar('\n'), 0, 0);
        const bool hit = filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive)
                         || address.contains(filter, Qt::CaseInsensitive);
        m_accounts->setRowHidden(i, !hit);
    }
}

void MainWindow::rebuildMessageList()
{
    const QString filter = m_search->text().trimmed();
    const QString onlyAccount = selectedMailboxId();

    QVector<Message> messages = m_vault->state().messages;

    std::sort(messages.begin(), messages.end(), [this](const Message& a, const Message& b) {
        int result = 0;
        switch (m_sortColumn) {
        case 0: result = a.mailboxId.compare(b.mailboxId); break;
        case 1: result = senderName(a.from).compare(senderName(b.from), Qt::CaseInsensitive); break;
        case 2: result = a.subject.compare(b.subject, Qt::CaseInsensitive); break;
        default: result = (a.date == b.date) ? 0 : (a.date < b.date ? -1 : 1); break;
        }
        if (result == 0)
            result = (a.date == b.date) ? 0 : (a.date < b.date ? -1 : 1);
        return m_sortAscending ? result < 0 : result > 0;
    });

    int row = 0;
    for (const Message& message : messages) {
        if (!onlyAccount.isEmpty() && message.mailboxId != onlyAccount)
            continue;
        if (!filter.isEmpty()) {
            const bool hit = message.subject.contains(filter, Qt::CaseInsensitive)
                             || message.from.contains(filter, Qt::CaseInsensitive)
                             || message.to.contains(filter, Qt::CaseInsensitive);
            if (!hit)
                continue;
        }

        const Mailbox* mailbox = m_vault->state().mailboxById(message.mailboxId);
        const QString account = mailbox
                                    ? (mailbox->displayName.isEmpty() ? mailbox->address
                                                                      : mailbox->displayName)
                                    : message.mailboxId;
        const bool unread = !message.flags.contains(QStringLiteral("\\Seen"));

        m_messages->insertRow(row);

        auto* accountItem = new QTableWidgetItem(account);
        accountItem->setData(Qt::UserRole, message.id);
        if (mailbox) {
            accountItem->setIcon(providerIcon(mailbox->providerId, 16));
            accountItem->setForeground(providerColor(mailbox->providerId));
        }
        m_messages->setItem(row, 0, accountItem);

        auto* fromItem = new QTableWidgetItem(senderName(message.from));
        fromItem->setData(Qt::UserRole, message.id);
        fromItem->setToolTip(decodeHeaderValue(message.from));
        m_messages->setItem(row, 1, fromItem);

        auto* subjectItem = new QTableWidgetItem(decodeHeaderValue(message.subject));
        subjectItem->setData(Qt::UserRole, message.id);
        if (unread) {
            QFont font = subjectItem->font();
            font.setBold(true);
            subjectItem->setFont(font);
        }
        m_messages->setItem(row, 2, subjectItem);

        m_messages->setItem(row, 3, new QTableWidgetItem(formatDate(message.date)));
        m_messages->setItem(row, 4, new QTableWidgetItem(QString()));
        row++;
    }

    m_messages->setRowCount(row);
    m_messageArea->setCurrentWidget(row > 0 ? static_cast<QWidget*>(m_messages) : static_cast<QWidget*>(m_messageHint));
    if (row == 0) {
        m_messageHint->setText(m_accounts->count() <= 1
                                    ? tr("No accounts yet.\n\nPress Add account to connect one.")
                                    : tr("No mail here yet.\n\nPress Refresh all (Ctrl+R) to fetch the "
                                         "mail of every account."));
    }
}

void MainWindow::onSortColumn(int column)
{
    if (column == m_sortColumn)
        m_sortAscending = !m_sortAscending;
    else {
        m_sortColumn = column;
        m_sortAscending = column != 3;   // the newest first for the date
    }
    rebuildMessageList();
}

Message MainWindow::messageById(const QString& id) const
{
    for (const Message& message : m_vault->state().messages)
        if (message.id == id)
            return message;
    return Message();
}

void MainWindow::showCooldownStatus()
{
    const QString id = selectedMailboxId();
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox) {
        m_cooldown->clear();
        return;
    }
    if (!mailbox->cooldownUntil.isValid()) {
        m_cooldown->setText(mailbox->cooldownNote);
        return;
    }
    const qint64 seconds = QDateTime::currentDateTime().secsTo(mailbox->cooldownUntil);
    if (seconds <= 0) {
        m_cooldown->setText(tr("%1\nThe cooldown is over, the account details can be changed again.")
                                .arg(mailbox->cooldownNote));
        return;
    }
    const int days = static_cast<int>(seconds / 86400);
    m_cooldown->setText(tr("%1\nDetails cannot be changed for %2 more day(s) (until %3).")
                            .arg(mailbox->cooldownNote)
                            .arg(days)
                            .arg(mailbox->cooldownUntil.toString(QStringLiteral("dd MMM yyyy"))));
}

void MainWindow::onAccountSelectionChanged()
{
    rebuildMessageList();
    showCooldownStatus();
}

void MainWindow::storeTokens(const QString& mailboxId, const OAuth2Tokens& tokens)
{
    if (!tokens.isValid())
        return;
    OAuthToken* token = m_vault->state().tokenFor(mailboxId);
    if (!token) {
        OAuthToken fresh;
        fresh.mailboxId = mailboxId;
        m_vault->state().tokens.append(fresh);
        token = m_vault->state().tokenFor(mailboxId);
    }
    if (!token)
        return;
    token->accessToken = tokens.accessToken;
    token->refreshToken = tokens.refreshToken;
    token->expiry = tokens.expiry;
    token->scope = tokens.scope;
}

void MainWindow::applyMailbox(const Mailbox& mailbox, bool isNew)
{
    if (isNew) {
        m_vault->state().mailboxes.append(mailbox);
    } else {
        for (Mailbox& existing : m_vault->state().mailboxes) {
            if (existing.id == mailbox.id) {
                existing = mailbox;
                break;
            }
        }
    }
    QString error;
    if (!m_vault->save(&error))
        log(error);
    rebuildAccountList();
    rebuildMessageList();
}

void MainWindow::onAddAccount()
{
    MailboxDialog dialog(m_vault, Mailbox(), MailboxDialog::AddAccount, false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const Mailbox mailbox = dialog.mailbox();
    applyMailbox(mailbox, true);
    storeTokens(mailbox.id, dialog.tokens());

    if (!dialog.signInError().isEmpty()) {
        QMessageBox::warning(
            this, tr("The account was added, but the sign in failed"),
            tr("%1\n\nThe account is in the list, you can try the sign in again by right "
               "clicking it.").arg(dialog.signInError()));
        return;
    }

    const bool hasToken = m_vault->state().tokenFor(mailbox.id) != nullptr;
    if (!hasToken && mailbox.secret.isEmpty()) {
        QMessageBox::information(
            this, tr("The account was added"),
            tr("%1 is in the list.\n\nIt cannot read mail yet. Open Settings and put in the "
               "client id of your %2 application, then right click the account and choose "
               "'Sign in again'.").arg(mailbox.address, providerLabelFor(mailbox.providerId)));
        return;
    }

    // The account is connected, so fetch the first mail right away.
    log(tr("Fetching the first mail of %1...").arg(mailbox.address));
    m_service->syncMailbox(mailbox.id);
}

void MainWindow::onEditAccount()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("No account picked"),
                                 tr("Pick one account in the list first."));
        return;
    }
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox)
        return;

    MailboxDialog dialog(m_vault, *mailbox, MailboxDialog::EditAccount, false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const Mailbox updated = dialog.mailbox();
    const bool startedCooldown = !mailbox->cooldownUntil.isValid() && updated.cooldownUntil.isValid();
    applyMailbox(updated, false);
    storeTokens(updated.id, dialog.tokens());

    if (startedCooldown) {
        QMessageBox::information(
            this, tr("Cooldown started"),
            tr("The details of %1 cannot be changed until %2.")
                .arg(updated.displayName.isEmpty() ? updated.address : updated.displayName,
                     updated.cooldownUntil.toString(QStringLiteral("dd MMM yyyy"))));
    }
    if (!dialog.signInError().isEmpty())
        QMessageBox::warning(this, tr("The sign in failed"), dialog.signInError());
}

void MainWindow::onRemoveAccount()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("No account picked"),
                                 tr("Pick the account you want to remove in the list first."));
        return;
    }
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox)
        return;

    if (mailbox->cooldownUntil.isValid() && mailbox->cooldownUntil > QDateTime::currentDateTime()) {
        QMessageBox::warning(
            this, tr("The account is on cooldown"),
            tr("The details of %1 cannot be changed until %2. The cooldown is there to "
               "protect the account, removing it now can get the account locked.")
                .arg(mailbox->address,
                     mailbox->cooldownUntil.toString(QStringLiteral("dd MMM yyyy"))));
        return;
    }

    if (QMessageBox::question(this, tr("Remove the account"),
                              tr("Remove %1 from the vault? The account itself is not deleted.")
                                  .arg(mailbox->address))
        != QMessageBox::Yes)
        return;

    m_vault->state().removeMailbox(id);
    QString error;
    if (!m_vault->save(&error))
        log(error);
    rebuildAccountList();
    rebuildMessageList();
}

void MainWindow::onSignInAgain()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("No account picked"),
                                 tr("Pick the account in the list first."));
        return;
    }
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox)
        return;

    MailboxDialog dialog(m_vault, *mailbox, MailboxDialog::EditAccount, true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.hasTokens()) {
        storeTokens(mailbox->id, dialog.tokens());
        QString error;
        m_vault->save(&error);
        log(tr("Signed in."));
    }
    if (!dialog.signInError().isEmpty())
        QMessageBox::warning(this, tr("The sign in failed"), dialog.signInError());
}

void MainWindow::onAccountMenu()
{
    const QString id = selectedMailboxId();
    QMenu menu(this);
    QAction* edit = menu.addAction(tr("Edit account"));
    QAction* signIn = menu.addAction(tr("Sign in again"));
    QAction* webmail = menu.addAction(tr("Open webmail"));
    menu.addSeparator();
    QAction* remove = menu.addAction(tr("Remove account"));
    if (id.isEmpty()) {
        edit->setEnabled(false);
        signIn->setEnabled(false);
        webmail->setEnabled(false);
        remove->setEnabled(false);
    }

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen)
        return;
    if (chosen == edit) {
        onEditAccount();
    } else if (chosen == signIn) {
        onSignInAgain();
    } else if (chosen == webmail) {
        onOpenWebmail();
    } else if (chosen == remove) {
        onRemoveAccount();
    }
}

void MainWindow::onImportAccounts()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import accounts"), QString(),
        tr("Text files (*.txt *.csv);;All files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Cannot read the file"), file.errorString());
        return;
    }
    const ImportResult result = importMailboxesFromText(file.readAll());

    if (!result.errors.isEmpty()) {
        QMessageBox::warning(this, tr("Some lines were skipped"),
                             result.errors.join(QStringLiteral("\n")));
    }
    if (result.mailboxes.isEmpty())
        return;

    for (const Mailbox& mailbox : result.mailboxes)
        m_vault->state().mailboxes.append(mailbox);
    QString error;
    if (!m_vault->save(&error))
        log(error);
    rebuildAccountList();
    log(tr("%1 accounts imported.").arg(result.mailboxes.size()));
}

void MainWindow::onRefreshAll()
{
    log(tr("Refreshing every account..."));
    m_service->syncAll();
}

void MainWindow::onRefreshAccount()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        onRefreshAll();
        return;
    }
    log(tr("Refreshing..."));
    m_service->syncMailbox(id);
}

void MainWindow::onSyncFinished(const QString& mailboxId, bool ok, const QString& error)
{
    if (!ok) {
        const Mailbox* mailbox = m_vault->state().mailboxById(mailboxId);
        log(tr("%1: %2").arg(mailbox ? mailbox->address : mailboxId, error));
        return;
    }
    rebuildAccountList();
    rebuildMessageList();
    log(tr("Refreshed."));
}

void MainWindow::onMessageSelectionChanged()
{
    const auto items = m_messages->selectedItems();
    if (items.isEmpty())
        return;
    const QString id = items.first()->data(Qt::UserRole).toString();
    if (id.isEmpty() || id == m_currentMessageId)
        return;
    m_currentMessageId = id;

    if (m_bodyCache.contains(id)) {
        showMessageBody(parseMime(m_bodyCache.value(id)), m_bodyCache.value(id));
        return;
    }
    m_readerHeader->setVisible(false);
    m_reader->setPlainText(tr("Fetching the message..."));
    m_service->fetchBody(id);
}

void MainWindow::onBodyReady(const QString& messageId, const QByteArray& raw)
{
    m_bodyCache.insert(messageId, raw);
    if (messageId != m_currentMessageId)
        return;
    showMessageBody(parseMime(raw), raw);
}

void MainWindow::onBodyFailed(const QString& messageId, const QString& error)
{
    if (messageId != m_currentMessageId)
        return;
    m_readerHeader->setVisible(false);
    m_reader->setPlainText(error);
    log(error);
}

void MainWindow::showMessageBody(const ParsedMessage& parsed, const QByteArray& raw)
{
    m_attachments->clear();
    m_attachments->setVisible(false);

    const Message message = messageById(m_currentMessageId);
    const QString from = decodeHeaderValue(parsed.header(QStringLiteral("from")));
    const QString to = decodeHeaderValue(parsed.header(QStringLiteral("to")));
    const QString subject = decodeHeaderValue(parsed.header(QStringLiteral("subject")));
    const QString date = decodeHeaderValue(parsed.header(QStringLiteral("date")));

    QString header;
    header += QStringLiteral("<div style='margin-bottom:6px'><b>%1</b></div>")
                  .arg(subject.toHtmlEscaped());
    if (!from.isEmpty())
        header += QStringLiteral("From: %1<br>").arg(from.toHtmlEscaped());
    if (!to.isEmpty())
        header += QStringLiteral("To: %1<br>").arg(to.toHtmlEscaped());
    if (!date.isEmpty())
        header += QStringLiteral("Date: %1<br>").arg(date.toHtmlEscaped());
    m_readerHeader->setText(header);
    m_readerHeader->setVisible(true);

    const QString html = parsed.bodyHtml();
    if (!html.isEmpty()) {
        m_reader->setHtml(html);
    } else {
        m_reader->setPlainText(parsed.bodyText().isEmpty() ? QString::fromUtf8(raw) : parsed.bodyText());
    }

    const QVector<MimePart> attachments = parsed.attachments();
    if (!attachments.isEmpty()) {
        for (const MimePart& part : attachments)
            m_attachments->addItem(part.filename.isEmpty() ? tr("attachment") : part.filename);
        m_attachments->setVisible(true);
    }

    // The message counts as read in MMC from now on.
    if (!message.flags.contains(QStringLiteral("\\Seen"))) {
        for (Message& stored : m_vault->state().messages) {
            if (stored.id == m_currentMessageId) {
                stored.flags = stored.flags + QStringLiteral(" \\Seen");
                break;
            }
        }
        QString error;
        m_vault->save(&error);
        rebuildMessageList();
    }
}

void MainWindow::onSaveAttachment()
{
    const QListWidgetItem* item = m_attachments->currentItem();
    if (!item || m_currentMessageId.isEmpty())
        return;
    const QByteArray raw = m_bodyCache.value(m_currentMessageId);
    if (raw.isEmpty())
        return;
    const ParsedMessage parsed = parseMime(raw);
    const QVector<MimePart> attachments = parsed.attachments();
    for (const MimePart& part : attachments) {
        if (part.filename != item->text())
            continue;
        const QString path = QFileDialog::getSaveFileName(this, tr("Save attachment"),
                                                          part.filename);
        if (path.isEmpty())
            return;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Cannot write the file"), file.errorString());
            return;
        }
        file.write(part.data);
        return;
    }
}

void MainWindow::onCompose()
{
    if (m_vault->state().mailboxes.isEmpty()) {
        QMessageBox::information(this, tr("No accounts"),
                                 tr("Add an account before you write a message."));
        return;
    }
    ComposeDialog dialog(m_vault->state().mailboxes, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.to().isEmpty()) {
        QMessageBox::warning(this, tr("No recipient"), tr("Who should get this message?"));
        return;
    }
    m_service->sendMessage(dialog.mailboxId(), dialog.to(), dialog.cc(), dialog.subject(),
                           dialog.bodyText(), QString(), dialog.attachmentData(),
                           dialog.attachmentNames());
}

void MainWindow::onSendFinished(bool ok, const QString& error)
{
    if (ok) {
        log(tr("Message sent."));
        return;
    }
    log(tr("Sending failed."));
    QMessageBox::warning(this, tr("The message was not sent"), error);
}

void MainWindow::onSearch()
{
    rebuildMessageList();
}

void MainWindow::onOpenWebmail()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("No account"),
                                 tr("Pick one account in the list first."));
        return;
    }
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox)
        return;
    const QString url = webmailUrlFor(mailbox->providerId);
    if (url.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::onSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Settings"));
    dialog.resize(600, 460);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    form->addRow(new QLabel(tr("The accounts live in the vault:\n%1").arg(m_vault->path()), &dialog));

    for (const QString& providerId : providerIds()) {
        const Provider* provider = providerById(providerId);
        auto* clientId = new QLineEdit(
            m_vault->state().settings.value(providerClientIdKey(providerId)), &dialog);
        auto* clientSecret = new QLineEdit(
            m_vault->state().settings.value(providerClientSecretKey(providerId)), &dialog);
        clientSecret->setEchoMode(QLineEdit::Password);
        clientId->setToolTip(tr("The client id of your %1 application, see "
                                "docs/PROVIDER-SETUP.md").arg(provider->name));
        form->addRow(tr("%1 client id").arg(provider->name), clientId);
        form->addRow(tr("%1 client secret").arg(provider->name), clientSecret);
        clientId->setObjectName(QStringLiteral("clientId.%1").arg(providerId));
        clientSecret->setObjectName(QStringLiteral("clientSecret.%1").arg(providerId));
    }

    form->addRow(new QLabel(tr("Put this address in the application registration:\n%1")
                                 .arg(OAuth2::redirectUriForPort(OAuth2::kDefaultLoopbackPort)),
                             &dialog));

    // A way back in when the master password is lost.
    auto* recoveryButton = new QPushButton(
        m_vault->hasRecovery() ? tr("Replace the recovery passphrase")
                                : tr("Set up a recovery passphrase"), &dialog);
    recoveryButton->setToolTip(tr("A code you write down. With it the vault can be opened again "
                                  "when the master password is forgotten."));
    form->addRow(tr("Recovery"), recoveryButton);
    connect(recoveryButton, &QPushButton::clicked, &dialog, [this, &dialog]() {
        const QString passphrase = Vault::makeRecoveryPassphrase();
        RecoverySetupDialog setup(passphrase, &dialog);
        setup.exec();
        QString error;
        if (!m_vault->createRecovery(passphrase, &error))
            QMessageBox::warning(&dialog, tr("Could not set up the recovery passphrase"), error);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &dialog]() {
        for (const QString& providerId : providerIds()) {
            auto* clientId = dialog.findChild<QLineEdit*>(QStringLiteral("clientId.%1").arg(providerId));
            auto* clientSecret = dialog.findChild<QLineEdit*>(QStringLiteral("clientSecret.%1").arg(providerId));
            if (clientId)
                m_vault->state().settings[providerClientIdKey(providerId)] = clientId->text().trimmed();
            if (clientSecret)
                m_vault->state().settings[providerClientSecretKey(providerId)] = clientSecret->text().trimmed();
        }
        QString error;
        if (!m_vault->save(&error))
            log(error);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

} // namespace mmc

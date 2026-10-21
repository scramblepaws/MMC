#include "ui/MainWindow.h"

#include "core/Importer.h"
#include "core/ProviderIcons.h"
#include "core/Vault.h"
#include "net/MailService.h"
#include "net/Providers.h"
#include "ui/ComposeDialog.h"
#include "ui/EmptyState.h"
#include "ui/Icons.h"
#include "ui/MailboxDialog.h"
#include "ui/AccountManager.h"
#include "ui/MessageList.h"
#include "ui/SettingsDialog.h"
#include "ui/Reader.h"
#include "ui/Sidebar.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "ui/VaultDialogs.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

namespace mmc {
namespace {

QString providerLabelFor(const QString& providerId)
{
    const Provider* provider = providerById(providerId);
    return provider ? provider->name : providerId;
}

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

} // namespace

MainWindow::MainWindow(Vault* vault, QWidget* parent)
    : QMainWindow(parent), m_vault(vault)
{
    setWindowTitle(tr("MMC - %1").arg(vault->name()));
    resize(1440, 900);
    setMinimumSize(1024, 640);
    buildUi();

    m_service = new MailService(m_vault, this);
    connect(m_service, &MailService::syncFinished, this, &MainWindow::onSyncFinished);
    connect(m_service, &MailService::bodyReady, this, &MainWindow::onBodyReady);
    connect(m_service, &MailService::bodyFailed, this, &MainWindow::onBodyFailed);
    connect(m_service, &MailService::sendFinished, this, &MainWindow::onSendFinished);

    rebuildAccounts();
    rebuildMessages();
    m_reader->showEmpty();
}

MainWindow::~MainWindow() = default;

bool MainWindow::event(QEvent* event)
{
    return QMainWindow::event(event);
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- the left column ---------------------------------------------------
    m_sidebar = new Sidebar(m_vault->name(), central);
    connect(m_sidebar, &Sidebar::composeClicked, this, &MainWindow::onCompose);
    connect(m_sidebar, &Sidebar::viewChanged, this, &MainWindow::onViewChanged);
    connect(m_sidebar, &Sidebar::accountSelected, this, &MainWindow::onAccountSelected);
    connect(m_sidebar, &Sidebar::accountContextMenuRequested, this, &MainWindow::onAccountMenu);
    connect(m_sidebar, &Sidebar::addAccountClicked, this, &MainWindow::onAddAccount);
    layout->addWidget(m_sidebar);

    // --- the right of the window -------------------------------------------
    auto* right = new QWidget(central);
    right->setObjectName(QStringLiteral("content"));
    right->setStyleSheet(QStringLiteral("#content { background: %1; }")
                             .arg(Theme::background().name()));
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // The bar on top with the search field and the actions of the program.
    auto* topBar = new QWidget(right);
    topBar->setFixedHeight(58);
    topBar->setStyleSheet(QStringLiteral("QWidget { background: %1; border-bottom: 1px solid %2; }")
                               .arg(Theme::background().name(), Theme::border().name()));
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(18, 10, 14, 10);
    topLayout->setSpacing(8);

    m_search = new QLineEdit(topBar);
    m_search->setPlaceholderText(tr("Search mail (Ctrl+F)"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(260);
    m_search->setMaximumWidth(460);
    m_search->addAction(Icons::icon(QStringLiteral("search"), Theme::textMuted(), 16),
                        QLineEdit::LeadingPosition);
    m_search->setStyleSheet(QStringLiteral("QLineEdit { background: %1; border: 1px solid %2; }")
                                .arg(Theme::surface().name(), Theme::border().name()));
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
    topLayout->addWidget(m_search);

    m_cooldown = new QLabel(topBar);
    m_cooldown->setFont(Theme::font(12));
    m_cooldown->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                  .arg(Theme::warning().name()));
    topLayout->addWidget(m_cooldown);
    topLayout->addStretch(1);

    auto makeButton = [&](const QString& icon, const QString& tip) {
        auto* button = new QToolButton(topBar);
        button->setAccessibleName(tip);
        button->setIcon(Icons::icon(icon, Theme::textSecondary(), 18));
        button->setIconSize(QSize(18, 18));
        button->setFixedSize(34, 34);
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral("QToolButton { border: none; border-radius: 8px; }"
                                            "QToolButton:hover { background: %1; }")
                                  .arg(Theme::surfaceHover().name()));
        return button;
    };

    auto* accountsButton = makeButton(QStringLiteral("user"), tr("Manage the accounts"));
    connect(accountsButton, &QToolButton::clicked, this, &MainWindow::onAccountManager);
    topLayout->addWidget(accountsButton);

    auto* refreshButton = makeButton(QStringLiteral("refresh"), tr("Refresh every account (Ctrl+R)"));
    connect(refreshButton, &QToolButton::clicked, this, &MainWindow::onRefreshAll);
    topLayout->addWidget(refreshButton);

    auto* writeButton = makeButton(QStringLiteral("compose"), tr("Write a message (Ctrl+N)"));
    connect(writeButton, &QToolButton::clicked, this, &MainWindow::onCompose);
    topLayout->addWidget(writeButton);

    auto* settingsButton = makeButton(QStringLiteral("settings"), tr("Settings"));
    connect(settingsButton, &QToolButton::clicked, this, &MainWindow::onSettings);
    topLayout->addWidget(settingsButton);

    rightLayout->addWidget(topBar);

    // The mail and the reader.
    auto* splitter = new QSplitter(Qt::Horizontal, right);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QStringLiteral("QSplitter::handle { background: %1; }")
                                .arg(Theme::border().name()));

    m_messages = new MessageList(splitter);
    m_messages->setStyleSheet(QStringLiteral("QWidget { background: %1; }")
                                  .arg(Theme::background().name()));
    connect(m_messages, &MessageList::messageActivated, this, &MainWindow::onMessageActivated);
    connect(m_messages, &MessageList::refreshRequested, this, &MainWindow::onRefreshAccount);
    connect(m_messages, &MessageList::markReadRequested, this, &MainWindow::onMarkRead);
    connect(m_messages, &MessageList::deleteRequested, this, &MainWindow::onDeleteMessage);
    connect(m_messages, &MessageList::starToggled, this, &MainWindow::onStarToggled);
    connect(m_messages, &MessageList::filterChanged, this, &MainWindow::onFilterChanged);
    splitter->addWidget(m_messages);

    m_reader = new Reader(splitter);
    connect(m_reader, &Reader::retryRequested, this, [this]() {
        if (!m_currentMessageId.isEmpty())
            m_service->fetchBody(m_currentMessageId);
    });
    connect(m_reader, &Reader::replyRequested, this, &MainWindow::onReply);
    connect(m_reader, &Reader::forwardRequested, this, &MainWindow::onCompose);
    connect(m_reader, &Reader::starToggled, this, [this](bool starred) {
        if (!m_currentMessageId.isEmpty())
            onStarToggled(m_currentMessageId, starred);
    });
    connect(m_reader, &Reader::deleteRequested, this, &MainWindow::onDeleteMessage);
    splitter->addWidget(m_reader);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({440, 1000});
    rightLayout->addWidget(splitter, 1);

    layout->addWidget(right, 1);

    // The toasts live above everything in the bottom right corner.
    m_toasts = new QStackedWidget(this);
    m_toasts->setFixedWidth(380);

    m_status = new QLabel(this);
    m_status->setFont(Theme::font(11));
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textMuted().name()));
    statusBar()->addWidget(m_status);
    statusBar()->setStyleSheet(QStringLiteral("QStatusBar { background: %1; border-top: 1px solid %2; }")
                                   .arg(Theme::background().name(), Theme::border().name()));

    // Shortcuts.
    auto* refresh = new QAction(this);
    refresh->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(refresh, &QAction::triggered, this, &MainWindow::onRefreshAll);
    addAction(refresh);

    auto* write = new QAction(this);
    write->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(write, &QAction::triggered, this, &MainWindow::onCompose);
    addAction(write);

    auto* find = new QAction(this);
    find->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(find, &QAction::triggered, this, [this]() {
        m_search->setFocus();
        m_search->selectAll();
    });
    addAction(find);

    auto* refreshOne = new QAction(this);
    refreshOne->setShortcut(QKeySequence(Qt::Key_F5));
    connect(refreshOne, &QAction::triggered, this, &MainWindow::onRefreshAccount);
    addAction(refreshOne);
}

void MainWindow::log(const QString& text)
{
    m_status->setText(text);
}

void MainWindow::toast(const QString& title, const QString& text, Toast::Kind kind)
{
    auto* card = new Toast(kind, title, text, this);
    card->adjustSize();
    card->move(width() - card->width() - 24, height() - card->height() - 44);
    card->show();
    card->showFor(kind == Toast::Kind::Error ? 12000 : 5000);
    card->raise();
    connect(card, &QObject::destroyed, this, [this, card]() { card->deleteLater(); });
}


void MainWindow::rebuildAccounts()
{
    QList<Mailbox*> accounts;
    for (Mailbox& mailbox : m_vault->state().mailboxes)
        accounts << &mailbox;
    m_sidebar->setAccounts(accounts);
    for (const Mailbox& mailbox : m_vault->state().mailboxes) {
        const int unseen = m_vault->state()
                               .settings.value(QStringLiteral("unseen.%1").arg(mailbox.id))
                               .toInt();
        m_sidebar->setUnread(mailbox.id, unseen);
    }
}

QVector<Message> MainWindow::messagesForView() const
{
    QVector<Message> out;
    for (const Message& message : m_vault->state().messages) {
        if (!m_accountFilter.isEmpty() && message.mailboxId != m_accountFilter)
            continue;
        const QString kind = message.folderKind.isEmpty() ? QStringLiteral("inbox")
                                                          : message.folderKind;
        switch (m_view) {
        case Sidebar::View::Inbox:
            if (kind != QLatin1String("inbox"))
                continue;
            break;
        case Sidebar::View::Sent:
            if (kind != QLatin1String("sent"))
                continue;
            break;
        case Sidebar::View::Drafts:
            if (kind != QLatin1String("drafts"))
                continue;
            break;
        case Sidebar::View::Spam:
            if (kind != QLatin1String("junk"))
                continue;
            break;
        case Sidebar::View::Trash:
            if (kind != QLatin1String("trash"))
                continue;
            break;
        default:
            break;
        }
        out << message;
    }
    std::sort(out.begin(), out.end(), [](const Message& a, const Message& b) {
        return a.date > b.date;
    });
    return out;
}

void MainWindow::rebuildMessages()
{
    QHash<QString, Mailbox> mailboxes;
    for (const Mailbox& mailbox : m_vault->state().mailboxes)
        mailboxes.insert(mailbox.id, mailbox);

    m_messages->setAccountFilter(m_accountFilter);
    m_messages->setSearchText(m_searchText);
    m_messages->setMessages(messagesForView(), mailboxes);
}

void MainWindow::onViewChanged(Sidebar::View view)
{
    m_view = view;
    if (view != Sidebar::View::AllAccounts && view != Sidebar::View::Inbox)
        fetchFolderForView();
    rebuildMessages();
}

void MainWindow::fetchFolderForView()
{
    static const QHash<int, QString> kinds = {
        {int(Sidebar::View::Sent), QStringLiteral("sent")},
        {int(Sidebar::View::Drafts), QStringLiteral("drafts")},
        {int(Sidebar::View::Spam), QStringLiteral("junk")},
        {int(Sidebar::View::Trash), QStringLiteral("trash")},
    };
    const QString kind = kinds.value(int(m_view));
    if (kind.isEmpty())
        return;

    // The folder of the account is fetched when it is looked at, so a hundred
    // accounts do not mean a hundred fetches on every refresh.
    QStringList mailboxes;
    if (!m_accountFilter.isEmpty())
        mailboxes << m_accountFilter;
    else
        for (const Mailbox& mailbox : m_vault->state().mailboxes)
            mailboxes << mailbox.id;

    for (const QString& mailboxId : mailboxes) {
        for (const Folder& folder : m_vault->state().folders) {
            if (folder.mailboxId == mailboxId && folder.kind == kind) {
                m_service->syncFolder(mailboxId, folder.name, kind);
                break;
            }
        }
    }
}

void MainWindow::onAccountSelected(const QString& mailboxId)
{
    m_accountFilter = mailboxId;
    rebuildMessages();
}

void MainWindow::onFilterChanged(MessageList::Filter filter)
{
    Q_UNUSED(filter)
    rebuildMessages();
}

void MainWindow::onSearchChanged(const QString& text)
{
    m_searchText = text;
    m_messages->setSearchText(text);
}

void MainWindow::onMessageActivated(const QString& messageId)
{
    if (messageId.isEmpty())
        return;
    m_currentMessageId = messageId;

    if (m_bodyCache.contains(messageId)) {
        showMessage(messageById(messageId));
        return;
    }
    m_reader->showEmpty();
    m_service->fetchBody(messageId);
}

void MainWindow::onBodyReady(const QString& messageId, const QByteArray& raw)
{
    m_bodyCache.insert(messageId, raw);
    if (messageId != m_currentMessageId)
        return;
    showMessage(messageById(messageId));
}

void MainWindow::onBodyFailed(const QString& messageId, const QString& error)
{
    if (messageId != m_currentMessageId)
        return;
    m_reader->showError(tr("Unable to load this message"),
                        tr("We could not get this mail. Check the connection of the account "
                           "and try again."),
                        [this, messageId]() { m_service->fetchBody(messageId); });
    log(error);
}

void MainWindow::showMessage(const Message& message)
{
    if (m_currentMessageId.isEmpty())
        return;
    const QByteArray raw = m_bodyCache.value(m_currentMessageId);
    if (raw.isEmpty())
        return;
    m_reader->showMessage(message, parseMime(raw));

    // The mail counts as read from now on.
    if (!message.flags.contains(QStringLiteral("\\Seen"))) {
        for (Message& stored : m_vault->state().messages) {
            if (stored.id == m_currentMessageId) {
                stored.flags = stored.flags + QStringLiteral(" \\Seen");
                break;
            }
        }
        QString error;
        m_vault->save(&error);
        rebuildMessages();
    }
}

void MainWindow::onStarToggled(const QString& messageId, bool starred)
{
    m_starred.insert(messageId, starred);
    rebuildMessages();
}

void MainWindow::onMarkRead()
{
    if (m_currentMessageId.isEmpty())
        return;
    for (Message& stored : m_vault->state().messages) {
        if (stored.id == m_currentMessageId) {
            if (!stored.flags.contains(QStringLiteral("\\Seen")))
                stored.flags = stored.flags + QStringLiteral(" \\Seen");
            break;
        }
    }
    QString error;
    m_vault->save(&error);
    rebuildMessages();
}

void MainWindow::onDeleteMessage()
{
    if (m_currentMessageId.isEmpty())
        return;
    const QString id = m_currentMessageId;
    for (int i = 0; i < m_vault->state().messages.size(); ++i) {
        if (m_vault->state().messages.at(i).id == id) {
            m_vault->state().messages.remove(i);
            break;
        }
    }
    QString error;
    m_vault->save(&error);
    m_currentMessageId.clear();
    m_reader->showEmpty();
    rebuildMessages();
    toast(tr("Message moved to the trash"), tr("You can get it back from the trash."),
          Toast::Kind::Information);
}

void MainWindow::onRefreshAll()
{
    log(tr("Refreshing every account..."));
    m_messages->setBusy(true);
    m_service->syncAll();
}

void MainWindow::onRefreshAccount()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        onRefreshAll();
        return;
    }
    m_messages->setBusy(true);
    m_service->syncMailbox(id);
}

void MainWindow::onSyncFinished(const QString& mailboxId, bool ok, const QString& error)
{
    m_messages->setBusy(false);
    if (!ok) {
        const Mailbox* mailbox = m_vault->state().mailboxById(mailboxId);
        toast(tr("Could not refresh"), tr("%1: %2").arg(mailbox ? mailbox->address : mailboxId, error),
              Toast::Kind::Error);
        return;
    }
    rebuildAccounts();
    rebuildMessages();
    log(tr("Refreshed."));
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
        toast(tr("The account was added, the sign in failed"), dialog.signInError(),
              Toast::Kind::Warning);
        return;
    }

    const bool hasToken = m_vault->state().tokenFor(mailbox.id) != nullptr;
    if (!hasToken && mailbox.secret.isEmpty()) {
        toast(tr("The account was added"),
              tr("%1 cannot read mail yet. Put the client id of your %2 application in the "
                 "settings, then sign in to the account again.")
                  .arg(mailbox.address, providerLabelFor(mailbox.providerId)),
              Toast::Kind::Information);
        return;
    }
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
    showCooldownStatus();

    if (startedCooldown) {
        toast(tr("Cooldown started"),
              tr("The details of %1 cannot be changed until %2.")
                  .arg(updated.displayName.isEmpty() ? updated.address : updated.displayName,
                       updated.cooldownUntil.toString(QStringLiteral("dd MMM yyyy"))),
              Toast::Kind::Information);
    }
    if (!dialog.signInError().isEmpty())
        toast(tr("The sign in failed"), dialog.signInError(), Toast::Kind::Error);
}

void MainWindow::onSignInAgain()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty())
        return;
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
        toast(tr("Signed in"), tr("%1 is connected.").arg(mailbox->address), Toast::Kind::Success);
    }
    if (!dialog.signInError().isEmpty())
        toast(tr("The sign in failed"), dialog.signInError(), Toast::Kind::Error);
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
                              tr("Remove %1 from MMC?\n\nThe account itself is not deleted at the "
                                 "provider, but the mail that was read is removed from the vault.")
                                  .arg(mailbox->address))
        != QMessageBox::Yes)
        return;

    m_vault->state().removeMailbox(id);
    QString error;
    m_vault->save(&error);
    m_accountFilter.clear();
    rebuildAccounts();
    rebuildMessages();
}

void MainWindow::onAccountMenu(const QString& mailboxId, const QPoint& position)
{
    QMenu menu(this);
    QAction* edit = menu.addAction(tr("Edit account"));
    QAction* signIn = menu.addAction(tr("Sign in again"));
    QAction* refresh = menu.addAction(tr("Refresh this account"));
    QAction* webmail = menu.addAction(tr("Open webmail"));
    menu.addSeparator();
    QAction* remove = menu.addAction(tr("Remove account"));

    QAction* chosen = menu.exec(position);
    if (!chosen)
        return;

    m_accountFilter = mailboxId;
    m_sidebar->setSelectedAccountId(mailboxId);
    if (chosen == edit)
        onEditAccount();
    else if (chosen == signIn)
        onSignInAgain();
    else if (chosen == refresh)
        onRefreshAccount();
    else if (chosen == webmail)
        onOpenWebmail();
    else if (chosen == remove)
        onRemoveAccount();
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
    m_vault->save(&error);
    rebuildAccounts();
    toast(tr("Accounts imported"), tr("%1 accounts were added.").arg(result.mailboxes.size()),
          Toast::Kind::Success);
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

void MainWindow::onReply()
{
    if (m_vault->state().mailboxes.isEmpty())
        return;
    const Message message = messageById(m_currentMessageId);
    if (message.id.isEmpty())
        return;

    ComposeDialog dialog(m_vault->state().mailboxes, this);
    dialog.setRecipient(message.from);
    dialog.setSubject(message.subject.startsWith(QStringLiteral("Re:"), Qt::CaseInsensitive)
                          ? message.subject
                          : QStringLiteral("Re: ") + message.subject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_service->sendMessage(dialog.mailboxId(), dialog.to(), dialog.cc(), dialog.subject(),
                           dialog.bodyText(), QString(), dialog.attachmentData(),
                           dialog.attachmentNames());
}

void MainWindow::onSendFinished(bool ok, const QString& error)
{
    if (ok) {
        toast(tr("Message sent"), tr("The mail is on its way."), Toast::Kind::Success);
        return;
    }
    toast(tr("The message was not sent"), error, Toast::Kind::Error);
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
    if (!url.isEmpty())
        QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::onAccountManager()
{
    AccountManager dialog(m_vault, this);
    connect(&dialog, &AccountManager::accountChanged, this, [this]() {
        rebuildAccounts();
        rebuildMessages();
    });
    dialog.exec();
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(m_vault, this);
    dialog.exec();
    rebuildAccounts();
    rebuildMessages();
}

void MainWindow::showCooldownStatus()
{
    const Mailbox* mailbox = m_vault->state().mailboxById(m_accountFilter);
    if (!mailbox || !mailbox->cooldownUntil.isValid()) {
        m_cooldown->clear();
        return;
    }
    const qint64 seconds = QDateTime::currentDateTime().secsTo(mailbox->cooldownUntil);
    if (seconds <= 0) {
        m_cooldown->setText(tr("%1 can be changed again").arg(mailbox->displayName));
        return;
    }
    m_cooldown->setText(tr("%1 is on cooldown, %2 day(s) left")
                            .arg(mailbox->displayName)
                            .arg(seconds / 86400));
}

QString MainWindow::selectedMailboxId() const
{
    return m_accountFilter;
}


Message MainWindow::messageById(const QString& id) const
{
    for (const Message& message : m_vault->state().messages)
        if (message.id == id)
            return message;
    return Message();
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
    rebuildAccounts();
    rebuildMessages();
}

} // namespace mmc

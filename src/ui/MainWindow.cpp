#include "ui/MainWindow.h"

#include "core/Importer.h"
#include "core/Vault.h"
#include "net/MailService.h"
#include "net/Mime.h"
#include "net/Providers.h"
#include "ui/ComposeDialog.h"
#include "ui/MailboxDialog.h"
#include "ui/VaultDialogs.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
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

} // namespace

MainWindow::MainWindow(Vault* vault, QWidget* parent)
    : QMainWindow(parent), m_vault(vault)
{
    setWindowTitle(tr("MMC - %1").arg(vault->name()));
    resize(1280, 800);
    buildUi();

    m_service = new MailService(m_vault, this);
    connect(m_service, &MailService::syncFinished, this, &MainWindow::onSyncFinished);
    connect(m_service, &MailService::bodyReady, this, &MainWindow::onBodyReady);
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

    auto* refreshAll = new QAction(tr("Refresh all"), this);
    connect(refreshAll, &QAction::triggered, this, &MainWindow::onRefreshAll);
    toolbar->addAction(refreshAll);

    auto* refreshOne = new QAction(tr("Refresh account"), this);
    connect(refreshOne, &QAction::triggered, this, &MainWindow::onRefreshAccount);
    toolbar->addAction(refreshOne);

    toolbar->addSeparator();

    auto* compose = new QAction(tr("Write"), this);
    connect(compose, &QAction::triggered, this, &MainWindow::onCompose);
    toolbar->addAction(compose);

    auto* webmail = new QAction(tr("Open webmail"), this);
    connect(webmail, &QAction::triggered, this, &MainWindow::onOpenWebmail);
    toolbar->addAction(webmail);

    toolbar->addSeparator();

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search the loaded mail"));
    m_search->setMaximumWidth(280);
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::returnPressed, this, &MainWindow::onSearch);
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

    // Left: the accounts.
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel(tr("Accounts"), left));

    m_accounts = new QListWidget(left);
    m_accounts->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_accounts, 1);

    m_cooldown = new QLabel(left);
    m_cooldown->setWordWrap(true);
    leftLayout->addWidget(m_cooldown);

    auto* accountButtons = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Add"), left);
    auto* editButton = new QPushButton(tr("Edit"), left);
    auto* removeButton = new QPushButton(tr("Remove"), left);
    auto* importButton = new QPushButton(tr("Import"), left);
    accountButtons->addWidget(addButton);
    accountButtons->addWidget(editButton);
    accountButtons->addWidget(removeButton);
    accountButtons->addWidget(importButton);
    leftLayout->addLayout(accountButtons);

    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddAccount);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::onEditAccount);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveAccount);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::onImportAccounts);
    connect(m_accounts, &QListWidget::itemSelectionChanged, this, &MainWindow::onAccountSelectionChanged);

    // Middle: the message list.
    auto* middle = new QWidget(splitter);
    auto* middleLayout = new QVBoxLayout(middle);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    m_messages = new QTableWidget(middle);
    m_messages->setColumnCount(5);
    m_messages->setHorizontalHeaderLabels({tr("Account"), tr("From"), tr("Subject"), tr("Date"), tr("")});
    m_messages->horizontalHeader()->setStretchLastSection(false);
    m_messages->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_messages->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_messages->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_messages->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_messages->verticalHeader()->setVisible(false);
    m_messages->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_messages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_messages->setShowGrid(false);
    middleLayout->addWidget(m_messages);
    connect(m_messages, &QTableWidget::itemSelectionChanged, this, &MainWindow::onMessageSelectionChanged);

    // Right: the reading pane.
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_reader = new QTextBrowser(right);
    m_reader->setOpenExternalLinks(true);
    m_reader->setOpenLinks(false);
    rightLayout->addWidget(m_reader, 1);

    m_attachments = new QListWidget(right);
    m_attachments->setMaximumHeight(100);
    m_attachments->setToolTip(tr("Attachments of the selected message"));
    rightLayout->addWidget(m_attachments);

    auto* saveAttachment = new QPushButton(tr("Save attachment"), right);
    rightLayout->addWidget(saveAttachment);
    connect(saveAttachment, &QPushButton::clicked, this, &MainWindow::onSaveAttachment);

    splitter->addWidget(left);
    splitter->addWidget(middle);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 3);
    splitter->setSizes({240, 600, 640});
    layout->addWidget(splitter);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status);
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

    for (const Mailbox& mailbox : m_vault->state().mailboxes) {
        const int unseen = m_vault->state()
                               .settings.value(QStringLiteral("unseen.%1").arg(mailbox.id))
                               .toInt();
        QString text = mailbox.displayName.isEmpty() ? mailbox.address : mailbox.displayName;
        if (unseen > 0)
            text += QStringLiteral("  (%1)").arg(unseen);
        if (mailbox.cooldownUntil.isValid() && mailbox.cooldownUntil > QDateTime::currentDateTime())
            text += QStringLiteral("  [cooldown]");

        auto* item = new QListWidgetItem(text, m_accounts);
        item->setData(Qt::UserRole, mailbox.id);
        item->setToolTip(mailbox.address);
        if (current == mailbox.id)
            m_accounts->setCurrentItem(item);
    }
    if (!m_accounts->currentItem() && m_accounts->count() > 0)
        m_accounts->setCurrentRow(0);
    showCooldownStatus();
}

void MainWindow::rebuildMessageList()
{
    const QString filter = m_search->text().trimmed();
    const QString onlyAccount = selectedMailboxId();

    m_messages->setRowCount(0);

    QVector<Message> messages = m_vault->state().messages;
    std::sort(messages.begin(), messages.end(), [](const Message& a, const Message& b) {
        return a.date > b.date;
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
        const QString account = mailbox ? (mailbox->displayName.isEmpty() ? mailbox->address : mailbox->displayName)
                                        : message.mailboxId;

        m_messages->insertRow(row);
        auto* accountItem = new QTableWidgetItem(account);
        accountItem->setData(Qt::UserRole, message.id);
        m_messages->setItem(row, 0, accountItem);

        auto* fromItem = new QTableWidgetItem(decodeHeaderValue(message.from));
        fromItem->setData(Qt::UserRole, message.id);
        m_messages->setItem(row, 1, fromItem);

        auto* subjectItem = new QTableWidgetItem(decodeHeaderValue(message.subject));
        subjectItem->setData(Qt::UserRole, message.id);
        m_messages->setItem(row, 2, subjectItem);

        m_messages->setItem(row, 3, new QTableWidgetItem(formatDate(message.date)));
        m_messages->setItem(row, 4, new QTableWidgetItem(QString()));
        row++;
    }
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

void MainWindow::onAddAccount()
{
    MailboxDialog dialog(m_vault, Mailbox(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    Mailbox mailbox = dialog.mailbox();
    m_vault->state().mailboxes.append(mailbox);
    QString error;
    if (!m_vault->save(&error))
        log(error);
    rebuildAccountList();
    rebuildMessageList();
}

void MainWindow::onEditAccount()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty())
        return;
    const Mailbox* mailbox = m_vault->state().mailboxById(id);
    if (!mailbox)
        return;

    MailboxDialog dialog(m_vault, *mailbox, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const Mailbox updated = dialog.mailbox();

    for (Mailbox& existing : m_vault->state().mailboxes) {
        if (existing.id == id) {
            if (!existing.cooldownUntil.isValid() && updated.cooldownUntil.isValid()) {
                QMessageBox::information(
                    this, tr("Cooldown started"),
                    tr("The details of %1 cannot be changed until %2.")
                        .arg(updated.displayName.isEmpty() ? updated.address : updated.displayName,
                             updated.cooldownUntil.toString(QStringLiteral("dd MMM yyyy"))));
            }
            existing = updated;
            break;
        }
    }
    QString error;
    if (!m_vault->save(&error))
        log(error);
    rebuildAccountList();
    showCooldownStatus();
}

void MainWindow::onRemoveAccount()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty())
        return;
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
    m_reader->setText(tr("Loading the message..."));
    m_service->fetchBody(id);
}

void MainWindow::onBodyReady(const QString& messageId, const QByteArray& raw)
{
    m_bodyCache.insert(messageId, raw);
    if (messageId != m_currentMessageId)
        return;
    showMessageBody(parseMime(raw), raw);
}

void MainWindow::showMessageBody(const ParsedMessage& parsed, const QByteArray& raw)
{
    m_attachments->clear();

    const QString html = parsed.bodyHtml();
    if (!html.isEmpty()) {
        m_reader->setHtml(html);
    } else {
        m_reader->setPlainText(parsed.bodyText().isEmpty() ? QString::fromUtf8(raw) : parsed.bodyText());
    }

    const QVector<MimePart> attachments = parsed.attachments();
    for (const MimePart& part : attachments)
        m_attachments->addItem(part.filename.isEmpty() ? tr("attachment") : part.filename);
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
    log(tr("Searched the loaded mail."));
}

void MainWindow::onOpenWebmail()
{
    const QString id = selectedMailboxId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("No account"), tr("Pick an account first."));
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
    dialog.resize(560, 420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    form->addRow(new QLabel(tr("Your accounts are stored in the vault:\n%1").arg(m_vault->path()), &dialog));

    for (const QString& providerId : providerIds()) {
        const Provider* provider = providerById(providerId);
        auto* clientId = new QLineEdit(
            m_vault->state().settings.value(providerClientIdKey(providerId)), &dialog);
        auto* clientSecret = new QLineEdit(
            m_vault->state().settings.value(providerClientSecretKey(providerId)), &dialog);
        clientSecret->setEchoMode(QLineEdit::Password);
        form->addRow(tr("%1 client id").arg(provider->name), clientId);
        form->addRow(tr("%1 client secret").arg(provider->name), clientSecret);
        clientId->setObjectName(QStringLiteral("clientId.%1").arg(providerId));
        clientSecret->setObjectName(QStringLiteral("clientSecret.%1").arg(providerId));
    }

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

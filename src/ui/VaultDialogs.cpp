#include "ui/VaultDialogs.h"

#include "core/Vault.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace mmc {

VaultChooserDialog::VaultChooserDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("MMC - Open a vault"));
    resize(560, 400);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Your accounts live in a vault. A vault is one encrypted "
                                     "file, you can put it on an encrypted USB stick and use "
                                     "it on any computer."), this));

    m_list = new QListWidget(this);
    layout->addWidget(m_list);

    const QStringList vaults = Vault::listVaults();
    for (const QString& path : vaults) {
        auto* item = new QListWidgetItem(QFileInfo(path).fileName(), m_list);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
    if (m_list->count() == 0) {
        auto* empty = new QListWidgetItem(tr("No vaults yet, create one to get started."), m_list);
        empty->setFlags(Qt::NoItemFlags);
    }

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* createButton = buttons->addButton(tr("New vault"), QDialogButtonBox::ActionRole);
    QPushButton* browseButton = buttons->addButton(tr("Open file..."), QDialogButtonBox::ActionRole);
    m_openButton = buttons->addButton(tr("Open"), QDialogButtonBox::AcceptRole);
    m_openButton->setEnabled(false);
    layout->addWidget(buttons);

    connect(createButton, &QPushButton::clicked, this, &VaultChooserDialog::onNewVault);
    connect(browseButton, &QPushButton::clicked, this, &VaultChooserDialog::onOpenFile);
    connect(m_openButton, &QPushButton::clicked, this, &VaultChooserDialog::onOpen);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &VaultChooserDialog::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &VaultChooserDialog::onOpen);
}

void VaultChooserDialog::onSelectionChanged()
{
    const auto items = m_list->selectedItems();
    m_selectedPath = items.isEmpty() ? QString() : items.first()->data(Qt::UserRole).toString();
    m_openButton->setEnabled(!m_selectedPath.isEmpty());
}

void VaultChooserDialog::onNewVault()
{
    QDir().mkpath(Vault::defaultVaultDir());
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Where should the vault live?"),
        Vault::defaultVaultDir() + QStringLiteral("/mail.mmcv"),
        tr("MMC vault (*.mmcv)"));
    if (path.isEmpty())
        return;

    MasterPasswordDialog dialog(tr("Choose a master password"), true);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Vault vault;
    QString error;
    if (!vault.create(path, dialog.password(), &error)) {
        QMessageBox::critical(this, tr("Could not create the vault"), error);
        return;
    }

    // A recovery passphrase, so the vault can be opened again when the master
    // password is lost.
    if (QMessageBox::question(this, tr("A way back in"),
                               tr("Set up a recovery passphrase?\n\n"
                                  "It is a code you write down. With it the vault can be opened "
                                  "again when the master password is forgotten. Without it the "
                                  "accounts are gone for good."))
        == QMessageBox::Yes) {
        const QString passphrase = Vault::makeRecoveryPassphrase();
        RecoverySetupDialog setup(passphrase, this);
        setup.exec();
        if (!vault.createRecovery(passphrase, &error)) {
            QMessageBox::warning(this, tr("Could not set up the recovery passphrase"), error);
        }
    }

    m_selectedPath = path;
    m_createdPassword = dialog.password();
    accept();
}

void VaultChooserDialog::onOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open a vault"), Vault::defaultVaultDir(),
        tr("MMC vault (*.mmcv);;All files (*)"));
    if (path.isEmpty())
        return;
    m_selectedPath = path;
    accept();
}

void VaultChooserDialog::onOpen()
{
    if (m_selectedPath.isEmpty())
        return;
    accept();
}

MasterPasswordDialog::MasterPasswordDialog(const QString& title, bool confirm,
                                           const QString& vaultPath, QWidget* parent)
    : QDialog(parent), m_askConfirm(confirm), m_vaultPath(vaultPath)
{
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("The master password unlocks the vault. It is never stored, "
                                     "so if you forget it the accounts are gone."), this));

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Master password"));
    layout->addWidget(m_password);

    if (confirm) {
        m_confirmField = new QLineEdit(this);
        m_confirmField->setEchoMode(QLineEdit::Password);
        m_confirmField->setPlaceholderText(tr("Repeat the password"));
        layout->addWidget(m_confirmField);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    // The way back in, when the master password is lost.
    if (!m_vaultPath.isEmpty()) {
        auto* forgot = buttons->addButton(tr("Forgot the master password?"),
                                          QDialogButtonBox::ActionRole);
        connect(forgot, &QPushButton::clicked, this, &MasterPasswordDialog::onForgotPassword);
    }

    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_password->text().isEmpty()) {
            QMessageBox::warning(this, tr("No password"), tr("The master password cannot be empty."));
            return;
        }
        if (m_askConfirm && m_confirmField->text() != m_password->text()) {
            QMessageBox::warning(this, tr("Not the same"),
                                 tr("The two passwords are not the same."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_password->setFocus();
}

void MasterPasswordDialog::onForgotPassword()
{
    if (!Vault::vaultHasRecovery(m_vaultPath)) {
        QMessageBox::information(
            this, tr("No way back in"),
            tr("This vault was made without a recovery passphrase, so the master password "
               "cannot be reset. The accounts are locked behind it.\n\n"
               "The next vault you make can have one, MMC asks for it when you create it."));
        return;
    }

    ResetPasswordDialog reset(m_vaultPath, this);
    if (reset.exec() != QDialog::Accepted)
        return;
    m_resetPassword = reset.newPassword();
    m_password->setText(m_resetPassword);
    accept();
}

QString MasterPasswordDialog::password() const
{
    return m_password->text();
}

RecoverySetupDialog::RecoverySetupDialog(const QString& passphrase, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Your recovery passphrase"));
    resize(520, 240);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Write this down and keep it somewhere safe. It is the only "
                                     "way into the vault when the master password is "
                                     "forgotten."), this));

    auto* field = new QLineEdit(passphrase, this);
    field->setReadOnly(true);
    field->setFont(QFont(QStringLiteral("Consolas"), 12));
    layout->addWidget(field);

    auto* copyButton = new QPushButton(tr("Copy to the clipboard"), this);
    layout->addWidget(copyButton);
    connect(copyButton, &QPushButton::clicked, this, [field]() {
        QApplication::clipboard()->setText(field->text());
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("I wrote it down"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

ResetPasswordDialog::ResetPasswordDialog(const QString& vaultPath, QWidget* parent)
    : QDialog(parent), m_vaultPath(vaultPath)
{
    setWindowTitle(tr("Open the vault with the recovery passphrase"));
    resize(520, 320);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Type the recovery passphrase of this vault and choose a new "
                                     "master password."), this));

    auto* form = new QFormLayout();
    layout->addLayout(form);

    m_recovery = new QLineEdit(this);
    m_recovery->setPlaceholderText(tr("Recovery passphrase"));
    m_recovery->setFont(QFont(QStringLiteral("Consolas"), 10));
    form->addRow(tr("Recovery passphrase"), m_recovery);

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("New master password"));
    form->addRow(tr("New master password"), m_password);

    m_confirmField = new QLineEdit(this);
    m_confirmField->setEchoMode(QLineEdit::Password);
    m_confirmField->setPlaceholderText(tr("Repeat the password"));
    form->addRow(tr("Repeat it"), m_confirmField);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Open the vault"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ResetPasswordDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_recovery->setFocus();
}

void ResetPasswordDialog::onAccept()
{
    if (m_recovery->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("No passphrase"),
                             tr("Type the recovery passphrase of the vault."));
        return;
    }
    if (m_password->text().isEmpty()) {
        QMessageBox::warning(this, tr("No password"),
                             tr("Choose a new master password."));
        return;
    }
    if (m_password->text() != m_confirmField->text()) {
        QMessageBox::warning(this, tr("Not the same"),
                             tr("The two passwords are not the same."));
        return;
    }

    QString error;
    if (!Vault::resetMasterPassword(m_vaultPath, m_recovery->text(), m_password->text(), &error)) {
        QMessageBox::warning(this, tr("Could not open the vault"), error);
        return;
    }
    m_newPassword = m_password->text();
    accept();
}

} // namespace mmc

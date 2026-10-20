#include "ui/VaultDialogs.h"

#include "core/Vault.h"

#include <QDialogButtonBox>
#include <QFileDialog>
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

    MasterPasswordDialog dialog(tr("Choose a master password"), true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Vault vault;
    QString error;
    if (!vault.create(path, dialog.password(), &error)) {
        QMessageBox::critical(this, tr("Could not create the vault"), error);
        return;
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

MasterPasswordDialog::MasterPasswordDialog(const QString& title, bool confirm, QWidget* parent)
    : QDialog(parent), m_askConfirm(confirm)
{
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("The master password unlocks the vault. It is never stored, "
                                     "so if you forget it the accounts are gone."), this));

    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Master password"));
    layout->addWidget(m_password);

    if (m_askConfirm) {
        m_confirmField = new QLineEdit(this);
        m_confirmField->setEchoMode(QLineEdit::Password);
        m_confirmField->setPlaceholderText(tr("Repeat the password"));
        layout->addWidget(m_confirmField);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
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

QString MasterPasswordDialog::password() const
{
    return m_password->text();
}

} // namespace mmc

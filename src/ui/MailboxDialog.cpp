#include "ui/MailboxDialog.h"

#include "core/Vault.h"
#include "net/OAuth2.h"
#include "net/Providers.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUuid>
#include <QVBoxLayout>

namespace mmc {

MailboxDialog::MailboxDialog(Vault* vault, const Mailbox& mailbox, QWidget* parent)
    : QDialog(parent), m_vault(vault), m_mailbox(mailbox)
{
    m_isNew = mailbox.id.isEmpty();
    if (m_isNew) {
        // Give the account its id right away, the sign in token is bound to it.
        m_mailbox.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    setWindowTitle(m_isNew ? tr("Add an account") : tr("Edit account"));
    resize(520, 420);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    m_address = new QLineEdit(m_mailbox.address, this);
    m_address->setPlaceholderText(tr("name@gmail.com"));
    form->addRow(tr("Address"), m_address);

    m_provider = new QComboBox(this);
    for (const QString& id : providerIds()) {
        const Provider* provider = providerById(id);
        m_provider->addItem(provider->name, id);
    }
    if (!m_mailbox.providerId.isEmpty())
        m_provider->setCurrentIndex(m_provider->findData(m_mailbox.providerId));
    form->addRow(tr("Provider"), m_provider);

    m_displayName = new QLineEdit(m_mailbox.displayName, this);
    m_displayName->setPlaceholderText(tr("Optional, shown in the account list"));
    form->addRow(tr("Name"), m_displayName);

    m_secret = new QLineEdit(QString::fromUtf8(m_mailbox.secret), this);
    m_secret->setEchoMode(QLineEdit::Password);
    m_secret->setPlaceholderText(tr("App password, only for providers without a browser sign in"));
    form->addRow(tr("App password"), m_secret);

    auto* signInRow = new QHBoxLayout();
    m_signInButton = new QPushButton(tr("Sign in with the browser"), this);
    m_signInStatus = new QLabel(this);
    m_signInStatus->setWordWrap(true);
    signInRow->addWidget(m_signInButton);
    signInRow->addWidget(m_signInStatus, 1);
    form->addRow(tr("Sign in"), signInRow);

    m_cooldownOn = new QCheckBox(tr("Account details are on cooldown"), this);
    form->addRow(m_cooldownOn);

    m_cooldownDays = new QSpinBox(this);
    m_cooldownDays->setRange(1, 3650);
    m_cooldownDays->setValue(m_mailbox.cooldownDays > 0 ? m_mailbox.cooldownDays : 30);
    m_cooldownDays->setSuffix(tr(" days"));
    form->addRow(tr("Cooldown length"), m_cooldownDays);

    m_cooldownUntil = new QDateTimeEdit(this);
    m_cooldownUntil->setCalendarPopup(true);
    m_cooldownUntil->setDateTime(m_mailbox.cooldownUntil.isValid()
                                     ? m_mailbox.cooldownUntil
                                     : QDateTime::currentDateTime().addDays(30));
    form->addRow(tr("Until"), m_cooldownUntil);

    m_cooldownNote = new QLineEdit(m_mailbox.cooldownNote, this);
    m_cooldownNote->setPlaceholderText(tr("Optional, for example the Steam account name"));
    form->addRow(tr("Note"), m_cooldownNote);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        const QString address = m_address->text().trimmed();
        if (address.isEmpty() || !address.contains(QChar('@'))) {
            QMessageBox::warning(this, tr("No address"),
                                 tr("Give the full email address of the account."));
            return;
        }
        m_mailbox.address = address;
        m_mailbox.providerId = m_provider->currentData().toString();
        m_mailbox.displayName = m_displayName->text().trimmed();
        if (m_mailbox.displayName.isEmpty())
            m_mailbox.displayName = address;
        m_mailbox.label = m_mailbox.displayName.left(2).toUpper();
        m_mailbox.secret = m_secret->text().toUtf8();
        m_mailbox.cooldownDays = m_cooldownDays->value();
        m_mailbox.cooldownNote = m_cooldownNote->text().trimmed();
        m_mailbox.cooldownUntil = m_cooldownOn->isChecked() ? m_cooldownUntil->dateTime() : QDateTime();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_signInButton, &QPushButton::clicked, this, &MailboxDialog::onSignIn);
    connect(m_cooldownOn, &QCheckBox::toggled, this, &MailboxDialog::onCooldownToggled);

    onCooldownToggled(m_mailbox.cooldownUntil.isValid());
    if (m_vault && m_vault->state().tokenFor(m_mailbox.id))
        m_signInStatus->setText(tr("Signed in."));
}

void MailboxDialog::onCooldownToggled(bool on)
{
    m_cooldownDays->setEnabled(on);
    m_cooldownUntil->setEnabled(on);
    m_cooldownNote->setEnabled(on);
}

void MailboxDialog::onSignIn()
{
    if (!m_vault)
        return;
    const QString providerId = m_provider->currentData().toString();
    const Provider* provider = providerById(providerId);
    if (!provider)
        return;

    const QString clientId = m_vault->state().settings.value(providerClientIdKey(providerId));
    if (clientId.isEmpty()) {
        QMessageBox::information(
            this, tr("No client id"),
            tr("MMC needs the client id of your %1 application before it can sign in.\n\n"
               "Open Settings and fill in the client id and client secret of your app "
               "registration. docs/PROVIDER-SETUP.md in the repository explains how to get them.")
                .arg(provider->name));
        return;
    }

    m_signInButton->setEnabled(false);
    m_signInStatus->setText(tr("Waiting for the browser..."));
    QApplication::processEvents();

    OAuth2Tokens tokens;
    QString error;
    const QString redirect = OAuth2::redirectUriForPort(OAuth2::kDefaultLoopbackPort);
    const QString clientSecret = m_vault->state().settings.value(providerClientSecretKey(providerId));

    const bool ok = OAuth2::authorize(*provider, clientId, clientSecret, redirect, &tokens, &error);
    m_signInButton->setEnabled(true);
    if (!ok) {
        m_signInStatus->setText(tr("Sign in failed."));
        QMessageBox::warning(this, tr("Sign in failed"), error);
        return;
    }

    const QString mailboxId = m_mailbox.id;
    OAuthToken* token = m_vault->state().tokenFor(mailboxId);
    if (!token) {
        OAuthToken fresh;
        fresh.mailboxId = mailboxId;
        fresh.accessToken = tokens.accessToken;
        fresh.refreshToken = tokens.refreshToken;
        fresh.expiry = tokens.expiry;
        fresh.scope = tokens.scope;
        m_vault->state().tokens.append(fresh);
    } else {
        token->accessToken = tokens.accessToken;
        token->refreshToken = tokens.refreshToken;
        token->expiry = tokens.expiry;
        token->scope = tokens.scope;
    }
    m_signedIn = true;
    m_signInStatus->setText(tr("Signed in."));
    QMessageBox::information(this, tr("Signed in"),
                             tr("The account is connected. Save the account to keep it."));
}

} // namespace mmc

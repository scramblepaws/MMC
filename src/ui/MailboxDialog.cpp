#include "ui/MailboxDialog.h"

#include "core/Vault.h"
#include "net/Providers.h"
#include "core/ProviderIcons.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace mmc {

MailboxDialog::MailboxDialog(Vault* vault, const Mailbox& mailbox, Mode mode,
                             bool signInNow, QWidget* parent)
    : QDialog(parent), m_vault(vault), m_mailbox(mailbox), m_mode(mode), m_signInNow(signInNow)
{
    if (m_mode == AddAccount && m_mailbox.id.isEmpty())
        m_mailbox.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    setWindowTitle(m_mode == AddAccount ? tr("Add an account") : tr("Edit account"));
    setMinimumWidth(540);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    // --- the address, the only thing that is really needed ---------------
    auto* addressLabel = new QLabel(m_mode == AddAccount ? tr("Email address") : tr("Address"), this);
    QFont heading = addressLabel->font();
    heading.setBold(true);
    addressLabel->setFont(heading);
    layout->addWidget(addressLabel);

    m_address = new QLineEdit(m_mailbox.address, this);
    m_address->setPlaceholderText(tr("name@gmail.com"));
    m_address->setMinimumHeight(34);
    layout->addWidget(m_address);

    m_providerHint = new QLabel(this);
    m_providerHint->setWordWrap(true);
    layout->addWidget(m_providerHint);

    // --- everything else, hidden until it is needed ----------------------
    m_moreButton = new QToolButton(this);
    m_moreButton->setText(tr("More options"));
    m_moreButton->setCheckable(true);
    m_moreButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_moreButton->setArrowType(Qt::RightArrow);
    m_moreButton->setAutoRaise(true);
    layout->addWidget(m_moreButton);

    m_moreBox = new QWidget(this);
    auto* moreLayout = new QVBoxLayout(m_moreBox);
    moreLayout->setContentsMargins(0, 0, 0, 0);

    auto* form = new QFormLayout();
    moreLayout->addLayout(form);

    m_displayName = new QLineEdit(m_mailbox.displayName, this);
    m_displayName->setPlaceholderText(tr("Optional, shown in the account list"));
    form->addRow(tr("Name"), m_displayName);

    m_provider = new QComboBox(this);
    for (const QString& id : providerIds()) {
        const Provider* provider = providerById(id);
        m_provider->addItem(providerIcon(id, 18), provider->name, id);
    }
    if (!m_mailbox.providerId.isEmpty())
        m_provider->setCurrentIndex(m_provider->findData(m_mailbox.providerId));
    m_providerTouched = !m_mailbox.providerId.isEmpty();
    form->addRow(tr("Provider"), m_provider);

    m_secret = new QLineEdit(QString::fromUtf8(m_mailbox.secret), this);
    m_secret->setEchoMode(QLineEdit::Password);
    m_secret->setPlaceholderText(tr("Only for accounts without a browser sign in"));
    form->addRow(tr("App password"), m_secret);

    m_signInButton = new QPushButton(tr("Sign in with the browser"), this);
    m_signInButton->setToolTip(tr("Signs the account in with %1. The browser opens and you allow "
                                  "the app.").arg(providerLabel()));
    form->addRow(QString(), m_signInButton);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    moreLayout->addWidget(line);

    m_cooldownOn = new QCheckBox(tr("The account behind it cannot be changed yet"), this);
    moreLayout->addWidget(m_cooldownOn);

    auto* cooldownForm = new QFormLayout();
    moreLayout->addLayout(cooldownForm);

    m_cooldownDays = new QSpinBox(this);
    m_cooldownDays->setRange(1, 3650);
    m_cooldownDays->setValue(m_mailbox.cooldownDays > 0 ? m_mailbox.cooldownDays : 30);
    m_cooldownDays->setSuffix(tr(" days"));
    cooldownForm->addRow(tr("Cooldown length"), m_cooldownDays);

    m_cooldownUntil = new QDateTimeEdit(this);
    m_cooldownUntil->setCalendarPopup(true);
    m_cooldownUntil->setDateTime(m_mailbox.cooldownUntil.isValid()
                                     ? m_mailbox.cooldownUntil
                                     : QDateTime::currentDateTime().addDays(30));
    cooldownForm->addRow(tr("Until"), m_cooldownUntil);

    m_cooldownNote = new QLineEdit(m_mailbox.cooldownNote, this);
    m_cooldownNote->setPlaceholderText(tr("Optional, for example the Steam account name"));
    cooldownForm->addRow(tr("Note"), m_cooldownNote);

    m_moreBox->setVisible(false);
    layout->addWidget(m_moreBox);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    // --- the buttons -----------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_acceptButton = buttons->addButton(m_mode == AddAccount ? tr("Add and sign in")
                                                             : tr("Save"),
                                        QDialogButtonBox::AcceptRole);
    m_acceptButton->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &MailboxDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_address, &QLineEdit::textChanged, this, &MailboxDialog::onAddressChanged);
    connect(m_address, &QLineEdit::returnPressed, this, &MailboxDialog::onAccept);
    connect(m_moreButton, &QToolButton::toggled, this, &MailboxDialog::onMoreToggled);
    connect(m_secret, &QLineEdit::textChanged, this, &MailboxDialog::onAddressChanged);
    connect(m_provider, &QComboBox::currentIndexChanged, this, [this]() {
        if (!m_settingProvider)
            m_providerTouched = true;
        onAddressChanged();
    });
    connect(m_signInButton, &QPushButton::clicked, this, [this]() { runSignIn(); });

    detectProvider();
    onCooldownToggled();
    connect(m_cooldownOn, &QCheckBox::toggled, this, &MailboxDialog::onCooldownToggled);

    m_address->setFocus();
    m_address->selectAll();
    onAddressChanged();

    if (m_signInNow)
        QTimer::singleShot(0, this, [this]() { runSignIn(); });
}

void MailboxDialog::onCooldownToggled()
{
    m_cooldownDays->setEnabled(m_cooldownOn->isChecked());
    m_cooldownUntil->setEnabled(m_cooldownOn->isChecked());
    m_cooldownNote->setEnabled(m_cooldownOn->isChecked());
}

void MailboxDialog::onMoreToggled(bool on)
{
    m_moreBox->setVisible(on);
    m_moreButton->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
}

QString MailboxDialog::providerLabel() const
{
    const Provider* provider = providerById(m_provider->currentData().toString());
    return provider ? provider->name : QString();
}

void MailboxDialog::detectProvider()
{
    if (m_providerTouched)
        return;
    const QString guessed = providerIdForAddress(m_address->text().trimmed());
    if (guessed.isEmpty())
        return;
    const int index = m_provider->findData(guessed);
    if (index >= 0 && index != m_provider->currentIndex()) {
        m_settingProvider = true;
        m_provider->setCurrentIndex(index);
        m_settingProvider = false;
    }
}

void MailboxDialog::onAddressChanged()
{
    detectProvider();

    const QString address = m_address->text().trimmed();
    const bool known = !providerIdForAddress(address).isEmpty();

    if (address.isEmpty()) {
        m_providerHint->setText(tr("Type the address of the account you want to add."));
    } else if (known) {
        m_providerHint->setText(tr("This is a %1 account.").arg(providerLabel()));
    } else {
        m_providerHint->setText(tr("MMC does not know this provider, open the more options "
                                   "and pick one."));
        m_moreButton->setChecked(true);
    }
    updateAcceptButton();
}

void MailboxDialog::updateAcceptButton()
{
    if (m_mode != AddAccount) {
        m_acceptButton->setText(tr("Save"));
        return;
    }
    const bool hasSecret = !m_secret->text().isEmpty();
    const bool hasClientId = m_vault
                              && !m_vault->state()
                                      .settings.value(providerClientIdKey(
                                          m_provider->currentData().toString()))
                                       .isEmpty();
    if (hasSecret || !hasClientId)
        m_acceptButton->setText(tr("Add"));
    else
        m_acceptButton->setText(tr("Add and sign in"));
}

bool MailboxDialog::wantsSignIn() const
{
    if (m_mode != AddAccount)
        return false;
    if (!m_secret->text().isEmpty())
        return false;   // an app password is enough
    const QString clientId = m_vault->state()
                                 .settings.value(providerClientIdKey(m_provider->currentData().toString()));
    return !clientId.isEmpty();
}

void MailboxDialog::setStatus(const QString& text, bool isError)
{
    m_status->setText(text);
    m_status->setStyleSheet(isError ? QStringLiteral("color: #c0392b;") : QString());
}

void MailboxDialog::runSignIn()
{
    const QString providerId = m_provider->currentData().toString();
    const Provider* provider = providerById(providerId);
    if (!provider)
        return;

    const QString clientId = m_vault->state().settings.value(providerClientIdKey(providerId));
    if (clientId.isEmpty()) {
        setStatus(tr("There is no client id for %1 yet. Put one in Settings first, then try "
                     "again.").arg(provider->name), true);
        return;
    }
    const QString clientSecret = m_vault->state().settings.value(providerClientSecretKey(providerId));
    const QString redirect = OAuth2::redirectUriForPort(OAuth2::kDefaultLoopbackPort);

    setStatus(tr("MMC opens your browser now. Allow the app there, then come back."));
    m_acceptButton->setEnabled(false);
    m_address->setEnabled(false);
    m_signInButton->setEnabled(false);
    QApplication::processEvents();

    QString error;
    const bool ok = OAuth2::authorize(*provider, clientId, clientSecret, redirect, &m_tokens, &error);

    m_acceptButton->setEnabled(true);
    m_address->setEnabled(true);
    m_signInButton->setEnabled(true);

    if (!ok) {
        m_signInError = error;
        setStatus(error, true);
        return;
    }
    m_hasTokens = true;
    m_signInError.clear();
    setStatus(tr("Signed in."));
}

void MailboxDialog::onAccept()
{
    const QString address = m_address->text().trimmed();
    if (address.isEmpty() || !address.contains(QChar('@'))) {
        setStatus(tr("Give the full email address of the account."), true);
        m_address->setFocus();
        return;
    }

    m_mailbox.address = address.toLower();
    m_mailbox.providerId = m_provider->currentData().toString();
    m_mailbox.displayName = m_displayName->text().trimmed();
    if (m_mailbox.displayName.isEmpty())
        m_mailbox.displayName = m_mailbox.address;
    m_mailbox.label = m_mailbox.displayName.left(2).toUpper();
    m_mailbox.secret = m_secret->text().toUtf8();
    m_mailbox.cooldownDays = m_cooldownDays->value();
    m_mailbox.cooldownNote = m_cooldownNote->text().trimmed();
    m_mailbox.cooldownUntil = m_cooldownOn->isChecked() ? m_cooldownUntil->dateTime() : QDateTime();

    if (wantsSignIn() && !m_hasTokens)
        runSignIn();

    accept();
}

} // namespace mmc

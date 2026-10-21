#include "ui/SettingsDialog.h"

#include "core/Vault.h"
#include "net/Providers.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace mmc {

SettingsDialog::SettingsDialog(Vault* vault, QWidget* parent)
    : QDialog(parent), m_vault(vault)
{
    setWindowTitle(tr("Settings"));
    resize(820, 560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    auto* heading = new QLabel(tr("Settings"), this);
    heading->setFont(Theme::font(19, QFont::DemiBold));
    layout->addWidget(heading);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);

    m_navigation = new QListWidget(splitter);
    m_navigation->setFixedWidth(210);
    m_navigation->setSpacing(2);
    splitter->addWidget(m_navigation);

    m_pages = new QStackedWidget(splitter);
    splitter->addWidget(m_pages);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    // --- general ----------------------------------------------------------
    auto* general = new QWidget(m_pages);
    auto* generalForm = new QFormLayout(general);
    generalForm->setContentsMargins(20, 20, 20, 20);
    generalForm->setSpacing(12);
    generalForm->addRow(new QLabel(tr("MMC keeps the mail of every account in one window. The "
                                      "accounts live in an encrypted vault, the master "
                                      "password opens it."), general));
    m_confirmOnSend = new QCheckBox(tr("Ask before sending a message"), general);
    m_confirmOnSend->setChecked(
        m_vault->state().settings.value(QStringLiteral("confirmBeforeSend"),
                                        QStringLiteral("true")) == QLatin1String("true"));
    generalForm->addRow(QString(), m_confirmOnSend);
    addPage(tr("General"), QStringLiteral("settings"), general);

    // --- appearance -------------------------------------------------------
    auto* appearance = new QWidget(m_pages);
    auto* appearanceForm = new QFormLayout(appearance);
    appearanceForm->setContentsMargins(20, 20, 20, 20);
    appearanceForm->setSpacing(12);
    appearanceForm->addRow(new QLabel(tr("MMC is a dark program. The text of the messages can be "
                                         "made bigger for the eyes."), appearance));
    auto* textSize = new QSpinBox(appearance);
    textSize->setRange(11, 22);
    textSize->setValue(m_vault->state().settings.value(QStringLiteral("messageTextSize"),
                                                      QStringLiteral("13")).toInt());
    appearanceForm->addRow(tr("Text size"), textSize);
    addPage(tr("Appearance"), QStringLiteral("palette"), appearance);

    // --- the providers ----------------------------------------------------
    auto* providers = new QWidget(m_pages);
    auto* providersForm = new QFormLayout(providers);
    providersForm->setContentsMargins(20, 20, 20, 20);
    providersForm->setSpacing(12);
    providersForm->addRow(new QLabel(
        tr("MMC signs in the way an application does, not the way a person does. Every provider "
           "needs a client id and a client secret. Put the address below in the registration of "
           "your application: %1")
            .arg(QStringLiteral("http://127.0.0.1:56789/oauth/callback")),
        providers));

    for (const QString& providerId : providerIds()) {
        const Provider* provider = providerById(providerId);
        auto* clientId = new QLineEdit(providers);
        auto* clientSecret = new QLineEdit(providers);
        clientSecret->setEchoMode(QLineEdit::Password);
        clientId->setObjectName(QStringLiteral("id.%1").arg(providerId));
        clientSecret->setObjectName(QStringLiteral("secret.%1").arg(providerId));
        clientId->setText(m_vault->state().settings.value(providerClientIdKey(providerId)));
        clientSecret->setText(m_vault->state().settings.value(providerClientSecretKey(providerId)));
        providersForm->addRow(tr("%1 client id").arg(provider->name), clientId);
        providersForm->addRow(tr("%1 client secret").arg(provider->name), clientSecret);
    }
    addPage(tr("Accounts"), QStringLiteral("at"), providers);

    // --- privacy ----------------------------------------------------------
    auto* privacy = new QWidget(m_pages);
    auto* privacyForm = new QFormLayout(privacy);
    privacyForm->setContentsMargins(20, 20, 20, 20);
    privacyForm->setSpacing(12);
    privacyForm->addRow(new QLabel(tr("A mail can carry pictures from the internet. When they are "
                                      "not loaded, the provider does not know that the mail was "
                                      "opened."), privacy));
    m_remoteImages = new QCheckBox(tr("Load pictures from the internet"), privacy);
    m_remoteImages->setChecked(
        m_vault->state().settings.value(QStringLiteral("loadRemoteImages"),
                                        QStringLiteral("false")) == QLatin1String("true"));
    privacyForm->addRow(QString(), m_remoteImages);
    addPage(tr("Privacy and security"), QStringLiteral("lock"), privacy);

    // --- about ------------------------------------------------------------
    auto* about = new QWidget(m_pages);
    auto* aboutLayout = new QVBoxLayout(about);
    aboutLayout->setContentsMargins(20, 20, 20, 20);
    aboutLayout->setSpacing(8);
    auto* aboutTitle = new QLabel(QStringLiteral("MMC"), about);
    aboutTitle->setFont(Theme::font(22, QFont::Bold));
    aboutLayout->addWidget(aboutTitle);
    auto* aboutText = new QLabel(tr("Multiple Mail Client\n\nThe accounts live in:\n%1\n\n"
                                    "The vault is encrypted with AES-256-GCM, the key comes from "
                                    "the master password.")
                                     .arg(m_vault->path()), about);
    aboutText->setFont(Theme::font(12));
    aboutText->setWordWrap(true);
    aboutText->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textSecondary().name()));
    aboutLayout->addWidget(aboutText);
    aboutLayout->addStretch(1);
    addPage(tr("About MMC"), QStringLiteral("info"), about);

    // --- the buttons ------------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        for (const QString& providerId : providerIds()) {
            auto* id = m_pages->findChild<QLineEdit*>(QStringLiteral("id.%1").arg(providerId));
            auto* secret = m_pages->findChild<QLineEdit*>(QStringLiteral("secret.%1").arg(providerId));
            if (id)
                m_vault->state().settings[providerClientIdKey(providerId)] = id->text().trimmed();
            if (secret)
                m_vault->state().settings[providerClientSecretKey(providerId)] = secret->text().trimmed();
        }
        m_vault->state().settings[QStringLiteral("loadRemoteImages")] =
            m_remoteImages->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
        m_vault->state().settings[QStringLiteral("confirmBeforeSend")] =
            m_confirmOnSend->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
        QString error;
        m_vault->save(&error);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_navigation, &QListWidget::itemSelectionChanged, this, [this]() {
        const auto items = m_navigation->selectedItems();
        if (!items.isEmpty())
            m_pages->setCurrentIndex(items.first()->data(Qt::UserRole).toInt());
    });
}

void SettingsDialog::addPage(const QString& title, const QString& icon, QWidget* page)
{
    const int index = m_pages->addWidget(page);
    auto* item = new QListWidgetItem(Icons::icon(icon, Theme::textSecondary(), 18), title, m_navigation);
    item->setData(Qt::UserRole, index);
    item->setSizeHint(QSize(0, 36));
    if (index == 0)
        m_navigation->setCurrentItem(item);
}

} // namespace mmc

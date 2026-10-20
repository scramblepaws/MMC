#pragma once

#include "core/Model.h"
#include "net/OAuth2.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QToolButton;
class QWidget;

namespace mmc {

class Vault;

// Adding or editing one account.
//
// Adding is two things in one: the account details and the sign in with the
// provider. The dialog does both, so the user types an address and presses
// one button.
class MailboxDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { AddAccount, EditAccount };

    MailboxDialog(Vault* vault, const Mailbox& mailbox, Mode mode,
                   bool signInNow = false, QWidget* parent = nullptr);

    Mailbox mailbox() const { return m_mailbox; }
    bool hasTokens() const { return m_hasTokens; }
    const OAuth2Tokens& tokens() const { return m_tokens; }
    QString signInError() const { return m_signInError; }

private slots:
    void onAddressChanged();
    void onMoreToggled(bool on);
    void onCooldownToggled();
    void onAccept();

private:
    void detectProvider();
    void runSignIn();
    void updateAcceptButton();
    QString providerLabel() const;
    bool wantsSignIn() const;
    void setStatus(const QString& text, bool isError = false);

    Vault* m_vault = nullptr;
    Mailbox m_mailbox;
    Mode m_mode = AddAccount;
    bool m_signInNow = false;
    bool m_providerTouched = false;
    bool m_settingProvider = false;
    OAuth2Tokens m_tokens;
    bool m_hasTokens = false;
    QString m_signInError;

    QLineEdit* m_address = nullptr;
    QLabel* m_providerHint = nullptr;
    QToolButton* m_moreButton = nullptr;
    QWidget* m_moreBox = nullptr;
    QComboBox* m_provider = nullptr;
    QLineEdit* m_displayName = nullptr;
    QLineEdit* m_secret = nullptr;
    QPushButton* m_signInButton = nullptr;
    QCheckBox* m_cooldownOn = nullptr;
    QSpinBox* m_cooldownDays = nullptr;
    QDateTimeEdit* m_cooldownUntil = nullptr;
    QLineEdit* m_cooldownNote = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_acceptButton = nullptr;
};

} // namespace mmc

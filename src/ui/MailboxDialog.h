#pragma once

#include "core/Model.h"

#include <QDialog>

class QComboBox;
class QDateTimeEdit;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

namespace mmc {

class Vault;

// Add or edit one mailbox: the address, how to log in, and the cooldown note.
class MailboxDialog : public QDialog
{
    Q_OBJECT
public:
    MailboxDialog(Vault* vault, const Mailbox& mailbox, QWidget* parent = nullptr);

    Mailbox mailbox() const { return m_mailbox; }
    bool isNewMailbox() const { return m_isNew; }

private slots:
    void onSignIn();
    void onCooldownToggled(bool on);

private:
    Vault* m_vault = nullptr;
    Mailbox m_mailbox;
    bool m_isNew = true;
    bool m_signedIn = false;

    QLineEdit* m_address = nullptr;
    QComboBox* m_provider = nullptr;
    QLineEdit* m_displayName = nullptr;
    QLineEdit* m_secret = nullptr;
    QPushButton* m_signInButton = nullptr;
    QLabel* m_signInStatus = nullptr;
    QCheckBox* m_cooldownOn = nullptr;
    QSpinBox* m_cooldownDays = nullptr;
    QDateTimeEdit* m_cooldownUntil = nullptr;
    QLineEdit* m_cooldownNote = nullptr;
};

} // namespace mmc

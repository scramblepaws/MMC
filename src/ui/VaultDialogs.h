#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QPushButton;
class QLineEdit;

namespace mmc {

// First screen: pick a vault file, or create a new one.
class VaultChooserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VaultChooserDialog(QWidget* parent = nullptr);

    QString selectedPath() const { return m_selectedPath; }
    // Set when the user just created a vault, so the caller does not have to
    // ask for the password a second time.
    QString createdPassword() const { return m_createdPassword; }

private slots:
    void onNewVault();
    void onOpenFile();
    void onSelectionChanged();
    void onOpen();

private:
    QListWidget* m_list = nullptr;
    QPushButton* m_openButton = nullptr;
    QString m_selectedPath;
    QString m_createdPassword;
};

// Asks for the master password, twice when a new vault is created.
//
// When a vault path is given and that vault has a recovery passphrase, the
// dialog offers a way back in without the master password.
class MasterPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    MasterPasswordDialog(const QString& title, bool confirm,
                         const QString& vaultPath = QString(), QWidget* parent = nullptr);

    QString password() const;
    // Set when the user came back in with the recovery passphrase.
    QString resetPassword() const { return m_resetPassword; }

private slots:
    void onForgotPassword();

private:
    QLineEdit* m_password = nullptr;
    QLineEdit* m_confirmField = nullptr;
    bool m_askConfirm = false;
    QString m_vaultPath;
    QString m_resetPassword;
};

// Shows a recovery passphrase that is shown only once.
class RecoverySetupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RecoverySetupDialog(const QString& passphrase, QWidget* parent = nullptr);
};

// Comes back into a vault with the recovery passphrase and sets a new master
// password.
class ResetPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ResetPasswordDialog(const QString& vaultPath, QWidget* parent = nullptr);

    QString newPassword() const { return m_newPassword; }

private slots:
    void onAccept();

private:
    QString m_vaultPath;
    QString m_newPassword;
    QLineEdit* m_recovery = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_confirmField = nullptr;
};

} // namespace mmc

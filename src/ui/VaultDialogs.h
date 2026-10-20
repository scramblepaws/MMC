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
class MasterPasswordDialog : public QDialog
{
    Q_OBJECT
public:
    MasterPasswordDialog(const QString& title, bool confirm, QWidget* parent = nullptr);

    QString password() const;

private:
    QLineEdit* m_password = nullptr;
    QLineEdit* m_confirmField = nullptr;
    bool m_askConfirm = false;
};

} // namespace mmc

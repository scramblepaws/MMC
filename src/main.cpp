#include "core/Vault.h"
#include "ui/MainWindow.h"
#include "ui/VaultDialogs.h"

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>

using namespace mmc;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MMC"));
    QApplication::setApplicationDisplayName(QStringLiteral("MMC"));
    QApplication::setOrganizationName(QStringLiteral("MMC"));

    // A vault can be named on the command line, which is handy for the vault
    // on a USB stick: MMC.exe D:\vaults\mine.mmcv
    const QStringList arguments = QApplication::arguments();
    const QString wantedVault = arguments.size() > 1 ? arguments.at(1) : QString();

    Vault vault;
    QString path = wantedVault;
    QString password;

    if (!path.isEmpty()) {
        if (!QFileInfo::exists(path)) {
            QMessageBox::critical(nullptr, QObject::tr("No vault"),
                                 QObject::tr("%1 does not exist.").arg(path));
            return 1;
        }
    } else {
        VaultChooserDialog chooser;
        if (chooser.exec() != QDialog::Accepted)
            return 0;
        path = chooser.selectedPath();
        if (path.isEmpty())
            return 0;
        password = chooser.createdPassword();
    }

    if (password.isEmpty()) {
        MasterPasswordDialog dialog(QObject::tr("Unlock %1").arg(QFileInfo(path).fileName()),
                                     false, path, nullptr);
        if (dialog.exec() != QDialog::Accepted)
            return 0;
        password = dialog.resetPassword().isEmpty() ? dialog.password() : dialog.resetPassword();
    }

    QString error;
    if (!vault.open(path, password, &error)) {
        // The master password can be wrong, but a recovery passphrase can get
        // in anyway.
        if (Vault::vaultHasRecovery(path)) {
            QMessageBox::critical(nullptr, QObject::tr("Cannot open the vault"),
                                  QObject::tr("%1\n\nStart MMC again and use 'Forgot the master "
                                               "password?' on the unlock screen.").arg(error));
        } else {
            QMessageBox::critical(nullptr, QObject::tr("Cannot open the vault"), error);
        }
        return 1;
    }

    MainWindow window(&vault);
    window.show();
    return app.exec();
}

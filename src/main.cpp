#include "core/Vault.h"
#include "ui/MainWindow.h"
#include "ui/VaultDialogs.h"

#include <QApplication>
#include <QMessageBox>

using namespace mmc;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MMC"));
    QApplication::setApplicationDisplayName(QStringLiteral("MMC"));
    QApplication::setOrganizationName(QStringLiteral("MMC"));

    VaultChooserDialog chooser;
    if (chooser.exec() != QDialog::Accepted)
        return 0;

    const QString path = chooser.selectedPath();
    if (path.isEmpty())
        return 0;

    Vault vault;
    if (!vault.isOpen()) {
        // A vault that was just created is already open, otherwise ask for the
        // master password.
        QString password = chooser.createdPassword();
        if (password.isEmpty()) {
            MasterPasswordDialog dialog(QObject::tr("Unlock %1").arg(path), false, nullptr);
            if (dialog.exec() != QDialog::Accepted)
                return 0;
            password = dialog.password();
        }
        QString error;
        if (!vault.open(path, password, &error)) {
            QMessageBox::critical(nullptr, QObject::tr("Cannot open the vault"), error);
            return 1;
        }
    }

    MainWindow window(&vault);
    window.show();
    return app.exec();
}

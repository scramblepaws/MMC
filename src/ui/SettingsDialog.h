#pragma once

#include <QDialog>

class QListWidget;
class QStackedWidget;
class QCheckBox;
class QLineEdit;

namespace mmc {

class Vault;

// The settings, in groups. The group that is picked is on the left, its
// settings on the right.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Vault* vault, QWidget* parent = nullptr);

private:
    void addPage(const QString& title, const QString& icon, QWidget* page);

    Vault* m_vault = nullptr;
    QListWidget* m_navigation = nullptr;
    QStackedWidget* m_pages = nullptr;
    QCheckBox* m_remoteImages = nullptr;
    QCheckBox* m_confirmOnSend = nullptr;
};

} // namespace mmc

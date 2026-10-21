#pragma once

#include <QDialog>

class QListWidget;
class QLabel;

namespace mmc {

class Vault;

// A page with every account: how it is connected, how much mail is waiting
// and what can be done with it.
class AccountManager : public QDialog
{
    Q_OBJECT
public:
    explicit AccountManager(Vault* vault, QWidget* parent = nullptr);

signals:
    void accountChanged();

private:
    void rebuild();
    void refreshSelected();

    Vault* m_vault = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_details = nullptr;
};

} // namespace mmc

#pragma once

#include <QWidget>

class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;

namespace mmc {

// The left column: the name of the program, the button to write a message,
// where to look, and the accounts.
class Sidebar : public QWidget
{
    Q_OBJECT
public:
    // The places the program can show.
    enum class View {
        AllAccounts,
        Inbox,
        Starred,
        Sent,
        Drafts,
        Spam,
        Trash,
    };
    static QString viewName(View view);

    explicit Sidebar(const QString& vaultName, QWidget* parent = nullptr);

    void setAccounts(const QList<class Mailbox*>& accounts);
    void setUnread(const QString& mailboxId, int count);
    void setView(View view);
    View view() const { return m_view; }
    QString selectedAccountId() const;
    void setSelectedAccountId(const QString& id);

signals:
    void composeClicked();
    void viewChanged(Sidebar::View view);
    void accountSelected(const QString& mailboxId);
    void accountContextMenuRequested(const QString& mailboxId, const QPoint& position);
    void addAccountClicked();

private:
    QListWidget* m_nav = nullptr;
    QListWidget* m_accounts = nullptr;
    QLineEdit* m_filter = nullptr;
    QLabel* m_accountsTitle = nullptr;
    QPushButton* m_collapseButton = nullptr;
    View m_view = View::AllAccounts;
};

} // namespace mmc

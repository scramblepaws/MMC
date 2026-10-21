#pragma once

#include "core/Model.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;

namespace mmc {

// Write a new message with one of the accounts in the vault.
class ComposeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ComposeDialog(const QVector<Mailbox>& mailboxes, QWidget* parent = nullptr);

    QString mailboxId() const;
    QString to() const;
    QString cc() const;
    QString subject() const;
    QString bodyText() const;
    QVector<QByteArray> attachmentData() const;
    QStringList attachmentNames() const;

    void setRecipient(const QString& address);
    void setSubject(const QString& subject);

private slots:
    void onAddAttachment();
    void onRemoveAttachment();

private:
    QComboBox* m_from = nullptr;
    QLineEdit* m_to = nullptr;
    QLineEdit* m_cc = nullptr;
    QLineEdit* m_subject = nullptr;
    QPlainTextEdit* m_body = nullptr;
    QListWidget* m_attachments = nullptr;
    QVector<QByteArray> m_attachmentData;
};

} // namespace mmc

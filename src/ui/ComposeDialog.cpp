#include "ui/ComposeDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace mmc {

ComposeDialog::ComposeDialog(const QVector<Mailbox>& mailboxes, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New message"));
    resize(720, 560);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    layout->addLayout(form);

    m_from = new QComboBox(this);
    for (const Mailbox& m : mailboxes)
        m_from->addItem(m.displayName.isEmpty() ? m.address : m.displayName, m.id);
    form->addRow(tr("From"), m_from);

    m_to = new QLineEdit(this);
    form->addRow(tr("To"), m_to);

    m_cc = new QLineEdit(this);
    form->addRow(tr("Cc"), m_cc);

    m_subject = new QLineEdit(this);
    form->addRow(tr("Subject"), m_subject);

    m_body = new QPlainTextEdit(this);
    m_body->setPlaceholderText(tr("Write your message"));
    layout->addWidget(m_body, 1);

    auto* attachmentRow = new QHBoxLayout();
    m_attachments = new QListWidget(this);
    m_attachments->setMaximumHeight(90);
    QPushButton* addButton = new QPushButton(tr("Add file"), this);
    QPushButton* removeButton = new QPushButton(tr("Remove"), this);
    attachmentRow->addWidget(m_attachments, 1);
    attachmentRow->addWidget(addButton);
    attachmentRow->addWidget(removeButton);
    layout->addLayout(attachmentRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* sendButton = buttons->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    connect(addButton, &QPushButton::clicked, this, &ComposeDialog::onAddAttachment);
    connect(removeButton, &QPushButton::clicked, this, &ComposeDialog::onRemoveAttachment);
    connect(sendButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ComposeDialog::onAddAttachment()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Attach files"));
    for (const QString& file : files) {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        m_attachmentData << f.readAll();
        m_attachments->addItem(QFileInfo(file).fileName());
    }
}

void ComposeDialog::onRemoveAttachment()
{
    const int row = m_attachments->currentRow();
    if (row < 0)
        return;
    m_attachments->takeItem(row);
    m_attachmentData.remove(row);
}

QString ComposeDialog::mailboxId() const
{
    return m_from->currentData().toString();
}

QString ComposeDialog::to() const
{
    return m_to->text().trimmed();
}

QString ComposeDialog::cc() const
{
    return m_cc->text().trimmed();
}

QString ComposeDialog::subject() const
{
    return m_subject->text();
}

QString ComposeDialog::bodyText() const
{
    return m_body->toPlainText();
}

QVector<QByteArray> ComposeDialog::attachmentData() const
{
    return m_attachmentData;
}

QStringList ComposeDialog::attachmentNames() const
{
    QStringList names;
    for (int i = 0; i < m_attachments->count(); ++i)
        names << m_attachments->item(i)->text();
    return names;
}

} // namespace mmc

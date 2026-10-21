#pragma once

#include "core/Model.h"
#include "net/Mime.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QStackedWidget;
class QTextBrowser;
class QToolButton;

namespace mmc {

class EmptyState;

// The right column: the message itself.
class Reader : public QWidget
{
    Q_OBJECT
public:
    explicit Reader(QWidget* parent = nullptr);

    void showMessage(const Message& message, const ParsedMessage& parsed);
    void showEmpty();
    void showError(const QString& title, const QString& text, const std::function<void()>& retry);
    void setRemoteImagesAllowed(bool allowed);
    void clear();

signals:
    void retryRequested();
    void replyRequested();
    void forwardRequested();
    void starToggled(bool starred);
    void deleteRequested();
    void attachmentSaved(const QString& fileName, const QByteArray& data);

private:
    void buildHeader();

    QStackedWidget* m_pages = nullptr;
    QWidget* m_messagePage = nullptr;
    EmptyState* m_emptyPage = nullptr;
    EmptyState* m_errorPage = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_subject = nullptr;
    QLabel* m_sender = nullptr;
    QLabel* m_recipient = nullptr;
    QLabel* m_date = nullptr;
    QLabel* m_account = nullptr;
    QToolButton* m_starButton = nullptr;
    QListWidget* m_attachments = nullptr;
    QTextBrowser* m_body = nullptr;
    QByteArray m_lastBody;
    bool m_remoteImages = false;
};

} // namespace mmc

#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

namespace mmc {

// The state of a place with nothing in it: no message picked, no account yet,
// an error that can be tried again.
class EmptyState : public QWidget
{
    Q_OBJECT
public:
    explicit EmptyState(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setText(const QString& text);
    void setIconName(const QString& name);
    void setAction(const QString& label, const std::function<void()>& action);
    void setVariant(const QString& variant);   // "neutral" or "error"

private:
    QLabel* m_icon = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_text = nullptr;
    QPushButton* m_button = nullptr;
};

} // namespace mmc

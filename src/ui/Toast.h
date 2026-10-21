#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace mmc {

// A small message that slides in at the bottom right of the window.
class Toast : public QWidget
{
    Q_OBJECT
public:
    enum class Kind { Success, Information, Warning, Error };

    explicit Toast(Kind kind, const QString& title, const QString& text, QWidget* parent = nullptr);

    void setAction(const QString& label, const std::function<void()>& action);
    // Starts the timer that takes the toast away again.
    void showFor(int milliseconds);

private:
    Kind m_kind = Kind::Information;
    QPushButton* m_actionButton = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_text = nullptr;
};

} // namespace mmc

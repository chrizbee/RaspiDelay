#ifndef PROGRESSWIDGET_H
#define PROGRESSWIDGET_H

#include <QWidget>

class ProgressWidget : public QWidget
{
    Q_OBJECT

public:
    ProgressWidget(const QString &title = "", QWidget *parent = nullptr);
    void setProgress(size_t currentValue, size_t maxValue);
    void setTitle(const QString &title);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString title_;
    size_t currentValue_;
    size_t maxValue_;
};

#endif // PROGRESSWIDGET_H

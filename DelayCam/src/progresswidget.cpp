#include "progresswidget.h"
#include <QPainter>

ProgressWidget::ProgressWidget(const QString &title, QWidget *parent) :
    QWidget{parent},
    title_(title),
    currentValue_(0),
    maxValue_(1)
{
}

void ProgressWidget::setProgress(size_t currentValue, size_t maxValue)
{
    currentValue_ = currentValue;
    maxValue_ = maxValue;
    update();
}

void ProgressWidget::setTitle(const QString &title)
{
    title_ = title;
    update();
}

void ProgressWidget::paintEvent(QPaintEvent *)
{
    // Initialize painter
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::black);
    painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

    // Draw progress bar
    QRect barRect(0, 0, width() / 3, height() / 20);
    barRect.moveCenter(rect().center());
    float progress = 1 - static_cast<float>(currentValue_) / maxValue_;
    int pixelProgress = barRect.width() * progress;
    QRect progressRect = barRect.adjusted(0, 0, -pixelProgress, 0);
    painter.fillRect(progressRect, Qt::white);
    painter.drawRect(barRect);

    // Draw title
    if (!title_.isEmpty()) {
        QRect titleRect = rect().adjusted(0, 0, 0, -barRect.height() * 2.5);
        painter.setFont(QFont("Fira Code", 20));
        painter.drawText(titleRect, Qt::AlignCenter, title_);
    }
}

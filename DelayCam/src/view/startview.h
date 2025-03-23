#ifndef STARTPAGE_H
#define STARTPAGE_H

#include <QWidget>

class Button;

class StartView : public QWidget
{
    Q_OBJECT

public:
    StartView(QWidget *parent = nullptr);

Q_SIGNALS:
    void startPressed();
    void galleryPressed();
    void settingsRequested();

protected:
    void setupUi();

private:
    Button *welcomeWidget_;
    Button *startButton_;
    Button *galleryButton_;
};

#endif // STARTPAGE_H

#include "startview.h"
#include "ui/button.h"
#include "util/config.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>

StartView::StartView(QWidget *parent) :
    QWidget(parent)
{
    // Setup ui and connect button signals
    setupUi();
    connect(startButton_, &Button::pressed, this, &StartView::startPressed);
    connect(galleryButton_, &Button::pressed, this, &StartView::galleryPressed);
    connect(welcomeWidget_, &Button::spammed, this, &StartView::settingsRequested);
}

void StartView::setupUi()
{
    // Check if layout exists already
    if (layout() != nullptr)
        return;

    // Create widgets
    welcomeWidget_ = new Button(CFG.read<std::string>("strings.welcomeMessage", "Fotokistn").c_str(), this);
    welcomeWidget_->setAnimated(false);
    startButton_ = new Button(tr("Start"), QPixmap("://icons/icon.png"), this);
    galleryButton_ = new Button(tr("Gallery"), QPixmap("://icons/gallery.png"), this);

    // Set widget fonts
    QFont welcomeFont("Brigitta Demo");
    welcomeFont.setPixelSize(CFG.read<int>("fonts.welcomeFontSize", 56));
    welcomeWidget_->setFont(welcomeFont);

    // Create button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(settings::spacing);
    buttonLayout->addWidget(startButton_, 1);
    buttonLayout->addWidget(galleryButton_, 1);

    // Create main layout
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(welcomeWidget_);
    mainLayout->addLayout(buttonLayout);

    // Create horizontal and vertical layouts with expanding spacers
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addStretch(2);
    hLayout->addLayout(mainLayout, 3);
    hLayout->addStretch(2);
    QVBoxLayout *vLayout = new QVBoxLayout;
    vLayout->addStretch();
    vLayout->addLayout(hLayout);
    vLayout->addStretch();
    setLayout(vLayout);
}

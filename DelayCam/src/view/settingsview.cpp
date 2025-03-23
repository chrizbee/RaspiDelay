#include "settingsview.h"
#include "ui/widget.h"
#include "ui/button.h"
#include "ui/slider.h"
#include "ui/buttongroup.h"
#include "ui/floatingbuttons.h"
#include "util/config.h"
#include <QGridLayout>

SettingsView::SettingsView(QWidget *parent) :
    QWidget{parent}
{
    // Setup ui and load configuration
    setupUi();
    loadConfig();

    // Connect signals
    connect(themeGroup_, &ButtonGroup::checked, this, &SettingsView::onThemeChanged);
    connect(cameraGroup_, &ButtonGroup::checked, this, &SettingsView::onCameraChanged);
    connect(focusGroup_, &ButtonGroup::checked, this, &SettingsView::onFocusChanged);
    connect(buttons_, &FloatingButtons::backPressed, this, &SettingsView::backPressed);
    connect(buttons_, &FloatingButtons::savePressed, this, &SettingsView::saveConfig);
    connect(brightnessSlider_, &Slider::sliderReleased, this, &SettingsView::onBrightnessChanged);
    connect(colorTemperatureSlider_, &Slider::sliderReleased, this, &SettingsView::onColorTemperatureChanged);
}

void SettingsView::setupUi()
{
    // Create theme widgets, buttons and layout
    themeWidget_ = new Widget(tr("Theme"), QPixmap("://icons/theme.png"), this);
    darkThemeButton_ = new Button(tr("Dark"), QPixmap(), true, this);
    lightThemeButton_ = new Button(tr("Light"), QPixmap(), true, this);
    customThemeButton_ = new Button(tr("Custom"), QPixmap(), true, this);
    darkThemeButton_->setObjectName("dark");
    lightThemeButton_->setObjectName("light");
    customThemeButton_->setObjectName("custom");
    themeGroup_ = new ButtonGroup;
    themeGroup_->setSpacing(settings::spacing);
    themeGroup_->addButtons({darkThemeButton_, lightThemeButton_, customThemeButton_});

    // Create camera widgets, buttons and layout
    cameraWidget_ = new Widget(tr("Camera"), QPixmap("://icons/camera.png"), this);
    qtCameraButton_ = new Button("Qt", QPixmap(), true, this);
    libCameraButton_ = new Button("LibCamera", QPixmap(), true, this);
    qtCameraButton_->setObjectName("Qt");
    libCameraButton_->setObjectName("LibCamera");
    cameraGroup_ = new ButtonGroup;
    cameraGroup_->setSpacing(settings::spacing);
    cameraGroup_->addButtons({libCameraButton_, qtCameraButton_});

    // Create focus widgets, buttons and layout
    focusWidget_ = new Widget(tr("Focus"), QPixmap("://icons/focus.png"), this);
    onceFocusButton_ = new Button(tr("Once"), QPixmap(), true, this);
    everytimeFocusButton_ = new Button(tr("Everytime"), QPixmap(), true, this);
    focusGroup_ = new ButtonGroup;
    focusGroup_->setSpacing(settings::spacing);
    focusGroup_->addButtons({everytimeFocusButton_, onceFocusButton_});

    // Create brightness widget and slider
    QHBoxLayout *brightnessLayout = new QHBoxLayout;
    brightnessWidget_ = new Widget(tr("Brightness"), QPixmap("://icons/brightness.png"), this);
    brightnessSlider_ = new Slider(0, 255, CFG.read<int>("led.brightness", 200), this);
    brightnessLayout->addWidget(brightnessSlider_);
    brightnessLayout->setContentsMargins(brightnessWidget_->shadowMargins());
    brightnessSlider_->setHeight(brightnessWidget_->rawSizeHint().height());

    // Create color temperature widget and slider
    QHBoxLayout *colorTemperatureLayout = new QHBoxLayout;
    colorTemperatureWidget_ = new Widget(tr("Color Temp"), QPixmap("://icons/temperature.png"), this);
    colorTemperatureSlider_ = new Slider(1000, 6500, CFG.read<uint>("led.colorTemperature", 3200), this);
    colorTemperatureLayout->addWidget(colorTemperatureSlider_);
    colorTemperatureLayout->setContentsMargins(colorTemperatureWidget_->shadowMargins());
    colorTemperatureSlider_->setHeight(colorTemperatureWidget_->rawSizeHint().height());

    // Create main layout
    // Form layout is replaced by grid layout since it does not expand the left side widget
    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->setSpacing(settings::spacing * 4);
    mainLayout->addWidget(themeWidget_, 0, 0);
    mainLayout->addLayout(themeGroup_, 0, 1);
    mainLayout->addWidget(cameraWidget_, 1, 0);
    mainLayout->addLayout(cameraGroup_, 1, 1);
    mainLayout->addWidget(focusWidget_, 2, 0);
    mainLayout->addLayout(focusGroup_, 2, 1);
    mainLayout->addWidget(brightnessWidget_, 3, 0);
    mainLayout->addLayout(brightnessLayout, 3, 1);
    mainLayout->addWidget(colorTemperatureWidget_, 4, 0);
    mainLayout->addLayout(colorTemperatureLayout, 4, 1);

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

    // Create floating buttons
    buttons_ = new FloatingButtons(Buttons::BackButton, settings::spacing, this);
}

void SettingsView::loadConfig()
{
    // Load theme
    QString theme = CFG.read<std::string>("colors.colorTheme", "light").c_str();
    if (QString::compare(theme, "light", Qt::CaseInsensitive) == 0)
        lightThemeButton_->setChecked(true);
    else if (QString::compare(theme, "dark", Qt::CaseInsensitive) == 0)
        darkThemeButton_->setChecked(true);
    else customThemeButton_->setChecked(true);

    // Load camera
#ifdef HAVE_LIBCAMERA
    QString camera = CFG.read<std::string>("camera.cameraView", "Qt").c_str();
    bool useLibCamera = QString::compare(camera, "LibCamera", Qt::CaseInsensitive) == 0;
#else
    bool useLibCamera = false;
    libCameraButton_->setEnabled(false);
#endif
    if (useLibCamera)  libCameraButton_->setChecked(true);
    else qtCameraButton_->setChecked(true);

    // Focus mode will be "everytime" by default
    // Changing this will open the camera view to autofocus once
    everytimeFocusButton_->setChecked(true);
}

void SettingsView::saveConfig()
{
    // Save config and update save button
    CFG.save();
    buttons_->showButton(Buttons::SaveButton, CFG.changed());
}

void SettingsView::onThemeChanged(Button *button)
{
    // Write to config and change theme
    QString theme = button->objectName();
    CFG.write("colors.colorTheme", theme.toStdString());
    emit themeChanged(theme);

    // Show / hide save button if config changed
    buttons_->showButton(Buttons::SaveButton, CFG.changed());
}

void SettingsView::onCameraChanged(Button *button)
{
    // Write to config and change camera
    QString camera = button->objectName();
    CFG.write("camera.cameraView", camera.toStdString());
    emit cameraChanged(button == libCameraButton_);

    // Show / hide save button if config changed
    buttons_->showButton(Buttons::SaveButton, CFG.changed());
}

void SettingsView::onFocusChanged(Button *button)
{
    emit focusChanged(button == onceFocusButton_);
}

void SettingsView::onBrightnessChanged()
{
    // Brightness will be changed on valueChanged
    // but saved only on sliderReleased
    int b = brightnessSlider_->value();
    CFG.write("led.brightness", b);
    emit brightnessChanged(b);

    // Show save button
    buttons_->showButton(Buttons::SaveButton, true);
}

void SettingsView::onColorTemperatureChanged()
{
    // Color temperature will be changed on valueChanged
    // but saved only on sliderReleased
    int t = colorTemperatureSlider_->value();
    CFG.write("led.colorTemperature", t);
    emit colorTemperatureChanged(t);

    // Show save button
    buttons_->showButton(Buttons::SaveButton, true);
}

void SettingsView::resizeEvent(QResizeEvent *)
{
    buttons_->setGeometry(rect());
}

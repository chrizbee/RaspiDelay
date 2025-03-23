#ifndef SETTINGSVIEW_H
#define SETTINGSVIEW_H

#include <QWidget>

class Widget;
class Button;
class Slider;
class ButtonGroup;
class FloatingButtons;

class SettingsView : public QWidget
{
    Q_OBJECT

public:
    SettingsView(QWidget *parent = nullptr);

Q_SIGNALS:
    void themeChanged(const QString &theme);
    void cameraChanged(bool libCamera);
    void focusChanged(bool once);
    void brightnessChanged(uint8_t brightness);
    void colorTemperatureChanged(uint kelvin);
    void backPressed();

protected:
    void setupUi();
    void loadConfig();
    void saveConfig();
    void onThemeChanged(Button *button);
    void onCameraChanged(Button *button);
    void onFocusChanged(Button *button);
    void onBrightnessChanged();
    void onColorTemperatureChanged();
    void resizeEvent(QResizeEvent *) override;

private:
    Widget *themeWidget_;
    Button *darkThemeButton_;
    Button *lightThemeButton_;
    Button *customThemeButton_;
    ButtonGroup *themeGroup_;

    Widget *cameraWidget_;
    Button *qtCameraButton_;
    Button *libCameraButton_;
    ButtonGroup *cameraGroup_;

    Widget *focusWidget_;
    Button *onceFocusButton_;
    Button *everytimeFocusButton_;
    ButtonGroup *focusGroup_;

    Widget *brightnessWidget_;
    Slider *brightnessSlider_;

    Widget *colorTemperatureWidget_;
    Slider *colorTemperatureSlider_;

    FloatingButtons *buttons_;
};

#endif // SETTINGSVIEW_H

/*!
 * Copyright (c) 2016 J-P Nurmi
 * Copyright (c) 2023 Emeric Grange
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "MobileUI.h"
#include "MobileUI_private.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlEngine>
#include <QScreen>
#include <QWindow>
#include <QTimer>

/* ************************************************************************** */

bool MobileUI::isPhone = false;
bool MobileUI::isTablet = false;

MobileUI::Theme MobileUIPrivate::deviceTheme = MobileUI::Light;

QColor MobileUIPrivate::statusbarColor;
MobileUI::Theme MobileUIPrivate::statusbarTheme = MobileUI::Light;

QColor MobileUIPrivate::navbarColor;
MobileUI::Theme MobileUIPrivate::navbarTheme = MobileUI::Light;

bool MobileUIPrivate::screenAlwaysOn = false;

MobileUI::ScreenOrientation MobileUIPrivate::screenOrientation = MobileUI::Unlocked;

/* ************************************************************************** */

void MobileUI::registerQML()
{
    qRegisterMetaType<MobileUI::Theme>("MobileUI::Theme");
    qRegisterMetaType<MobileUI::ScreenOrientation>("MobileUI::ScreenOrientation");
}

/* ************************************************************************** */

MobileUI::MobileUI(QObject *parent) : QObject(parent)
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QScreen *screen = qApp->primaryScreen();
    if (screen)
    {
        double screenSizeInch = std::sqrt(std::pow(screen->physicalSize().width(), 2.0) +
                                          std::pow(screen->physicalSize().height(), 2.0)) / (2.54 * 10.0);

        if (screenSizeInch >= 7.0) MobileUI::isTablet = true;
        else  MobileUI::isPhone = true;
    }

    // The application window doesn't exist yet when this object is created from QML,
    // so defer the signal hookup and the first safe area computation until the event loop is running.
    QTimer::singleShot(0, this, [this]() { connectSignals(); refreshMobileUI(); });
#endif
}

/* ************************************************************************** */

void MobileUI::connectSignals()
{
    if (m_signalsConnected) return;

    QScreen *screen = qApp->primaryScreen();
    if (screen)
    {
        QObject::connect(screen, &QScreen::orientationChanged,
                         this, [this](Qt::ScreenOrientation) { refreshMobileUI(); });
    }

    const QWindowList windows = qApp->allWindows();
    QWindow *window = windows.isEmpty() ? nullptr : windows.first();
    if (window)
    {
        QObject::connect(window, &QWindow::visibilityChanged,
                         this, [this](QWindow::Visibility) { refreshMobileUI(); });
    }

    // The OS may reset the native system bar styles/colors when the application
    // returns to the foreground, so we re-apply them when becoming active again.
    QObject::connect(qApp, &QGuiApplication::applicationStateChanged,
                     this, [this](Qt::ApplicationState state) { if (state == Qt::ApplicationActive) refreshSafeAreas(); });

    // A light/dark mode change does not emit orientationChanged/visibilityChanged,
    // so make sure we re-apply our settings.
    if (QStyleHints *hints = qApp->styleHints())
    {
        QObject::connect(hints, &QStyleHints::colorSchemeChanged,
                         this, [this](Qt::ColorScheme) { refreshMobileUI(); });
    }

    m_signalsConnected = true;
}

/* ************************************************************************** */

void MobileUI::refreshMobileUI()
{
    // Re-apply the native bar colors / themes (lost on rotation or resume)
    refreshUI();

    // Re-compute safe areas (changed on rotation)
    refreshSafeAreas();

    // After an orientation or visibility change the native insets and bar sizes are not always settled immediately,
    // so re-read them a few times with increasing delays until they stabilize.
    for (int delay : {66, 256, 512, 1024})
    {
        QTimer::singleShot(delay, this, [this]() { refreshUI(); refreshSafeAreas(); });
    }
}

/* ************************************************************************** */

MobileUI::Theme MobileUI::getDeviceTheme()
{
    return static_cast<MobileUI::Theme>(MobileUIPrivate::getDeviceTheme());
}

/* ************************************************************************** */

QColor MobileUI::getStatusbarColor()
{
    return MobileUIPrivate::statusbarColor;
}

void MobileUI::setStatusbarColor(const QColor &color)
{
    if (color.isValid())
    {
        MobileUIPrivate::statusbarColor = color;
        MobileUIPrivate::setColor_statusbar(color);
    }
}

MobileUI::Theme MobileUI::getStatusbarTheme()
{
    return MobileUIPrivate::statusbarTheme;
}

void MobileUI::setStatusbarTheme(const MobileUI::Theme theme)
{
    MobileUIPrivate::statusbarTheme = theme;
    MobileUIPrivate::setTheme_statusbar(theme);
}

/* ************************************************************************** */

QColor MobileUI::getNavbarColor()
{
    return MobileUIPrivate::navbarColor;
}

void MobileUI::setNavbarColor(const QColor &color)
{
    if (color.isValid())
    {
        MobileUIPrivate::navbarColor = color;
        MobileUIPrivate::setColor_navbar(color);
    }
}

MobileUI::Theme MobileUI::getNavbarTheme()
{
    return MobileUIPrivate::navbarTheme;
}

void MobileUI::setNavbarTheme(const MobileUI::Theme theme)
{
    MobileUIPrivate::navbarTheme = theme;
    MobileUIPrivate::setTheme_navbar(theme);
}

/* ************************************************************************** */

void MobileUI::refreshUI()
{
    if (MobileUIPrivate::statusbarColor.isValid())
        MobileUIPrivate::setColor_statusbar(MobileUIPrivate::statusbarColor);

    if (MobileUIPrivate::navbarColor.isValid())
        MobileUIPrivate::setColor_navbar(MobileUIPrivate::navbarColor);

    MobileUIPrivate::setTheme_statusbar(MobileUIPrivate::statusbarTheme);
    MobileUIPrivate::setTheme_navbar(MobileUIPrivate::navbarTheme);
}

/* ************************************************************************** */

void MobileUI::refreshSafeAreas()
{
    int statusbar = MobileUIPrivate::getStatusbarHeight();
    int navbar = MobileUIPrivate::getNavbarHeight();
    int top = 0;
    int left = 0;
    int right = 0;
    int bottom = 0;

    const QWindowList windows = qApp->allWindows();
    QWindow *window = windows.isEmpty() ? nullptr : windows.first();

    const bool fullscreenMode = (window && window->visibility() == QWindow::FullScreen);
    const bool maximizedHint = (window && (window->flags() & Qt::MaximizeUsingFullscreenGeometryHint));

    // Safe areas are only meaningful when the window covers the system bars,
    // i.e. in full screen mode or when using the maximized geometry hint.

    if (fullscreenMode || maximizedHint)
    {
        top = MobileUIPrivate::getSafeAreaTop();
        left = MobileUIPrivate::getSafeAreaLeft();
        right = MobileUIPrivate::getSafeAreaRight();
        bottom = MobileUIPrivate::getSafeAreaBottom();
    }

    // When the window is in full screen mode, the system bars are hidden.

    if (fullscreenMode)
    {
        statusbar = 0;
        navbar = 0;
    }

    if (statusbar != m_statusbarHeight || navbar != m_navbarHeight ||
        top != m_safeAreaTop || left != m_safeAreaLeft ||
        right != m_safeAreaRight || bottom != m_safeAreaBottom)
    {
        m_statusbarHeight = statusbar;
        m_navbarHeight = navbar;
        m_safeAreaTop = top;
        m_safeAreaLeft = left;
        m_safeAreaRight = right;
        m_safeAreaBottom = bottom;

        Q_EMIT safeAreaUpdated();
    }
}

/* ************************************************************************** */

MobileUI::ScreenOrientation MobileUI::getScreenOrientation()
{
    return MobileUIPrivate::screenOrientation;
}

void MobileUI::setScreenOrientation(const MobileUI::ScreenOrientation orientation)
{
    MobileUIPrivate::screenOrientation = orientation;
    MobileUIPrivate::setScreenOrientation(orientation);

    // Forcing the screen orientation does not emit QScreen::orientationChanged,
    // so we refresh the safe areas ourselves
    MobileUI::getInstance()->refreshMobileUI();
}

bool MobileUI::getScreenAlwaysOn()
{
    return MobileUIPrivate::screenAlwaysOn;
}

void MobileUI::setScreenAlwaysOn(const bool value)
{
    MobileUIPrivate::screenAlwaysOn = value;
    MobileUIPrivate::setScreenAlwaysOn(value);
}

/* ************************************************************************** */

int MobileUI::getScreenBrightness()
{
    return MobileUIPrivate::getScreenBrightness();
}

void MobileUI::setScreenBrightness(const int value)
{
    return MobileUIPrivate::setScreenBrightness(value);
}

/* ************************************************************************** */

void MobileUI::vibrate()
{
    MobileUIPrivate::vibrate();
}

void MobileUI::backToHomeScreen()
{
    MobileUIPrivate::backToHomeScreen();
}

/* ************************************************************************** */

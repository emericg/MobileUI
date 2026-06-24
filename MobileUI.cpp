/*!
 * Copyright (c) 2016 J-P Nurmi
 * Copyright (c) 2026 Emeric Grange
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
#include <QDebug>

#include <cmath>

/* ************************************************************************** */

MobileUI *MobileUI::instance = nullptr;

MobileUI *MobileUI::getInstance()
{
    if (instance == nullptr)
    {
        instance = new MobileUI();
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    }

    return instance;
}

MobileUI *MobileUI::create(QQmlEngine *, QJSEngine *)
{
    return MobileUI::getInstance();
}

/* ************************************************************************** */

MobileUI::MobileUI(QObject *parent) : QObject(parent), d(std::make_unique<MobileUIPrivate>())
{
    // Set up the retry timers used by refreshMobileUI()
    for (unsigned i = 0; i < 4; ++i)
    {
        m_retryTimers[i].setSingleShot(true);
        m_retryTimers[i].setInterval(m_retryDelays[i]);
        connect(&m_retryTimers[i], &QTimer::timeout, this, [this]() {
            refreshSystemBars();
            refreshSafeAreas();
        });
    }

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    QScreen *screen = qApp->primaryScreen();
    if (screen)
    {
        double screenSizeInch = std::sqrt(std::pow(screen->physicalSize().width(), 2.0) +
                                          std::pow(screen->physicalSize().height(), 2.0)) / (2.54 * 10.0);

        if (screenSizeInch >= 7.0) m_isTablet = true;
        else m_isPhone = true;
    }

    // The application window doesn't exist yet when this object is created from QML,
    // so we defer the signal hookup and the first safe area computation until the event loop is running.
    QTimer::singleShot(0, this, [this]() {
        // connectSignals() must be called only ONCE
        connectSignals();

        refreshSystemBars();
        refreshSafeAreas();
        refreshDeviceTheme();
    });
#endif
}

MobileUI::~MobileUI() = default;

/* ************************************************************************** */

void MobileUI::connectSignals()
{
    // When orientation changes, we need to at least recompute the safe areas.

    QScreen *screen = qApp->primaryScreen();
    if (screen)
    {
        QObject::connect(screen, &QScreen::orientationChanged,
                         this, [this](Qt::ScreenOrientation) { refreshMobileUI(); });
    }

    // The OS may reset the native system bar styles/colors when the application
    // returns to the foreground, so we re-apply them when becoming active again.

    const QWindowList windows = qApp->allWindows();
    QWindow *window = windows.isEmpty() ? nullptr : windows.first();
    if (window)
    {
        QObject::connect(window, &QWindow::visibilityChanged,
                         this, [this](QWindow::Visibility) { refreshMobileUI(); });
    }

    QObject::connect(qApp, &QGuiApplication::applicationStateChanged,
                     this, [this](Qt::ApplicationState state) { if (state == Qt::ApplicationActive) refreshMobileUI(); });

    // A light/dark mode change does not emit orientationChanged/visibilityChanged,
    // so make sure we re-apply our settings.

    if (QStyleHints *hints = qApp->styleHints())
    {
        QObject::connect(hints, &QStyleHints::colorSchemeChanged,
                         this, [this](Qt::ColorScheme) { refreshMobileUI(); refreshDeviceTheme(); });
    }
}

/* ************************************************************************** */

void MobileUI::refreshMobileUI()
{
    // Re-apply the native bar colors / themes (lost on rotation or resume)
    refreshSystemBars();

    // Re-compute safe areas (changed on rotation)
    refreshSafeAreas();

    // After an orientation or visibility change the native insets and bar sizes are not always settled immediately,
    // so re-read them a few times with increasing delays until they stabilize.
    for (unsigned i = 0; i < 4; ++i)
    {
        m_retryTimers[i].start();
    }
}

/* ************************************************************************** */

void MobileUI::refreshDeviceTheme()
{
    const MobileUI::Theme theme = static_cast<MobileUI::Theme>(d->getDeviceTheme());
    if (theme != m_osTheme)
    {
        m_osTheme = theme;
        Q_EMIT devicethemeUpdated();
    }
}

/* ************************************************************************** */

QColor MobileUI::getStatusbarColor() const
{
    return m_statusbarColor;
}

void MobileUI::setStatusbarColor(const QColor &color)
{
    //qDebug() << "MobileUI::setStatusbarColor(" << color.name() << ") luminance:" << colorLuminance(color);
    if (!color.isValid()) return;

    bool changed = (m_statusbarColor != color);

    // we re-apply anyway, and battle with the OS fighting us...
    m_statusbarColor = color;
    d->setColor_statusbar(color);

    // Automatically derive a theme from the underlying color
    // If transparent, that responsability is best left to the user
    if (color.alpha() > 0)
    {
        const MobileUI::Theme theme = static_cast<MobileUI::Theme>(!isColorLight_android(color));
        if (theme != m_statusbarTheme)
        {
            m_statusbarTheme = theme;
            d->setTheme_statusbar(theme);
            changed = true;
        }
    }

    if (changed) Q_EMIT statusbarUpdated();
}

MobileUI::Theme MobileUI::getStatusbarTheme() const
{
    return m_statusbarTheme;
}

void MobileUI::setStatusbarTheme(const MobileUI::Theme theme)
{
    const bool changed = (theme != m_statusbarTheme);

    // we re-apply anyway, and battle with the OS fighting us...
    m_statusbarTheme = theme;
    d->setTheme_statusbar(theme);

    if (changed) Q_EMIT statusbarUpdated();
}

/* ************************************************************************** */

QColor MobileUI::getNavbarColor() const
{
    return m_navbarColor;
}

void MobileUI::setNavbarColor(const QColor &color)
{
    //qDebug() << "MobileUI::setNavbarColor(" << color.name() << ") luminance:" << colorLuminance(color);
    if (!color.isValid()) return;

    bool changed = (m_navbarColor != color);

    // we re-apply anyway, and battle with the OS fighting us...
    m_navbarColor = color;
    d->setColor_navbar(color);

    // Automatically derive a theme from the underlying color
    // If transparent, that responsability is best left to the user
    if (color.alpha() > 0)
    {
        const MobileUI::Theme theme = static_cast<MobileUI::Theme>(!isColorLight_android(color));
        if (theme != m_navbarTheme)
        {
            m_navbarTheme = theme;
            d->setTheme_navbar(theme);
            changed = true;
        }
    }

    if (changed) Q_EMIT navbarUpdated();
}

MobileUI::Theme MobileUI::getNavbarTheme() const
{
    return m_navbarTheme;
}

void MobileUI::setNavbarTheme(const MobileUI::Theme theme)
{
    const bool changed = (theme != m_navbarTheme);

    // we re-apply anyway, and battle with the OS fighting us...
    m_navbarTheme = theme;
    d->setTheme_navbar(theme);

    if (changed) Q_EMIT navbarUpdated();
}

/* ************************************************************************** */

void MobileUI::refreshSystemBars()
{
    if (m_statusbarColor.isValid())
        d->setColor_statusbar(m_statusbarColor);

    if (m_navbarColor.isValid())
        d->setColor_navbar(m_navbarColor);

    d->setTheme_statusbar(m_statusbarTheme);
    d->setTheme_navbar(m_navbarTheme);
}

/* ************************************************************************** */

void MobileUI::refreshSafeAreas()
{
    int statusbar = 0, navbar = 0, top = 0, left = 0, right = 0, bottom = 0;
    d->getSafeAreaMetrics(statusbar, navbar, top, left, right, bottom);

    const QWindowList windows = qApp->allWindows();
    QWindow *window = windows.isEmpty() ? nullptr : windows.first();
    const bool fullscreenMode = (window && window->visibility() == QWindow::FullScreen);

    // When the window is in "full screen" mode, the system bars are not shown
    if (fullscreenMode)
    {
        statusbar = 0;
        navbar = 0;
    }

    // iOS without "maximized geometry" has no available safe areas
#if defined (Q_OS_IOS)
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    const bool maximizedHint = (window && (window->flags() & Qt::ExpandedClientAreaHint));
#else
    const bool maximizedHint = (window && (window->flags() & Qt::MaximizeUsingFullscreenGeometryHint));
#endif

    if (!maximizedHint)
    {
        statusbar = 0;
        navbar = 0;
        top = 0;
        left = 0;
        right = 0;
        bottom = 0;
    }
#endif

    // Notify the UI safeAreas have changed, if needed
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

MobileUI::ScreenOrientation MobileUI::getScreenOrientation() const
{
    return m_screenOrientation;
}

void MobileUI::setScreenOrientation(const MobileUI::ScreenOrientation orientation)
{
    const bool changed = (orientation != m_screenOrientation);

    // we re-apply, the OS might have changed that on its own
    m_screenOrientation = orientation;
    d->setScreenOrientation(orientation);

    // Forcing the screen orientation does not emit QScreen::orientationChanged,
    // so we refresh the safe areas ourselves
    refreshMobileUI();

    if (changed) Q_EMIT screenUpdated();
}

bool MobileUI::getScreenAlwaysOn() const
{
    return m_screenAlwaysOn;
}

void MobileUI::setScreenAlwaysOn(const bool value)
{
    const bool changed = (value != m_screenAlwaysOn);

    // we re-apply, the OS might have changed that on its own
    m_screenAlwaysOn = value;
    d->setScreenAlwaysOn(value);

    if (changed) Q_EMIT screenUpdated();
}

/* ************************************************************************** */

int MobileUI::getScreenBrightness()
{
    return d->getScreenBrightness();
}

void MobileUI::setScreenBrightness(const int value)
{
    const bool changed = (value != m_screenBrightness);

    // we re-apply, the OS might have changed that on its own
    m_screenBrightness = value;
    d->setScreenBrightness(value);

    if (changed) Q_EMIT screenUpdated();
}

/* ************************************************************************** */

void MobileUI::vibrate()
{
    d->vibrate();
}

void MobileUI::backToHomeScreen()
{
    d->backToHomeScreen();
}

/* ************************************************************************** */

double MobileUI::colorLuminance(const QColor &color)
{
    return (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0;
}

bool MobileUI::isColorLight_android(const QColor &color)
{
    return colorLuminance(color) > 0.66;
}

bool MobileUI::isColorLight_hyperos(const QColor &color)
{
    return colorLuminance(color) > 0.5;
}

/* ************************************************************************** */

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

#ifndef MOBILEUI_H
#define MOBILEUI_H
/* ************************************************************************** */

#include <QtQml/qqmlregistration.h>
#include <QObject>
#include <QColor>
#include <QTimer>

#include <memory>

class QQmlEngine;
class QJSEngine;
class MobileUIPrivate;

/* ************************************************************************** */

/*!
 * \brief Mobile platform UI integration singleton.
 *
 * MobileUI lets a QML (or C++) application interact with mobile specific features
 * that have no equivalent in the standard Qt API:
 * - Android status bar and navigation bar colors and themes.
 * - iOS status bar theme (iOS has no status bar color, and no navigation bar).
 * - The device OS theme (light or dark mode).
 * - The screen safe areas (notches, cutouts, system bars, rounded corners).
 * - Screensaver inhibition, forced screen orientation and screen brightness.
 * - Haptic feedbacks.
 * - Android "back to home screen" helper.
 *
 * It is exposed to QML as a singleton and the very same instance is shared with C++
 * through getInstance(), so settings applied from either side stay consistent.
 *
 * You can use it without worries on desktop platforms, a dummy backend is used so
 * no code is runned, setters are ignored, getters return neutral/default values...
 */
class MobileUI : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool isPhone READ isDevicePhone CONSTANT)
    Q_PROPERTY(bool isTablet READ isDeviceTablet CONSTANT)

    Q_PROPERTY(Theme deviceTheme READ getDeviceTheme NOTIFY devicethemeUpdated)

    Q_PROPERTY(QColor statusbarColor READ getStatusbarColor WRITE setStatusbarColor NOTIFY statusbarUpdated)
    Q_PROPERTY(Theme statusbarTheme READ getStatusbarTheme WRITE setStatusbarTheme NOTIFY statusbarUpdated)
    Q_PROPERTY(int statusbarHeight READ getStatusbarHeight NOTIFY safeAreaUpdated)

    Q_PROPERTY(QColor navbarColor READ getNavbarColor WRITE setNavbarColor NOTIFY navbarUpdated)
    Q_PROPERTY(Theme navbarTheme READ getNavbarTheme WRITE setNavbarTheme NOTIFY navbarUpdated)
    Q_PROPERTY(int navbarHeight READ getNavbarHeight NOTIFY safeAreaUpdated)

    Q_PROPERTY(int safeAreaTop READ getSafeAreaTop NOTIFY safeAreaUpdated)
    Q_PROPERTY(int safeAreaLeft READ getSafeAreaLeft NOTIFY safeAreaUpdated)
    Q_PROPERTY(int safeAreaRight READ getSafeAreaRight NOTIFY safeAreaUpdated)
    Q_PROPERTY(int safeAreaBottom READ getSafeAreaBottom NOTIFY safeAreaUpdated)

    Q_PROPERTY(bool screenAlwaysOn READ getScreenAlwaysOn WRITE setScreenAlwaysOn NOTIFY screenUpdated)
    Q_PROPERTY(ScreenOrientation screenOrientation READ getScreenOrientation WRITE setScreenOrientation NOTIFY screenUpdated)
    Q_PROPERTY(int screenBrightness READ getScreenBrightness WRITE setScreenBrightness NOTIFY screenUpdated)

public:
    enum Theme {
        Light,  //!< Light application theme, usually light background and dark texts.
        Dark    //!< Dark application theme, usually dark background and light texts.
    };
    Q_ENUM(Theme)

    /*!
     * \brief Screen orientations that can be forced through setScreenOrientation().
     *
     * The values are bit flags, so a sensor mode is conceptually the union of
     * its two fixed orientations. These are used to *lock* the orientation;
     * they cannot be used to read the device's current physical orientation.
     */
    enum ScreenOrientation {
        Unlocked = 0,                       //!< Orientation is unlocked; the OS decides freely.

        Portrait            = (1 << 0),     //!< Locked to portrait, right side up.
        Portrait_upsidedown = (1 << 1),     //!< Locked to portrait, upside down.
        Portrait_sensor     = (1 << 2),     //!< Both portrait orientations, sensor driven (Android only; falls back to Portrait on iOS).

        Landscape_left      = (1 << 3),     //!< Locked to landscape left.
        Landscape_right     = (1 << 4),     //!< Locked to landscape right.
        Landscape_sensor    = (1 << 5),     //!< Both landscape orientations, sensor driven (Android only; falls back to Landscape on iOS).
    };
    Q_ENUM(ScreenOrientation)

Q_SIGNALS:
    //! Emitted when the device OS theme (light/dark mode) changes.
    void devicethemeUpdated();
    //! Emitted when a status bar color or theme is set.
    void statusbarUpdated();
    //! Emitted when a navigation bar color or theme is set.
    void navbarUpdated();
    //! Emitted when the system bar heights or the screen safe areas are changed (by or rotation or some other reason).
    void safeAreaUpdated();
    //! Emitted when a screen related property (always-on, orientation, brightness) is set.
    void screenUpdated();

public:
    /*!
     * \brief Get the process-wide MobileUI singleton instance.
     *
     * Use this to access the instance (and its cached safe area properties / signals) from C++.
     * The same instance is shared with QML.
     */
    static MobileUI *getInstance();

    /*!
     * \brief QML singleton factory.
     *
     * Called by the QML engine to obtain the MobileUI singleton.
     * It simply returns getInstance(), so C++ and QML share a single instance.
     */
    static MobileUI *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    // MobileUI ////////////////////////////////////////////////////////////////

    /*!
     * \brief Refresh UI themes/colors and recompute system bar sizes and screen safe areas.
     *
     * This is the main entry point, called automatically whenever the screen orientation,
     * window visibility or OS theme changes, so the exposed properties stay up to date on their own.
     *
     * You don't usually need to call this function manually, but you can.
     */
    Q_INVOKABLE void refreshMobileUI();

    // Device type /////////////////////////////////////////////////////////////

    /*!
     * \brief Tell whether the application runs on a phone-sized device.
     * \return true on a phone, false otherwise.
     *
     * The classification is based on the physical screen diagonal computed at startup:
     * - A diagonal below 7 inches is considered a phone.
     * - On desktop platforms this returns false.
     */
    bool isDevicePhone() const { return m_isPhone; }

    /*!
     * \brief Tell whether the application runs on a tablet-sized device.
     * \return true on a tablet, false otherwise.
     *
     * The classification is based on the physical screen diagonal computed at startup:
     * - A diagonal of 7 inches or more is considered a tablet.
     * - On desktop platforms this returns false.
     */
    bool isDeviceTablet() const { return m_isTablet; }

    // Device theme ////////////////////////////////////////////////////////////

    /*!
     * \brief Get the theme currently in effect on this device.
     * \return see MobileUI::Theme enum.
     */
    MobileUI::Theme getDeviceTheme() const { return m_osTheme; }

    /*!
     * \brief Read the (actual) device theme from the backend.
     */
    Q_INVOKABLE void refreshDeviceTheme();

    // System bars /////////////////////////////////////////////////////////////

    // Status bar

    /*!
     * \brief Get the last status bar color that was set.
     * \return the current status bar color.
     * \note Android only.
     * \note This is the value previously passed to setStatusbarColor(), not a value read back from the OS.
     */
    QColor getStatusbarColor() const;

    /*!
     * \brief Set the status bar background color.
     * \param color: the desired status bar color; may be "transparent". Invalid colors are ignored.
     * \note Android only (iOS has no notion of a status bar color).
     *
     * When the color is opaque, a matching Light/Dark theme is derived
     * automatically from its perceived luminance.
     * For a transparent color the theme is left to the caller.
     */
    void setStatusbarColor(const QColor &color);

    /*!
     * \brief Get the current status bar theme.
     * \return see MobileUI::Theme enum.
     * \note Android only.
     */
    MobileUI::Theme getStatusbarTheme() const;

    /*!
     * \brief Set the status bar foreground theme (icons/text contrast).
     * \param theme: see MobileUI::Theme enum.
     *
     * On iOS and Android the OS may reset this on visibility/orientation
     * changes, so MobileUI re-applies it automatically.
     */
    void setStatusbarTheme(const MobileUI::Theme theme);

    // Navigation bar

    /*!
     * \brief Get the last navigation bar color that was set.
     * \return the current navigation bar color.
     * \note Android only.
     *
     * This is the value previously passed to setNavbarColor(), not a value read back from the OS.
     */
    QColor getNavbarColor() const;

    /*!
     * \brief Set the navigation bar background color.
     * \param color: the desired navigation bar color; may be "transparent". Invalid colors are ignored.
     * \note Android only (iOS has no navigation bar).
     *
     * When the color is opaque, a matching Light/Dark theme is derived
     * automatically from its perceived luminance.
     * For a transparent color the theme is left to the caller.
     */
    void setNavbarColor(const QColor &color);

    /*!
     * \brief Get the current navigation bar theme.
     * \return see MobileUI::Theme enum.
     * \note Android only.
     */
    MobileUI::Theme getNavbarTheme() const;

    /*!
     * \brief Set the navigation bar foreground theme (icons contrast).
     * \param theme: see MobileUI::Theme enum.
     * \note Android only.
     *
     * The OS may reset this on visibility/orientation changes, so MobileUI re-applies it automatically.
     */
    void setNavbarTheme(const MobileUI::Theme theme);

    /*!
     * \brief Re-apply the status bar and navigation bar colors and themes.
     *
     * The native system bar styles can be resetted by the OS (for instance when
     * the application returns to the foreground, or on rotation), so this
     * pushes the cached colors/themes back to the platform.
     *
     * It is called automatically by refreshMobileUI() whenever the screen orientation,
     * window visibility or OS theme changes, and usually does not need be called by hand.
     */
    Q_INVOKABLE void refreshSystemBars();

    // Screen safe areas ///////////////////////////////////////////////////////

    /*!
     * \brief Get the status bar height.
     * \return the status bar height in pixels, or 0 when it is not visible (full screen mode).
     *
     * The status bar is always at the top of the screen when visible.
     */
    int getStatusbarHeight() const { return m_statusbarHeight; }

    /*!
     * \brief Get the navigation bar height.
     * \return the navigation bar height in pixels, or 0 when it is not visible (full screen mode).
     *
     * The navigation bar is usually at the bottom of the screen. When the
     * device is rotated and the bar moves to the left/right edge, this returns
     * 0 and the bar size is instead accounted for in the left/right safe areas.
     */
    int getNavbarHeight() const { return m_navbarHeight; }

    /*!
     * \brief Get the top screen safe area inset.
     * \return the top inset in pixels.
     *
     * Integrates the status bar height when the status bar is visible.
     */
    int getSafeAreaTop() const { return m_safeAreaTop; }

    /*!
     * \brief Get the left screen safe area inset.
     * \return the left inset in pixels.
     */
    int getSafeAreaLeft() const { return m_safeAreaLeft; }

    /*!
     * \brief Get the right screen safe area inset.
     * \return the right inset in pixels.
     */
    int getSafeAreaRight() const { return m_safeAreaRight; }

    /*!
     * \brief Get the bottom screen safe area inset.
     * \return the bottom inset in pixels.
     *
     * Integrates the navigation bar height when the navigation bar is visible
     * at the bottom of the screen.
     */
    int getSafeAreaBottom() const { return m_safeAreaBottom; }

    /*!
     * \brief Recompute system bar sizes and screen safe areas.
     *
     * It is called automatically by refreshMobileUI() whenever the screen orientation
     * or the window visibility changes, and usually does not need be called by hand.
     */
    Q_INVOKABLE void refreshSafeAreas();

    // Screen helpers //////////////////////////////////////////////////////////

    /*!
     * \brief Get orientation lock (if set).
     * \return See MobileUI::ScreenOrientation enum.
     */
    MobileUI::ScreenOrientation getScreenOrientation() const;

    /*!
     * \brief Lock (or unlock) the screen orientation.
     * \param orientation: see MobileUI::ScreenOrientation enum.
     * \note - On iOS the sensor modes are approximated: Landscape_sensor allows both landscape orientations,
     *         while Portrait_sensor falls back to a fixed Portrait.
     *       - Forcing orientation is also not allowed on iPads.
     *
     * You can also achieve similar functionality through application manifest or plist:
     * - https://developer.android.com/guide/topics/manifest/activity-element.html#screen
     * - https://developer.apple.com/documentation/bundleresources/information_property_list/uisupportedinterfaceorientations
     */
    Q_INVOKABLE void setScreenOrientation(const MobileUI::ScreenOrientation orientation);

    /*!
     * \brief Get screensaver lock (if set).
     * \return on or off.
     */
    bool getScreenAlwaysOn() const;

    /*!
     * \brief Lock screensaver.
     * \param value: on or off.
     */
    Q_INVOKABLE void setScreenAlwaysOn(const bool value);

    /*!
     * \brief Get screen brightness set for the current app (on Android) or system wide (on iOS).
     * \return screen brightness, from 0 to 100, or -1 when unavailable.
     *
     * If brightness has not been set for the current app, this function will return the OS wide brightness level.
     */
    int getScreenBrightness();

    /*!
     * \brief Set screen brightness for the current app (on Android) or system wide (on iOS).
     * \param value: screen brightness, from 0 to 100. Values are clamped to that range.
     */
    Q_INVOKABLE void setScreenBrightness(const int value);

    // Other helpers ///////////////////////////////////////////////////////////

    /*!
     * \brief Trigger an haptic feedback.
     * \note iPads don't support haptic feedbacks.
     * \note On Android the "android.permission.VIBRATE" must be added to the manifest.
     */
    Q_INVOKABLE void vibrate();

    /*!
     * \brief Go back to Android home screen.
     *
     * You can use this method to bypass the default behavior for the Android back button,
     * which is to kill the application instead of doing what every single Android application does,
     * going back to the home screen...
     */
    Q_INVOKABLE void backToHomeScreen();

    // Color helpers ///////////////////////////////////////////////////////////

    /*!
     * \brief Compute the perceived luminance of a color.
     * \param color: the color to evaluate.
     * \return the perceived luminance (Rec. 601), normalized to the [0.0 ; 1.0] range.
     */
    static double colorLuminance(const QColor &color);

    /*!
     * \brief Tell whether a color is perceived as "light" using the Android cutoff.
     * \param color: the color to evaluate.
     * \return true if the color is light enough to warrant dark foreground icons.
     *
     * Uses the luminance cutoff (~0.66) that matches Android's own behavior;
     * this is the rule MobileUI uses internally to auto-derive a bar theme.
     */
    static bool isColorLight_android(const QColor &color);

    /*!
     * \brief Tell whether a color is perceived as "light" using the HyperOS cutoff.
     * \param color: the color to evaluate.
     * \return true if the color is above the ~0.5 luminance cutoff.
     *
     * Xiaomi MIUI / HyperOS devices auto-contrast the status bar icons from the
     * bar background's perceived brightness, around a ~0.5 luminance cutoff.
     * Use this to predict which foreground those ROMs will pick.
     */
    static bool isColorLight_hyperos(const QColor &color);

private:
    // Device types
    bool m_isPhone = false;
    bool m_isTablet = false;

    // OS theme
    MobileUI::Theme m_osTheme = MobileUI::Light;

    // System bars states
    QColor m_statusbarColor;
    MobileUI::Theme m_statusbarTheme = MobileUI::Light;

    QColor m_navbarColor;
    MobileUI::Theme m_navbarTheme = MobileUI::Light;

    int m_statusbarHeight = 0;
    int m_navbarHeight = 0;

    // Screen safe areas
    int m_safeAreaTop = 0;
    int m_safeAreaLeft = 0;
    int m_safeAreaRight = 0;
    int m_safeAreaBottom = 0;

    // Screen states
    int m_screenBrightness = -1;
    bool m_screenAlwaysOn = false;
    MobileUI::ScreenOrientation m_screenOrientation = MobileUI::Unlocked;

    //! Connect to screen orientation, window visibility and theme changes.
    void connectSignals();

    /*!
     * \brief Restartable retry timers re-setting the system bars / re-reading safe areas as they settle.
     *
     * Several signals (orientation, visibility, app-active, color-scheme) usually fire
     * back-to-back during one rotation or resume.
     *
     * Restarting the same set of timers (rather than queuing a fresh batch on every call)
     * coalesces such a burst into a single settle cascade aligned to the latest change.
     */
    QTimer m_retryTimers[4];
    const int m_retryDelays[4] = {66, 256, 512, 1024};

    //! Per-platform backend.
    std::unique_ptr<MobileUIPrivate> d;

    // Singleton
    static MobileUI *instance;
    explicit MobileUI(QObject *parent = nullptr);
    ~MobileUI() override;
};

/* ************************************************************************** */
#endif // MOBILEUI_H

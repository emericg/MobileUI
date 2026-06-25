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

#include "MobileUI_private.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>

#include <QJniObject>

#include <cmath>

/* ************************************************************************** */

// WindowManager.LayoutParams
#define FLAG_KEEP_SCREEN_ON                     0x00000080
#define FLAG_TRANSLUCENT_STATUS                 0x04000000
#define FLAG_TRANSLUCENT_NAVIGATION             0x08000000
#define FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS       0x80000000

// View
#define SYSTEM_UI_FLAG_LAYOUT_STABLE            0x00000100
#define SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION   0x00000200
#define SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN        0x00000400
#define SYSTEM_UI_FLAG_LIGHT_STATUS_BAR         0x00002000
#define SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR     0x00000010

// UI modes
#define UI_MODE_NIGHT_UNDEFINED                 0x00000000
#define UI_MODE_NIGHT_NO                        0x00000010
#define UI_MODE_NIGHT_YES                       0x00000020
#define UI_MODE_NIGHT_MASK                      0x00000030

// WindowInsetsController
#define APPEARANCE_OPAQUE_STATUS_BARS           0x00000001
#define APPEARANCE_OPAQUE_NAVIGATION_BARS       0x00000002
#define APPEARANCE_LOW_PROFILE_BARS             0x00000004
#define APPEARANCE_LIGHT_STATUS_BARS            0x00000008
#define APPEARANCE_LIGHT_NAVIGATION_BARS        0x00000010
#define APPEARANCE_SEMI_TRANSPARENT_STATUS_BARS 0x00000020
#define APPEARANCE_SEMI_TRANSPARENT_NAVIGATION_BARS 0x0040

#define BEHAVIOR_SHOW_BARS_BY_TOUCH             0x00000000
#define BEHAVIOR_SHOW_BARS_BY_SWIPE             0x00000001
#define BEHAVIOR_DEFAULT                        0x00000001
#define BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE   0x00000002

// VibrationEffect
#define DEFAULT_AMPLITUDE                       0xffffffff
#define EFFECT_CLICK                            0x00000000
#define EFFECT_DOUBLE_CLICK                     0x00000001
#define EFFECT_TICK                             0x00000002
#define EFFECT_HEAVY_CLICK                      0x00000005

// Screen orientation
#define SCREEN_ORIENTATION_UNSPECIFIED         -1
#define SCREEN_ORIENTATION_LANDSCAPE            0
#define SCREEN_ORIENTATION_PORTRAIT             1
#define SCREEN_ORIENTATION_SENSOR_LANDSCAPE     6
#define SCREEN_ORIENTATION_SENSOR_PORTRAIT      7
#define SCREEN_ORIENTATION_REVERSE_LANDSCAPE    8
#define SCREEN_ORIENTATION_REVERSE_PORTRAIT     9

// Screen brightness
#define BRIGHTNESS_OVERRIDE_NONE               -1

/* ************************************************************************** */

static QJniObject getAndroidWindow()
{
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    return activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
}

static QJniObject getAndroidDecorView()
{
    return getAndroidWindow().callObjectMethod("getDecorView", "()Landroid/view/View;");
}

static QJniObject getInsetsController()
{
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 30)
    {
        // getInsetsController // Added in API level 30

        return getAndroidWindow().callObjectMethod("getInsetsController", "()Landroid/view/WindowInsetsController;");
    }

    return QJniObject();
}

static QJniObject getAndroidRootWindowInsets()
{
    // getRootWindowInsets // Added in API level 28

    return getAndroidDecorView().callObjectMethod("getRootWindowInsets", "()Landroid/view/WindowInsets;");
}

static QJniObject getDisplayCutout()
{
    QJniObject insets = getAndroidRootWindowInsets();
    if (insets.isValid())
    {
        // getDisplayCutout // Added in API level 28

        return insets.callObjectMethod("getDisplayCutout", "()Landroid/view/DisplayCutout;");
    }

    return QJniObject();
}

/* ************************************************************************** */

static int pxToDip(int px)
{
    return static_cast<int>(std::lround(px / qApp->devicePixelRatio()));
}

static int insetField(const QJniObject &insets, const char *insetType, const char *side)
{
    //if (QNativeInterface::QAndroidApplication::sdkVersion() < 30) return 0;
    if (!insets.isValid()) return 0;

    // Modern system bar height, via WindowInsets.Type
    // WindowInsets.Type // Added in API level 30
    // Call from Android thread!

    jint type = QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type", insetType, "()I");
    QJniObject inset = insets.callObjectMethod("getInsets", "(I)Landroid/graphics/Insets;", type);

    if (!inset.isValid()) return 0;

    return inset.getField<jint>(side);
}

static int dimenHeight(const char *name, int fallbackValue)
{
    //if (QNativeInterface::QAndroidApplication::sdkVersion() < 28) return fallbackValue;

    // Legacy system bar height fallback, via the platform dimension resource
    // Call from Android thread!

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject resources = activity.callObjectMethod("getResources", "()Landroid/content/res/Resources;");

    QJniObject jname = QJniObject::fromString(name);
    QJniObject jtype = QJniObject::fromString("dimen");
    QJniObject jpackage = QJniObject::fromString("android");

    int identifier = resources.callMethod<int>("getIdentifier", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
                                               jname.object<jstring>(), jtype.object<jstring>(), jpackage.object<jstring>());

    if (identifier > 0)
    {
        return pxToDip(resources.callMethod<int>("getDimensionPixelSize", "(I)I", identifier));
    }

    return fallbackValue;
}

/* ************************************************************************** */

int MobileUIPrivate::getDeviceTheme()
{
    return QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
            QJniObject activity = QNativeInterface::QAndroidApplication::context();
            QJniObject resources = activity.callObjectMethod("getResources", "()Landroid/content/res/Resources;");
            QJniObject conf = resources.callObjectMethod("getConfiguration", "()Landroid/content/res/Configuration;");

            int uiMode = (conf.getField<int>("uiMode") & UI_MODE_NIGHT_MASK);

            return (uiMode == UI_MODE_NIGHT_YES) ? MobileUI::Theme::Dark
                                                 : MobileUI::Theme::Light;
        })
    .result()
    .toInt();
}

/* ************************************************************************** */

void MobileUIPrivate::setColor_statusbar(const QColor &color)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        if (QNativeInterface::QAndroidApplication::sdkVersion() < 35)
        {
            // setStatusBarColor // Added in API level 21 // Deprecated in API level 35

            QJniObject window = getAndroidWindow();
            window.callMethod<void>("addFlags", "(I)V", FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            window.callMethod<void>("clearFlags", "(I)V", FLAG_TRANSLUCENT_STATUS);
            window.callMethod<void>("setStatusBarColor", "(I)V", color.rgba());
        }
    });
}

void MobileUIPrivate::setTheme_statusbar(const MobileUI::Theme theme)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        if (QNativeInterface::QAndroidApplication::sdkVersion() >= 30)
        {
            // setSystemBarsAppearance // Added in API level 30

            QJniObject inset = getInsetsController();
            if (inset.isValid())
            {
                const int appearance = (theme == MobileUI::Theme::Light) ? APPEARANCE_LIGHT_STATUS_BARS : 0;

                inset.callMethod<void>("setSystemBarsAppearance", "(II)V",
                                       appearance, APPEARANCE_LIGHT_STATUS_BARS);
            }
        }
        else if (QNativeInterface::QAndroidApplication::sdkVersion() < 30)
        {
            // setSystemUiVisibility // Added in API level 23 // Deprecated in API level 30

            QJniObject view = getAndroidDecorView();

            int visibility = view.callMethod<int>("getSystemUiVisibility", "()I");
            if (theme == MobileUI::Theme::Light)
                visibility |= SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
            else
                visibility &= ~SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;

            view.callMethod<void>("setSystemUiVisibility", "(I)V", visibility);
        }
    });
}

/* ************************************************************************** */

void MobileUIPrivate::setColor_navbar(const QColor &color)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        if (QNativeInterface::QAndroidApplication::sdkVersion() < 35)
        {
            // setNavigationBarColor // Deprecated in API level 35

            QJniObject window_android = getAndroidWindow();
            QWindow *window_qt = qApp->allWindows().isEmpty() ? nullptr : qApp->allWindows().first();

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            const bool maximizedHint = (window_qt && (window_qt->flags() & Qt::ExpandedClientAreaHint));
#else
            const bool maximizedHint = (window_qt && (window_qt->flags() & Qt::MaximizeUsingFullscreenGeometryHint));
#endif

            if (maximizedHint)
            {
                // if we try to set the FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS flag while in fullscreen mode, it will mess everything up
                window_android.callMethod<void>("addFlags", "(I)V", FLAG_TRANSLUCENT_NAVIGATION);
                window_android.callMethod<void>("setNavigationBarColor", "(I)V", color.rgba());
            }
            else
            {
                // set color
                window_android.callMethod<void>("addFlags", "(I)V", FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
                window_android.callMethod<void>("clearFlags", "(I)V", FLAG_TRANSLUCENT_NAVIGATION);
                window_android.callMethod<void>("setNavigationBarColor", "(I)V", color.rgba());
            }
        }
    });
}

void MobileUIPrivate::setTheme_navbar(const MobileUI::Theme theme)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        if (QNativeInterface::QAndroidApplication::sdkVersion() >= 30)
        {
            // setSystemBarsAppearance // Added in API level 30

            QJniObject inset = getInsetsController();
            if (inset.isValid())
            {
                const int appearance = (theme == MobileUI::Theme::Light) ? APPEARANCE_LIGHT_NAVIGATION_BARS : 0;

                inset.callMethod<void>("setSystemBarsAppearance", "(II)V",
                                       appearance, APPEARANCE_LIGHT_NAVIGATION_BARS);
            }
        }
        else if (QNativeInterface::QAndroidApplication::sdkVersion() < 30)
        {
            // getSystemUiVisibility // Added in API level 23 // Deprecated in API level 30

            QJniObject view = getAndroidDecorView();

            int visibility = view.callMethod<int>("getSystemUiVisibility", "()I");
            if (theme == MobileUI::Theme::Light)
                visibility |= SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
            else
                visibility &= ~SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;

            view.callMethod<void>("setSystemUiVisibility", "(I)V", visibility);
        }
    });
}

/* ************************************************************************** */

void MobileUIPrivate::getSafeAreaMetrics(int &statusbarHeight, int &navbarHeight,
                                         int &top, int &left, int &right, int &bottom)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([&]() -> void {
        QJniObject insets = getAndroidRootWindowInsets(); // called just once

        if (!insets.isValid()) {
            statusbarHeight = navbarHeight = top = left = right = bottom = 0;
            return;
        }

        const int sdk = QNativeInterface::QAndroidApplication::sdkVersion();

        // System bars: the top/bottom inset
        // side-mounted bars in landscape are folded into the left/right safe areas below

        if (sdk >= 30)
        {
            // status/navigation bar is the top/bottom inset; a side-mounted bar in
            // landscape reads 0 here and is folded into the left/right safe areas below.
            statusbarHeight = pxToDip(insetField(insets, "statusBars", "top"));
            navbarHeight = pxToDip(insetField(insets, "navigationBars", "bottom"));
        }
        else
        {
            statusbarHeight = dimenHeight("status_bar_height", 24);
            navbarHeight = dimenHeight("navigation_bar_height", 48);
        }

        // Screen safe areas: the display cutout
        // widened by any side-mounted system bar in landscape on the left/right edges

        QJniObject cutout = insets.callObjectMethod("getDisplayCutout", "()Landroid/view/DisplayCutout;");
        if (cutout.isValid())
        {
            top = pxToDip(cutout.callMethod<int>("getSafeInsetTop", "()I"));
            left = pxToDip(cutout.callMethod<int>("getSafeInsetLeft", "()I"));
            right = pxToDip(cutout.callMethod<int>("getSafeInsetRight", "()I"));
            bottom = pxToDip(cutout.callMethod<int>("getSafeInsetBottom", "()I"));

            if (sdk >= 30)
            {
                const int barLeft = pxToDip(insetField(insets, "systemBars", "left"));
                const int barRight = pxToDip(insetField(insets, "systemBars", "right"));
                if (barLeft > left) left = barLeft;
                if (barRight > right) right = barRight;
            }
        }
    }).waitForFinished();
}

/* ************************************************************************** */

int MobileUIPrivate::getKeyboardHeight()
{
    return QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> int {
            // WindowInsets.Type.ime() // Added in API level 30
            if (QNativeInterface::QAndroidApplication::sdkVersion() < 30) return -1;

            QJniObject insets = getAndroidRootWindowInsets();
            if (!insets.isValid()) return -1;

            return pxToDip(insetField(insets, "ime", "bottom"));
        })
    .result()
    .toInt();
}

/* ************************************************************************** */

int MobileUIPrivate::getScreenBrightness()
{
    return QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
            // If a brightness override has been set for the current app window, use it.
            // screenBrightness is a float in [0.0, 1.0], or -1 when unset.
            QJniObject layoutParams = getAndroidWindow().callObjectMethod("getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
            float brightnessApp = layoutParams.getField<jfloat>("screenBrightness");
            if (brightnessApp >= 0.f) return static_cast<int>(std::lround(brightnessApp * 100.f));

            // Otherwise, fall back to the OS-wide brightness
            // Settings.System.SCREEN_BRIGHTNESS is an integer in [0, 255].
            constexpr jint brightnessUnavailable = BRIGHTNESS_OVERRIDE_NONE;
            QJniObject activity = QNativeInterface::QAndroidApplication::context();
            QJniObject contentResolver = activity.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
            QJniObject SCREEN_BRIGHTNESS = QJniObject::getStaticObjectField("android/provider/Settings$System",
                                                                            "SCREEN_BRIGHTNESS", "Ljava/lang/String;");
            jint brightnessOS = QJniObject::callStaticMethod<jint>("android/provider/Settings$System", "getInt",
                                                                   "(Landroid/content/ContentResolver;Ljava/lang/String;I)I",
                                                                   contentResolver.object(), SCREEN_BRIGHTNESS.object<jstring>(),
                                                                   brightnessUnavailable);

            if (brightnessOS < 0) return -1; // unavailable

            constexpr float brightnessMax = 255.f;
            return static_cast<int>(std::lround((brightnessOS / brightnessMax) * 100.f));
        })
    .result()
    .toInt();
}

void MobileUIPrivate::setScreenBrightness(const int value)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        QJniObject window = getAndroidWindow();
        QJniObject layoutParams = window.callObjectMethod("getAttributes", "()Landroid/view/WindowManager$LayoutParams;");

        // A negative value releases the app override and hands brightness back to the system.
        float brightness = BRIGHTNESS_OVERRIDE_NONE;
        if (value >= 0)
        {
            brightness = value / 100.f; // screenBrightness is 0.0 to 1.0
            if (brightness > 1.0f) brightness = 1.0f;
        }

        layoutParams.setField("screenBrightness", brightness);
        window.callMethod<void>("setAttributes", "(Landroid/view/WindowManager$LayoutParams;)V", layoutParams.object());
    });
}

/* ************************************************************************** */

void MobileUIPrivate::setScreenLockOrientation(const MobileUI::ScreenLockOrientation orientation)
{
    int value = SCREEN_ORIENTATION_UNSPECIFIED;

    if (orientation == MobileUI::Portrait) value = SCREEN_ORIENTATION_PORTRAIT;
    else if (orientation == MobileUI::Portrait_upsidedown) value = SCREEN_ORIENTATION_REVERSE_PORTRAIT;
    else if (orientation == MobileUI::Portrait_sensor) value = SCREEN_ORIENTATION_SENSOR_PORTRAIT;
    else if (orientation == MobileUI::Landscape_left) value = SCREEN_ORIENTATION_LANDSCAPE;
    else if (orientation == MobileUI::Landscape_right) value = SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
    else if (orientation == MobileUI::Landscape_sensor) value = SCREEN_ORIENTATION_SENSOR_LANDSCAPE;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([value]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
        {
            activity.callMethod<void>("setRequestedOrientation", "(I)V", value);
}
    });
}

void MobileUIPrivate::setScreenAlwaysOn(const bool on)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        QJniObject window = getAndroidWindow();

        if (on)
            window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
        else
            window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    });
}

/* ************************************************************************** */

static QJniObject getVibrator()
{
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) return QJniObject();

    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 31)
    {
        // vibrator_manager // Added in API level 31

        QJniObject name = QJniObject::fromString("vibrator_manager");
        QJniObject manager = activity.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                                       name.object<jstring>());
        if (manager.isValid())
        {
            return manager.callObjectMethod("getDefaultVibrator", "()Landroid/os/Vibrator;");
        }
        return QJniObject();
    }

    QJniObject name = QJniObject::fromString("vibrator");
    return activity.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                     name.object<jstring>());
}

static void doVibrate(jint predefinedEffect, jlong oneShotMs)
{
    QJniObject vibrator = getVibrator();
    if (!vibrator.isValid() || !vibrator.callMethod<jboolean>("hasVibrator", "()Z")) return;

    QJniObject effect;
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 29)
    {
        // createPredefined() // Added in API level 29

        // a nicer, system-tuned effect (EFFECT_TICK/CLICK/HEAVY_CLICK/DOUBLE_CLICK)
        effect = QJniObject::callStaticObjectMethod("android/os/VibrationEffect",
                                                    "createPredefined", "(I)Landroid/os/VibrationEffect;",
                                                    predefinedEffect);
    }
    else
    {
        // createOneShot() // Added in API level 26

        effect = QJniObject::callStaticObjectMethod("android/os/VibrationEffect",
                                                    "createOneShot", "(JI)Landroid/os/VibrationEffect;",
                                                    oneShotMs, static_cast<jint>(DEFAULT_AMPLITUDE));
    }

    if (effect.isValid())
    {
        vibrator.callMethod<void>("vibrate", "(Landroid/os/VibrationEffect;)V", effect.object<jobject>());
    }
}

void MobileUIPrivate::triggerHapticFeedback(const MobileUI::HapticFeedback type)
{
    // Android has no notification/selection haptics, so we map each style to
    // the closest predefined effect (or one-shot duration on API 28).

    jint effect = EFFECT_TICK;
    jlong ms = 25;

    switch (type)
    {
    case MobileUI::HapticSelection:
        effect = EFFECT_TICK;
        ms = 20;
        break;
    case MobileUI::HapticLight:
        effect = EFFECT_TICK;
        ms = 20;
        break;
    case MobileUI::HapticMedium:
        effect = EFFECT_CLICK;
        ms = 30;
        break;
    case MobileUI::HapticHeavy:
        effect = EFFECT_HEAVY_CLICK;
        ms = 40;
        break;
    case MobileUI::HapticSuccess:
        effect = EFFECT_HEAVY_CLICK;
        ms = 30;
        break;
    case MobileUI::HapticWarning:
        effect = EFFECT_DOUBLE_CLICK;
        ms = 40;
        break;
    case MobileUI::HapticError:
        effect = EFFECT_DOUBLE_CLICK;
        ms = 60;
        break;
}

    doVibrate(effect, ms);
}

/* ************************************************************************** */

void MobileUIPrivate::backToHomeScreen()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([=]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
        {
            // Sends the application to the background
            activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", true);
        }
    });
}

/* ************************************************************************** */

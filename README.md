# MobileUI

MobileUI allows QML applications to interact with Mobile specific features, like Android and iOS `status bar` and Android `navigation bar`.

You can see it in action in the [MobileUI demo](https://github.com/emericg/MobileUI_demo).

> Supports Qt6 and Qt5.

> Supports iOS 11 and up (tested with iOS 16).

> Supports Android API 21 and up (tested with API 33).

> Works with QMake and CMake.

## Features

- Get Android OS theme
- Set Android `status bar` and Android `navigation bar` color and theme
- Set iOS `status bar` color and theme
- Get device `safe areas` (WIP)
- Lock screensaver
- Set screen orientation
- Trigger device vibration

## Quick start

### Build

To get started, simply checkout the MobileUI repository as a submodule, or copy the
MobileUI directory into your project, then include the library files with either
the `MobileUI.pro` QMake project file or the `CMakeLists.txt` CMake project file.

```qmake
include(MobileUI/MobileUI.pri)
```

```cmake
add_subdirectory(MobileUI/)
target_link_libraries(${PROJECT_NAME} MobileUI::MobileUI)
```

### Use

First, you need to register the MobileUI QML module in your C++ main.cpp file.  
You can also use MobileUI directly in the C++ code if you want to.  

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <MobileUI>

int main() {
    QGuiApplication app();

    MobileUI::registerQML(); // that is required

    MobileUI::setStatusbarColor("white"); // use it directly if you want

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

    return app.exec();
}
```

Example usage in QML:

```qml
import QtQuick
import MobileUI

ApplicationWindow {
    MobileUI {
        statusbarTheme: MobileUI.Light
        statusbarColor: "white"
        navbarColor: "white"
    }
}
```

There is no navigation bar on iOS obviously, so it won't have any effects there.

## Licensing

This project is licensed under the [MIT license](LICENSE).

This project is based on [qtstatusbar](https://github.com/jpnurmi/qtstatusbar) by jpnurmi.

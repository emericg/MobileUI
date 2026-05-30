cmake_minimum_required(VERSION 3.20)

project(MobileUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

################################################################################

# Generic sources
set(CORE_SOURCES MobileUI.h MobileUI.cpp)

# OS specific sources & dependencies
if(ANDROID)
    if(QT_DEFAULT_MAJOR_VERSION EQUAL 6)
        set(PLATFORM_SOURCES MobileUI_android_qt6.cpp)
    elseif(QT_DEFAULT_MAJOR_VERSION EQUAL 5)
        find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS AndroidExtras)
        set(PLATFORM_LIBRARIES Qt::AndroidExtras)
        set(PLATFORM_SOURCES MobileUI_android_qt5.cpp)
    endif()
elseif(IOS)
    set(PLATFORM_SOURCES MobileUI_ios.mm)
    set(PLATFORM_LIBRARIES "-framework UIKit")
else()
    set(PLATFORM_SOURCES MobileUI_dummy.cpp)
endif()

################################################################################

if(NOT QT_DEFAULT_MAJOR_VERSION)
    set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt version to use (5 or 6), defaults to 6")
endif()

find_package(Qt${QT_DEFAULT_MAJOR_VERSION} REQUIRED COMPONENTS Core Gui Qml)

add_library(MobileUI OBJECT ${CORE_SOURCES} ${PLATFORM_SOURCES})
add_library(MobileUI::MobileUI ALIAS MobileUI)

target_link_libraries(MobileUI PUBLIC Qt::Core Qt::Quick Qt::Qml)
target_link_libraries(MobileUI PRIVATE ${PLATFORM_LIBRARIES})

target_include_directories(MobileUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

################################################################################

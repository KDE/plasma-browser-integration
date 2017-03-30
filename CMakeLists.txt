project(asdf)
set(PROJECT_VERSION "5.1.90")
set(PROJECT_VERSION_MAJOR 5)

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
set(QT_MIN_VERSION "5.3.0")

find_package(ECM 0.0.11 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMInstallIcons)
include(ECMMarkAsTest)
include(ECMMarkNonGuiExecutable)
include(FeatureSummary)
include(CheckIncludeFiles)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
Gui
DBus
Widgets
)

set(KF5_MIN_VERSION "5.2.0")
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    KIO
    Notifications
)

add_subdirectory(host)

configure_file(org.kde.plasma.chrome_integration.json.in org.kde.plasma.chrome_integration.json @ONLY)

#chromium
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION /etc/chromium/native-messaging-hosts/)
#google-chrome
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION /etc/opt/chrome/native-messaging-hosts/)

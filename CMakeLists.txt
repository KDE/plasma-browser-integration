cmake_minimum_required(VERSION 3.0)

project(plasma-browser-integration)
set(PROJECT_VERSION "5.12.80")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.9.0")
set(KF5_MIN_VERSION "5.42.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
Gui
DBus
Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    KIO
    I18n
    Notifications
    Runner
    WindowSystem
    Activities
)

# Options
option(INSTALL_CHROME_MANIFEST "Whether to install a configuration file that makes Chrome automatically download the extension from the store" FALSE)
add_feature_info(INSTALL_CHROME_MANIFEST ${INSTALL_CHROME_MANIFEST} "Install extension from Chrome store automatically")

add_subdirectory(host)
add_subdirectory(tabsrunner)

if(NOT DEFINED CHROME_EXTENSION_ID)
    # The extension ID is based on the key used to sign the extension
    # see https://stackoverflow.com/questions/23873623/obtaining-chrome-extension-id-for-development
    set(CHROME_EXTENSION_ID "cimiefiiaegbelhefglklhhakcgmhkai")
endif()

# TODO configure manifest.json

configure_file(org.kde.plasma.chrome_integration.json.in org.kde.plasma.chrome_integration.json @ONLY)
configure_file(org.kde.plasma.firefox_integration.json.in org.kde.plasma.firefox_integration.json @ONLY)

# #chromium
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION ${KDE_INSTALL_FULL_SYSCONFDIR}/chromium/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
# #google-chrome
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION ${KDE_INSTALL_FULL_SYSCONFDIR}/opt/chrome/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
# firefox

#these have to match where firefox is installed, not where p-b-i is installed
find_path(FIREFOX_PATH NAMES firefox)
if(FIREFOX_PATH)
    set(MOZILLA_PREFIX ${FIREFOX_PATH}/..)
else()
    set(MOZILLA_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.firefox_integration.json DESTINATION ${MOZILLA_PREFIX}/lib/mozilla/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)

if (INSTALL_CHROME_MANIFEST) # Install a policy to have browsers automatically add the extension
    # google-chrome
    install(FILES chrome_install_from_store_policy DESTINATION ${KDE_INSTALL_DATADIR}/google-chrome/extensions RENAME ${CHROME_EXTENSION_ID}.json)
    # chromium
    install(FILES chrome_install_from_store_policy DESTINATION ${KDE_INSTALL_DATADIR}/chromium-browser/extensions RENAME ${CHROME_EXTENSION_ID}.json)
endif()

# TODO firefox

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

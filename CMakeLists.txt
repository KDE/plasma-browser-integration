cmake_minimum_required(VERSION 3.16)

set(PROJECT_VERSION "6.4.80")

project(plasma-browser-integration VERSION ${PROJECT_VERSION})

set(PROJECT_DEP_VERSION "6.3.90")
set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.8.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} )

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMDeprecationSettings)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
Gui
DBus
Widgets
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    KIO
    I18n
    CoreAddons
    Config
    Crash
    DBusAddons
    Notifications
    Runner
    Purpose
    FileMetaData
    JobWidgets
    Service
    StatusNotifierItem
)

find_package(PlasmaActivities ${PROJECT_DEP_VERSION} CONFIG REQUIRED)
find_package(LibTaskManager ${PROJECT_DEP_VERSION} CONFIG REQUIRED)

add_definitions(-DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT)
ecm_set_disabled_deprecation_versions(QT 6.8.1
    KF 6.9.0
)

ecm_setup_version(PROJECT VARIABLE_PREFIX PBI
                  VERSION_HEADER ${CMAKE_BINARY_DIR}/version.h
                  )

# Options
option(INSTALL_CHROME_MANIFEST "Whether to install a configuration file that makes Chrome automatically download the extension from the store" FALSE)
add_feature_info(INSTALL_CHROME_MANIFEST ${INSTALL_CHROME_MANIFEST} "Install extension from Chrome store automatically")

# in ubuntu and derivatives, this should be installed in the chromium-browser/extensions,
# but since this is distribution specific path, let's use what upstream uses
# ideally ubuntu and derivatives should patch this in their packaging
set(CHROMIUM_EXTENSIONS_DIR "chromium" CACHE STRING "Directory name to install Chromium extensions into")
add_feature_info(CHROMIUM_EXTENSIONS_DIR On "Directory name to install Chromium extensions into is '${CHROMIUM_EXTENSIONS_DIR}'")

option(COPY_MESSAGING_HOST_FILE_HOME "Copy the native messaging hosts json file to user home dir" FALSE)
add_feature_info(COPY_MESSAGING_HOST_FILE_HOME ${COPY_MESSAGING_HOST_FILE_HOME} "Enable this option to copy the native messaging hosts json file to home dir if you install plasma-browser-integration to custom prefix (non-/usr)")

set(MOZILLA_DIR "${CMAKE_INSTALL_PREFIX}/lib/mozilla" CACHE STRING "Mozilla directory")
add_feature_info(MOZILLA_DIR On "Mozilla directory is '${MOZILLA_DIR}'")

set(LIBREWOLF_DIR "${CMAKE_INSTALL_PREFIX}/lib/librewolf" CACHE STRING "LibreWolf directory")
add_feature_info(LIBREWOLF_DIR On "LibreWolf directory is '${LIBREWOLF_DIR}'")

add_subdirectory(host)
add_subdirectory(reminder)
add_subdirectory(flatpak-integrator)

if(NOT DEFINED CHROME_EXTENSION_ID)
    # The extension ID is based on the key used to sign the extension
    # see https://stackoverflow.com/questions/23873623/obtaining-chrome-extension-id-for-development
    set(CHROME_EXTENSION_ID "cimiefiiaegbelhefglklhhakcgmhkai")
endif()

if(NOT DEFINED EDGE_EXTENSION_ID)
    set(EDGE_EXTENSION_ID "dnnckbejblnejeabhcmhklcaljjpdjeh")
endif()

# TODO configure manifest.json

configure_file(org.kde.plasma.chrome_integration.json.in org.kde.plasma.chrome_integration.json @ONLY)
configure_file(org.kde.plasma.firefox_integration.json.in org.kde.plasma.firefox_integration.json @ONLY)

# #chromium
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION ${KDE_INSTALL_FULL_SYSCONFDIR}/chromium/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
# #google-chrome
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION ${KDE_INSTALL_FULL_SYSCONFDIR}/opt/chrome/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
# Microsoft Edge
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION ${KDE_INSTALL_FULL_SYSCONFDIR}/opt/edge/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
# firefox
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.firefox_integration.json DESTINATION ${MOZILLA_DIR}/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
# LibreWolf
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.firefox_integration.json DESTINATION ${LIBREWOLF_DIR}/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)

if (COPY_MESSAGING_HOST_FILE_HOME)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.firefox_integration.json DESTINATION $ENV{HOME}/.mozilla/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.firefox_integration.json DESTINATION $ENV{HOME}/.librewolf/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION $ENV{HOME}/.config/chromium/NativeMessagingHosts/ RENAME org.kde.plasma.browser_integration.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION $ENV{HOME}/.config/google-chrome/NativeMessagingHosts/ RENAME org.kde.plasma.browser_integration.json)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.chrome_integration.json DESTINATION $ENV{HOME}/.config/microsoft-edge/NativeMessagingHosts/ RENAME org.kde.plasma.browser_integration.json)
endif()

if (INSTALL_CHROME_MANIFEST) # Install a policy to have browsers automatically add the extension
    # google-chrome
    install(FILES chrome_install_from_store_policy DESTINATION ${KDE_INSTALL_DATADIR}/google-chrome/extensions RENAME ${CHROME_EXTENSION_ID}.json)
    # chromium
    install(FILES chrome_install_from_store_policy DESTINATION ${KDE_INSTALL_DATADIR}/${CHROMIUM_EXTENSIONS_DIR}/extensions RENAME ${CHROME_EXTENSION_ID}.json)
endif()

# TODO firefox

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

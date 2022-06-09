cmake_minimum_required(VERSION 3.16)

project(plasma-browser-integration)
set(PROJECT_VERSION "5.25.0")
set(PROJECT_VERSION_MAJOR 5)

set(QT_MIN_VERSION "5.15.0")
set(KF5_MIN_VERSION "5.90.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} )

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
Gui
DBus
Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    KIO
    I18n
    CoreAddons
    Config
    Crash
    DBusAddons
    Notifications
    Runner
    Activities
    Purpose
    FileMetaData
    JobWidgets
)

find_package(LibTaskManager ${PROJECT_VERSION} CONFIG REQUIRED)

add_definitions(-DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT)
#add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x060000)

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

add_subdirectory(host)
add_subdirectory(reminder)

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

if (COPY_MESSAGING_HOST_FILE_HOME)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.firefox_integration.json DESTINATION $ENV{HOME}/.mozilla/native-messaging-hosts/ RENAME org.kde.plasma.browser_integration.json)
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

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

add_definitions(-DTRANSLATION_DOMAIN=\"plasma-browser-integration-host\")

configure_file(config-host.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-host.h)

set(HOST_SOURCES main.cpp
                connection.cpp
                pluginmanager.cpp
                settings.cpp
                mprisplugin.cpp
                abstractbrowserplugin.cpp
                abstractkrunnerplugin.cpp
                kdeconnectplugin.cpp
                downloadplugin.cpp
                downloadjob.cpp
                historyrunnerplugin.cpp
                tabsrunnerplugin.cpp
                purposeplugin.cpp
                connection.h
                pluginmanager.h
                settings.h
                mprisplugin.h
                abstractbrowserplugin.h
                abstractkrunnerplugin.h
                kdeconnectplugin.h
                downloadplugin.h
                downloadjob.h
                historyrunnerplugin.h
                tabsrunnerplugin.h
                purposeplugin.h
                )

qt_add_dbus_adaptor(HOST_SOURCES ../dbus/org.mpris.MediaPlayer2.xml mprisplugin.h MPrisPlugin mprisroot MPrisRoot)
qt_add_dbus_adaptor(HOST_SOURCES ../dbus/org.mpris.MediaPlayer2.Player.xml mprisplugin.h MPrisPlugin mprisplayer MPrisPlayer)

qt_add_dbus_adaptor(HOST_SOURCES ../dbus/org.kde.krunner1.xml abstractkrunnerplugin.h AbstractKRunnerPlugin)

add_executable(plasma-browser-integration-host ${HOST_SOURCES})
target_link_libraries(
    plasma-browser-integration-host
    Qt::DBus
    Qt::Gui
    Qt::Widgets
    KF6::Activities
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::Crash
    KF6::I18n
    KF6::JobWidgets
    KF6::KIOCore
    KF6::PurposeWidgets
    KF6::Runner
    KF6::FileMetaData
    KF6::Service
    PW::LibTaskManager
)

configure_file(org.kde.plasma.browser_integration.host.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.browser_integration.host.desktop @ONLY)

install(TARGETS plasma-browser-integration-host ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.plasma.browser_integration.host.desktop DESTINATION ${KDE_INSTALL_APPDIR})

install(FILES plasma-runner-browsertabs.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)
install(FILES plasma-runner-browserhistory.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)

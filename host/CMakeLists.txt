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
                )

qt5_add_dbus_adaptor(HOST_SOURCES ../dbus/org.mpris.MediaPlayer2.xml mprisplugin.h MPrisPlugin mprisroot MPrisRoot)
qt5_add_dbus_adaptor(HOST_SOURCES ../dbus/org.mpris.MediaPlayer2.Player.xml mprisplugin.h MPrisPlugin mprisplayer MPrisPlayer)

qt5_add_dbus_adaptor(HOST_SOURCES ../dbus/org.kde.krunner1.xml abstractkrunnerplugin.h AbstractKRunnerPlugin)

add_executable(plasma-browser-integration-host ${HOST_SOURCES})
target_link_libraries(
    plasma-browser-integration-host
    Qt5::DBus
    Qt5::Gui
    Qt5::Widgets
    KF5::Activities
    KF5::CoreAddons
    KF5::Crash
    KF5::I18n
    KF5::KIOCore
    KF5::PurposeWidgets
    KF5::Runner
    KF5::FileMetaData
    PW::LibTaskManager
)

install(TARGETS plasma-browser-integration-host ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES plasma-runner-browsertabs.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)
install(FILES plasma-runner-browserhistory.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)

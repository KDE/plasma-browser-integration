include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(FOO_SOURCES main.cpp
                mpris.cpp
                mprisplayer.cpp
                connection.cpp
                abstractbrowserplugin.cpp
                incognitoplugin.cpp
                kdeconnectplugin.cpp
                downloadplugin.cpp
                downloadjob.cpp
                )

add_executable(plasma-browser-integration-host ${FOO_SOURCES})
target_link_libraries(plasma-browser-integration-host Qt5::DBus Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::Notifications)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

install(TARGETS plasma-browser-integration-host ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

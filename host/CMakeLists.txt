include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(FOO_SOURCES main.cpp downloadjob.cpp mpris.cpp mprisplayer.cpp)
add_executable(plasma-chrome-integration-host ${FOO_SOURCES})
target_link_libraries(plasma-chrome-integration-host Qt5::DBus Qt5::Gui Qt5::Widgets KF5::KIOCore KF5::Notifications)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

# install(TARGETS compositorplugin DESTINATION ${KDE_INSTALL_QMLDIR}/org/dave)
# install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/org/dave)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(kded_browserlaunchwatcher browserlaunchwatcher.cpp )

add_library(browserlaunchwatcher MODULE ${kded_browserlaunchwatcher})
kcoreaddons_desktop_to_json(browserlaunchwatcher browserlaunchwatcher.desktop)


target_link_libraries(browserlaunchwatcher Qt5::DBus KF5::CoreAddons KF5::DBusAddons KF5::I18n Qt5::Widgets KF5::Notifications KF5::ConfigCore)

install(TARGETS browserlaunchwatcher  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded)

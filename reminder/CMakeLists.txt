add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(kded_browserintegrationreminder browserintegrationreminder.cpp )

add_library(browserintegrationreminder MODULE ${kded_browserintegrationreminder})
kcoreaddons_desktop_to_json(browserintegrationreminder browserintegrationreminder.desktop)


target_link_libraries(browserintegrationreminder
    Qt5::DBus
    Qt5::Widgets
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::I18n
    KF5::Notifications
    KF5::ConfigCore
    KF5::Activities
    KF5::KIOWidgets
    KF5::WindowSystem)

install(TARGETS browserintegrationreminder  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded)

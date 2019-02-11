add_definitions(-DTRANSLATION_DOMAIN=\"plasma-browser-integration-reminder\")

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(kded_browserintegrationreminder browserintegrationreminder.cpp )

add_library(browserintegrationreminder MODULE ${kded_browserintegrationreminder})
kcoreaddons_desktop_to_json(browserintegrationreminder browserintegrationreminder.desktop)


target_link_libraries(browserintegrationreminder
    Qt5::DBus
    Qt5::Widgets
    KF5::CoreAddons
    KF5::DBusAddons # for KDEDModule
    KF5::I18n
    KF5::Notifications # for KStatusNotifierItem
    KF5::ConfigCore
    KF5::Activities # for KActivities::ResourceInstance
    KF5::KIOWidgets # for KRun
)

install(TARGETS browserintegrationreminder  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kded)

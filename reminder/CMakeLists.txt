add_definitions(-DTRANSLATION_DOMAIN=\"plasma-browser-integration-reminder\")

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(kded_browserintegrationreminder browserintegrationreminder.cpp browserintegrationreminder.h)

add_library(browserintegrationreminder MODULE ${kded_browserintegrationreminder})

target_link_libraries(browserintegrationreminder
    Qt::DBus
    Qt::Widgets
    KF6::CoreAddons
    KF6::DBusAddons # for KDEDModule
    KF6::I18n
    KF6::Notifications # for KStatusNotifierItem
    KF6::ConfigCore
    KF6::Activities # for KActivities::ResourceInstance
    KF6::KIOGui # for KIO::ApplicationLauncherJob
    KF6::StatusNotifierItem
)

install(TARGETS browserintegrationreminder  DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)

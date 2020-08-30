add_definitions(-DTRANSLATION_DOMAIN=\"plasma_runner_browsertabs\")

set(krunner_browsertabs_SRCS
    tabsrunner.cpp
)

add_library(krunner_browsertabs MODULE ${krunner_browsertabs_SRCS})
target_link_libraries(krunner_browsertabs
    Qt5::DBus
    KF5::I18n
    KF5::Runner
)

kcoreaddons_desktop_to_json(krunner_browsertabs plasma-runner-browsertabs.desktop)

install(TARGETS krunner_browsertabs DESTINATION "${KDE_INSTALL_PLUGINDIR}/kf5/krunner")

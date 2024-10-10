# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

add_definitions(-DTRANSLATION_DOMAIN=\"plasma-browser-integration-flatpak-integrator\")

add_library(browserintegrationflatpakintegrator MODULE plugin.cpp data.qrc)
target_link_libraries(browserintegrationflatpakintegrator KF6::DBusAddons KF6::CoreAddons)
install(TARGETS browserintegrationflatpakintegrator DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)

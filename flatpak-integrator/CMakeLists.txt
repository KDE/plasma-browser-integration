# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

add_library(browserintegrationflatpakintegrator MODULE plugin.cpp data.qrc)
target_link_libraries(browserintegrationflatpakintegrator KF6::DBusAddons KF6::CoreAddons)
install(TARGETS browserintegrationflatpakintegrator DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/kded)

ecm_qt_declare_logging_category(
    browserintegrationflatpakintegrator
    HEADER debug.h
    IDENTIFIER INTEGRATOR
    CATEGORY_NAME org.kde.plasma.browser-integration.flatpak.integrator
    DESCRIPTION "Plasma Browser Integration Flatpak Integrator"
)

#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: 2020-2024 Harald Sitter <sitter@kde.org>

# Our l10n scripting isn't working with spaces anywhere and we actively rely on word splitting in our Messages.sh.
# shellcheck disable=SC2046

podir=${podir:?} # ensure it is defined

$XGETTEXT $(find . -name \*.cpp -o -name \*.h) -o "$podir"/plasma-browser-integration-flatpak-integrator.pot
# Extract JavaScripty files as what they are, otherwise for example template literals won't work correctly (by default we extract as C++).
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Template_literals
$XGETTEXT --join-existing --language=JavaScript $(find . -name \*.qml -o -name \*.js) -o "$podir"/plasma-browser-integration-flatpak-integrator.pot

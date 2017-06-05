#!/bin/sh

# This script requires
# https://github.com/i18next/i18next-gettext-converter

# We fill in the en "translations" manually. We extract this to the KDE system as pot as normal
# then on release, download them all and run i18next-conv to turn them all back into JSON in all the languages

# This is outside the extension folder so it can be zipped upped and deployed without build system stuff

i18next-conv -l en -s ./extension/_locales/en/messages.json -t $podir/plasma-browser-extension.pot

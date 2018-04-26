#!/bin/env python3

import json
import datetime
import sys

if len(sys.argv) < 2:
    print ("Usage ./convertpottojson output_pot_file")
    sys.exit(-1)

potFileName = sys.argv[1]

jsonFilename = "./extension/_locales/en/messages.json"

outfile = open(potFileName, 'w')
with open(jsonFilename, 'r') as infile:
    data = json.load(infile)

outfile.write('msgid ""\n')
outfile.write('''msgstr ""
"Project-Id-Version: PACKAGE VERSION\\n"
"Report-Msgid-Bugs-To: http://bugs.kde.org\\n"
"POT-Creation-Date: %s\\n"
"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n"
"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n"
"Language-Team: LANGUAGE <kde-i18n-doc@kde.org>\\n"
"Language: \\n"
"MIME-Version: 1.0\\n"
"Content-Type: text/plain; charset=UTF-8\\n"
"Content-Transfer-Encoding: 8bit"\n\n''' % datetime.datetime.now())

def writeEntry(key, value):
    value = value.replace('"', '\\\"')
    outfile.write('{0} "{1}"\n'.format(key, value))

for msgid in data:
    msg = data[msgid]
    outfile.write('\n')
    outfile.write('#: %s:0\n' % msgid)

    if "description" in msg:
        writeEntry('msgctxt', msg["description"])
    writeEntry('msgid', msg["message"])
    writeEntry('msgstr', '')


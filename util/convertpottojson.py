#!/bin/env python3

import json
import datetime
import os, sys

if len(sys.argv) < 3:
    print ("Usage ./convertpottojson input_pot_file language")
    sys.exit(-1)

potFileName = sys.argv[1]
lang = sys.argv[2]
path = "./extension/_locales/%s" % lang
enPath = "./extension/_locales/en/messages.json"

os.makedirs(path, exist_ok=True)
outfile = open(path + "/messages.json" , 'w')

translations = {}
currentGroup = {}
currentId = ""
currentMsg = ""

with open(potFileName, 'r') as infile:
    for line in infile.readlines():
        line = line.strip()

        if not line:
            if not currentId.isspace() and not currentMsg.isspace():
                translations[currentId] = currentMsg
            currentId = ""
            currentMsg = ""
            continue
        #split at first space
        parts = line.split(' ', 1)
        if len(parts) != 2:
            continue
        if parts[0] == "#:":
            currentId = parts[1].split(":")[0]
        elif parts[0] == "msgstr":
            msg = parts[1].strip('\"')
            msg = msg.replace('\\\"', '\"')
            currentMsg = msg
        else:
            msg = line.strip('\"')
            msg = msg.replace('\\\"', '\"')
            currentMsg += msg


outTranslations = {}

with open(enPath, 'r') as infile:
    enData = json.load(infile)

for msgId in enData:
    msg = ""
    if msgId in translations:
        msg = translations[msgId]
    if not msg:
        msg = enData[msgId]["message"]
    outTranslations[msgId] = {"message" : msg}

outfile.write(json.JSONEncoder(indent=4, ensure_ascii=False).encode(outTranslations))

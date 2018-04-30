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

os.makedirs(path, exist_ok=True)
outfile = open(path + "/messages.json" , 'w')

data = []
currentGroup = {}

with open(potFileName, 'r') as infile:
    for line in infile.readlines():
        line = line.strip()

        if not line:
            data.append(currentGroup)
            currentGroup = {}
            continue
        #split at first space
        parts = line.split(' ', 1)
        if len(parts) != 2:
            continue
        if parts[0] == "#:":
            currentGroup["id"] = parts[1].split(":")[0]
        if parts[0] == "msgstr":
            msg = parts[1].strip('\"')
            msg = msg.replace('\\\"', '\"')
            currentGroup["message"] = msg


jsonData = {}

for d in data:
    if not 'id' in d or not 'message' in d:
        continue
    jsonData[d["id"]] = {"message" : d["message"]}

outfile.write(json.JSONEncoder(indent=4, ensure_ascii=False).encode(jsonData))

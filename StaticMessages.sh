#!/usr/bin/env bash

# We fill in the en "translations" manually. We extract this to the KDE system as pot as normal, then populate the other json files


# The name of catalog we create (without the.pot extension), sourced from the scripty scripts
FILENAME="plasma-browser-extension"

function export_pot_file # First parameter will be the path of the pot file we have to create, includes $FILENAME
{
    potfile=$1
    python3 ./util/convertjsontopot.py $potfile
}

function import_po_files # First parameter will be a path that will contain several .po files with the format LANG.po
{
    podir=$1
    for file in `ls $podir`
    do
        lang=${file%.po} #remove .po from end of file
        python3 ./util/convertpottojson.py $podir/$file $lang
    done
}


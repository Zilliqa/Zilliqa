#!/bin/bash

set -e

if [ -z "$1" ] || [ ! -f "$1" ]
then
    echo Usage: $0 XMLFILE
    exit 1
fi

export XMLLINT_INDENT="    "

formatted=/tmp/$(basename $1).formatted

xmllint $1 --format > $formatted

diff $1 $formatted

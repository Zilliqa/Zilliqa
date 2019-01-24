#!/bin/bash
# Copyright (C) 2019 Zilliqa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

title=$1
webhook=$2
doonce=$3
last_record="x"

function doreport() {
    time_now=`date +'%y-%m-%dT%T'`
    secs_now=`date +%s -d ${time_now}`
    slack_input="*$title*"
    slack_input="$slack_input (as of $time_now)"

    slack_input="$slack_input\n\`\`\`\nPoW submission count:"

    latest_record=`tac state-00001-log.txt | grep -m1 "\[POW\]"`
    if [ "$latest_record" = "$last_record" ]
    then
        echo "No new record since last check"
        return
    fi

    last_record=$latest_record

    time_record=`echo $last_record | cut -d' ' -f 2`
    secs_record=`date +%s -d ${time_record}`
    slack_input="$slack_input\n$last_record ("`expr ${secs_now} - ${secs_record}`" seconds ago)\`\`\`"

    slack_output=$( curl -X POST -H 'Content-type: application/json' --data "{\"text\":\"$slack_input\"}" $webhook )

    echo "$slack_output"
}

function print_usage() {
cat <<EOF
Usage: $0 <report title> <webhook URL> [do once]

Set [do once] to 1 to avoid the loop on reporting

EOF
}

if [ -z "$title" ] || [ -z "$webhook" ]
then
    print_usage
    exit 1
fi

if [ "$doonce" = "1" ]
then
    echo "Generating report once only"
    doreport
else
    echo "Generating report every 20 minutes (when new records appear)"
    while true; do
        doreport
        sleep 1200
    done
fi
#!/bin/bash

prog=$(basename $0)
mykeyfile=mykey.txt
myaddrfile=myaddr.txt
whitelistkeyfile=whitelistkey.txt
cmd_log=last.log

function initialize_variables() {

noninteractive="$NONINTERACTIVE"
name="zilliqa"
ip=""
port="30303"
extseedprivk=""
zilliqa_path="$ZILLIQA_PATH"
zilliqa_binary_path=""
storage_path="`dirname \"$0\"`"
storage_path="`( cd \"$MY_PATH\" && pwd )`"
base_path="$storage_path"
# Will be modified by bootstrap.py
testnet_name='devnet'
# Will be modified by bootstrap.py
exclusion_txbodies_mb="true"
# Will be modified by bootstrap.py
bucket_name='zilliqa-devnet'
# Will be modified by bootstrap.py
isSeed="false"
}

function check_path() {
if [ -z "$zilliqa_path" -o ! -x $zilliqa_binary_path/zilliqa ]
then
    echo "Cannot find zilliqa binary or the path specified!"
    exit 1
fi
}

function validate_keyfile() {
if [ ! -s $mykeyfile ]
then
    echo "Cannot find $mykeyfile, generating new keypair for you..."
    $zilliqa_binary_path/genkeypair > $mykeyfile
fi

prikey=$(cat $mykeyfile | awk '{ print $2 }')
pubkey=$(cat $mykeyfile | awk '{ print $1 }')

$zilliqa_binary_path/getaddr --pubk $pubkey > $myaddrfile
}

function validate_whitelist_keyfile() {

# Assumes whitelist node key file if no whitelist keyfile is found.
if [ ! -s $whitelistkeyfile ]
then
    echo "Cannot find $whitelistkeyfile, using mykeyfile"
    whitelistkeyfile="$mykeyfile"
fi
whitelistprikey=$(cat $whitelistkeyfile | awk '{ print $2 }')
whitelistpubkey=$(cat $whitelistkeyfile | awk '{ print $1 }')
}

function non_interactive_setup() {

# non interactive setup assumptions
# Docker running path if you're running this script inside a container: /run/zilliqa
# mounted key file if available is: /run/zilliqa/mykey.txt
# zilliqa path is: /zilliqa
# zilliqa binary path is: /usr/local/bin
# if whitelist_privatekey is missing, will assume node key
# environment variables:
# NONINTERACTIVE
# IP_ADDRESS
# IP_WHITELISTING

zilliqa_path="/zilliqa"
zilliqa_binary_path="/usr/local/bin"

check_path

storage_path="/run/zilliqa"
ip="$IP_ADDRESS"
port="30303"

validate_keyfile


if [ "$isSeed" = "true" ]
then
    validate_whitelist_keyfile
    extseedprivk="$whitelistprikey"
fi

}

function interactive_setup() {

echo -n "Enter the full path of your zilliqa source code directory: " && read path_read && [ -n "$path_read" ] && zilliqa_path=$path_read
zilliqa_binary_path="$zilliqa_path/build/bin"

check_path

echo -n "Enter the full path for persistence storage (default: current working directory): " && read path_read && [ -n "$path_read" ] && storage_path=$path_read
echo -n "Enter your IP address (*.*.*.*): " && read ip_read && [ -n "$ip_read" ] && ip=$ip_read

validate_keyfile

if [ "$isSeed" = "true" ]
then
    echo -n "Enter the private key (32-byte hex string) to be used by this node and whitelisted by upper seeds: " && read extseedprivk_read && [ -n "$extseedprivk_read" ] && extseedprivk=$extseedprivk_read
fi

}

function run() {

initialize_variables

if [ "$noninteractive" = "true" ]
then
    non_interactive_setup
else
    interactive_setup
fi

cmd="cp ${zilliqa_path}/scripts/download_incr_DB.py ${base_path}/download_incr_DB.py; cp ${zilliqa_path}/scripts/download_static_DB.py ${base_path}/download_static_DB.py; sed -i \"/TESTNET_NAME=/c\TESTNET_NAME= '${testnet_name}'\" ${base_path}/download_incr_DB.py ${base_path}/download_static_DB.py; sed -i \"/BUCKET_NAME=/c\BUCKET_NAME= '${bucket_name}'\" ${base_path}/download_incr_DB.py ${base_path}/download_static_DB.py; o1=\$(grep -oPm1 '(?<=<NUM_FINAL_BLOCK_PER_POW>)[^<]+' ${base_path}/constants.xml); [ ! -z \$o1 ] && sed -i \"/NUM_FINAL_BLOCK_PER_POW=/c\NUM_FINAL_BLOCK_PER_POW= \$o1\" ${base_path}/download_incr_DB.py; o1=\$(grep -oPm1 '(?<=<INCRDB_DSNUMS_WITH_STATEDELTAS>)[^<]+' ${base_path}/constants.xml); [ ! -z \$o1 ] && sed -i \"/NUM_DSBLOCK=/c\NUM_DSBLOCK= \$o1\" ${base_path}/download_incr_DB.py"
eval ${cmd}

if [ ! -z "$storage_path" ]; then
 # patch constant for STORAGE_PATH
 tag="STORAGE_PATH"
 tag_value=$(grep "<$tag>.*<.$tag>" constants.xml | sed -e "s/^.*<$tag/<$tag/" | cut -f2 -d">"| cut -f1 -d"<")
 # Replacing element value with new storage path
 sed -i -e "s|<$tag>$tag_value</$tag>|<$tag>$storage_path</$tag>|g" constants.xml
fi

cmd="zilliqa --privk $prikey --pubk $pubkey --address $ip --port $port --synctype 1"

cmd="$cmd --l2lsyncmode --extseedprivk $extseedprivk"

$zilliqa_binary_path/$cmd > $cmd_log 2>&1 &

echo
echo "Use 'cat $cmd_log' to see the command output"
echo "Use 'tail -f zilliqa-00001-log.txt' to see the runtime log"
}

function cleanup() {
rm -rfv "*-log.txt"
}

function usage() {
cat <<EOF
Usage: $prog [OPTIONS]

Options:
    --cleanup               remove log files
    --help                  show this help message
EOF
}

case "$1" in
    "") run;;
    --cleanup) cleanup;;
    --help) usage ;;
    *) echo "Unrecognized option '$1'"; usage ;;
esac

#!/bin/bash

folderName='historicalDB'
TESTNET_NAME=''
image='zilliqa/zilliqa:testnetv3'

########################################
# try install the dependencies
apt install python-pip
pip install request clint

function common()
{
python fetchHistorical.py $folderName $TESTNET_NAME


cd $folderName

if [ ! -s $keyfile ]
then
	echo "Cannot find $keyfile"
	return 1
fi

}

function getkeypair()
{
	keyfile="verifier.txt"

	if [ ! -s $keyfile ]
	then
		echo "Cannot find $keyfile"
		return 1
	fi
	echo "$(cat $keyfile | awk '{ print $2, $1 }')"
	
}


function native_run()
{

name="zilliqa"
zilliqa_path="$ZILLIQA_PATH"
ip="127.0.0.1"
port="1234"

echo -n "Enter the full path of your zilliqa source code directory: " && read path_read && [ -n "$path_read" ] && zilliqa_path=$path_read

key_param=($(getkeypair))
common

prikey=${key_param[0]}
pubkey=${key_param[1]}

cmd="zilliqa --privk $prikey --pubk $pubkey --address $ip --port $port --synctype 8"

cmd_log="verifier_log.txt"

if [ -z "$zilliqa_path" ] || ([ ! -x $zilliqa_path/build/bin/zilliqa ] && [ ! -x $zilliqa_path/bin/zilliqa ]); then
    echo "Cannot find zilliqa binary on the path you specified"
    exit 1
fi

if [ -x $zilliqa_path/build/bin/zilliqa ]; then
    $zilliqa_path/build/bin/$cmd > $cmd_log 2>&1 &
elif [ -x $zilliqa_path/bin/zilliqa ]; then
    $zilliqa_path/bin/$cmd > $cmd_log 2>&1 &
fi

}


function docker_run()
{
	key_param=($(getkeypair))
	common
	workdir=/run/zilliqa
	name="zilliqa_verif"
	prikey=${key_param[0]}
	pubkey=${key_param[1]}
	ip="127.0.0.1"
	port="1234"
	cmd="zilliqa --privk $prikey --pubk $pubkey --address $ip --port $port --synctype 8"
	sudo docker run --network host --rm -v $(pwd):$workdir -w $workdir --name $name $image -c "$cmd"
}

function usage()
{
	echo -e "Run with --docker or --native"
}

case "$1" in
    --docker) docker_run;;
    --native) native_run;;
    *) echo "Unrecognized option '$1'"; usage ;;
esac

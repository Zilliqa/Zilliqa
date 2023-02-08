#!/bin/bash

prog=$(basename $0)
mykeyfile=mykey.txt
myaddrfile=myaddr.txt
image=zilliqa/zilliqa:testnetv3
os=$(uname)
testnet_name='devnet'
exclusion_txbodies_mb="true"
bucket_name='zilliqa-devnet'
isSeed="false"

case "$os" in
    Linux)
        # we should be good on Linux
        ;;
    Darwin)
        echo "This script doesn not support Docker for Mac"
        exit 1
        ;;
    *)
        echo "This script does not support Docker on your platform"
        exit 1
        ;;
esac

function genkeypair() {
if [ -s $mykeyfile ]
then
    echo -n "$mykeyfile exist, overwrite [y/N]?"
    read confirm && [ "$confirm" != "yes" -a "$confirm" != "y" ] && return
fi
sudo docker run --rm $image -c genkeypair > $mykeyfile
}

function run() {

name="zilliqa"
ip=""
port="30303"
multiplier_sync="Y"
extseedprivk=""

if [ "$1" = "cuda" ]
then
    cuda_docker="--runtime=nvidia"
    image="$image-cuda"
fi

workdir=/run/zilliqa

if [ ! -s $mykeyfile ]
then
    echo "Cannot find $mykeyfile, generating new keypair for you..."
    sudo docker run $image -c genkeypair > $mykeyfile && echo "$mykeyfile generated"
fi

prikey=$(cat $mykeyfile | awk '{ print $2 }')
pubkey=$(cat $mykeyfile | awk '{ print $1 }')
echo -n "Assign a name to your container (default: $name): " && read name_read && [ -n "$name_read" ] && name=$name_read
echo -n "Enter your IP address (*.*.*.*): " && read ip_read && [ -n "$ip_read" ] && ip=$ip_read
echo -n "Enter your listening port (default: $port): " && read port_read && [ -n "$port_read" ] && port=$port_read
if [ "$isSeed" = "true" ]
then
   echo -n "Use IP whitelisting registration approach (default: $multiplier_sync): " && read sync_read && [ -n "$sync_read" ] && multiplier_sync=$sync_read

   if [ "$multiplier_sync" = "N" ] || [ "$multiplier_sync" = "n" ]
   then
       echo -n "Enter the private key (32-byte hex string) to be used by this node and whitelisted by upper seeds: " && read extseedprivk_read && [ -n "$extseedprivk_read" ] && extseedprivk=$extseedprivk_read
   fi
fi

MY_PATH="`dirname \"$0\"`"
MY_PATH="`( cd \"$MY_PATH\" && pwd )`"

#FIXME-LATER -Now replace TESTNET_NAME in download_incr_DB.py directly because it will be invoked by zilliqa process in some cases and
#             and will need the testnet name.
sudo docker run $image -c "getaddr --pubk $pubkey" > $myaddrfile

sudo docker run $cuda_docker --network host -d -v $(pwd):$workdir -w $workdir --name $name $image -c "tail -f /dev/null"
sleep 5
 
cmd="cp /zilliqa/scripts/download_incr_DB.py /run/zilliqa/download_incr_DB.py && cp /zilliqa/scripts/download_static_DB.py /run/zilliqa/download_static_DB.py && sed -i \"/TESTNET_NAME=/c\TESTNET_NAME= '${testnet_name}'\" /run/zilliqa/download_incr_DB.py /run/zilliqa/download_static_DB.py && sed -i \"/BUCKET_NAME=/c\BUCKET_NAME= '${bucket_name}'\" /run/zilliqa/download_incr_DB.py /run/zilliqa/download_static_DB.py && o1=\$(grep -oPm1 '(?<=<NUM_FINAL_BLOCK_PER_POW>)[^<]+' /run/zilliqa/constants.xml) && [ ! -z \$o1 ] && sed -i \"/NUM_FINAL_BLOCK_PER_POW=/c\NUM_FINAL_BLOCK_PER_POW= \$o1\" /run/zilliqa/download_incr_DB.py && o1=\$(grep -oPm1 '(?<=<INCRDB_DSNUMS_WITH_STATEDELTAS>)[^<]+' /run/zilliqa/constants.xml) && [ ! -z \$o1 ] && sed -i \"/NUM_DSBLOCK=/c\NUM_DSBLOCK= \$o1\" /run/zilliqa/download_incr_DB.py && zilliqa --privk $prikey --pubk $pubkey --address $ip --port $port --synctype 1"

if [ "$multiplier_sync" = "N" ] || [ "$multiplier_sync" = "n" ]
then
   cmd="$cmd --l2lsyncmode --extseedprivk $extseedprivk" 
fi

echo "Running in docker : ${name} with command: '$cmd'"
sudo docker exec -d -w $workdir $name bash -c "$cmd"

# Please note - Do not remove below line
# RUN DAEMON COMMAND FOR SEED

echo
echo "Use 'docker ps' to check the status of the docker"
echo "Use 'docker stop $name' to terminate the container"
echo "Use 'tail -f zilliqa-00001-log.txt' to see the runtime log"
}

function cleanup() {
rm -rfv "*-log.txt"
}

function usage() {
cat <<EOF
Usage: $prog [OPTIONS]
Options:
    --genkeypair            generate new keypair and saved to '$mykeyfile'
    --cuda                  use nvidia-docker for mining
    --cleanup               remove log files
    --help                  show this help message
EOF
}

case "$1" in
    "") run;;
    --genkeypair) genkeypair;;
    --cleanup) cleanup;;
    --cuda) run cuda;;
    --help) usage;;
    *) echo "Unrecongized option '$1'"; usage;;
esac

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

#######################################################################################
# This script should be only used by Zilliqa Research,                                #
# for releasing a draft version binary with relative version information onto GitHub. #
#######################################################################################

# [MUST BE FILLED IN] User configuration settings
privKeyFile=""
pubKeyFile=""
constantFile=""
constantLookupFile=""
constantLevel2LookupFile=""
constantNewLookupFile=""
S3ReleaseTarBall=""

useNewUpgradeMethod="Y"
applyUpgrade="N"

# [MUST BE FILLED IN] Only when using old upgrading method (useNewUpgradeMethod = "N"), need to take care on these fields
GitHubToken="empty_token"
packageName="package"
releaseTitle="title"
releaseDescription="description"

# [MUST BE FILLED IN] Only when applying upgrading immediately (applyUpgrade = "Y"), need to take care on these fields
testnet="empty_testnet"
current_cluster_name="empty_cluster" # eg: dev.k8s.z7a.xyz
#optional for auto upload the persistent data to S3 DB and ask all nodes to download from S3
S3PersistentDBFileName="empty_testnet" ## need not be same as testnet name ex: tesnet multiregion - multi_m1 and multi_m2
                                 ## and backup script would create DB with filename multi.tar.gz. So in this case
                                 ## S3PersistentDBFileName should be 'multi'  
lookup_no="0"
shouldUploadPersistentDB="Y"

# [OPTIONAL] User configuration settings
# If you want to release Zilliqa, please keep this variable "true"
# If you do NOT want to release Zilliqa, please change this variable "false"
releaseZilliqa="true"

# [OPTIONAL] User configuration settings
# If you want to release Scilla, please define this variable
# If you do NOT want to release Scilla, please leave this variable empty
scillaPath=""

# Environment variables
releaseDir="release"
versionFile="VERSION"
dsNodeFile="dsnodes.xml"
scillaVersionPath="/src/lang/base/Syntax.ml"
scillaVersionKeyword="scilla_version"
scillaDebFolder="release_scilla"
zilliqaMajorLine=2
zilliqaMinorLine=4
zilliqaFixLine=6
zilliqaDSLine=8
scillaDSLine=10
scillaMajorLine=14
scillaMinorLine=16
scillaFixLine=18
zilliqaCommitLine=20
zilliqaShaLine=22
zilliqaSigLine=24
scillaCommitLine=26
scillaShaLine=28
scillaSigLine=30

# user-defined-helper-functions
function download_verify_s3db_zilliqa_only()
{
   echo "Ask all nodes to download and verify s3 database bucket : ${S3ReleaseTarBall} for zilliqa"
   public_keys=$(cat ${pubKeyFile}| tr '\n' ' ')
   run_cmd_for_all_in_parallel "./download_and_verify.sh -u zilliqa -k \"${public_keys}\" -z \"${zilliqaDebFile}\" -i \"${zilliqaSha}\" -q \"${zilliqaSignature}\" -d  \"${S3ReleaseTarBall}\""
}

function download_verify_replace_s3db_scilla_only()
{
   echo "Ask all nodes to download, verify and upgrade s3 database bucket : ${S3ReleaseTarBall} for scilla"
   public_keys=$(cat ${pubKeyFile}| tr '\n' ' ')
   run_cmd_for_all_in_parallel "./download_and_verify.sh -u scilla -k \"${public_keys}\" -s \"${scillaDebFile}\" -p \"${scillaSha}\" -r \"${scillaSignature}\" -d \"${S3ReleaseTarBall}\""
}

function download_verify_s3db_both()
{
   echo "Ask all nodes to download and verify s3 database bucket : ${S3ReleaseTarBall} for zilliqa and scilla"
   public_keys=$(cat ${pubKeyFile}| tr '\n' ' ')
   run_cmd_for_all_in_parallel "./download_and_verify.sh -u both -k \"${public_keys}\" -z \"${zilliqaDebFile}\" -i \"${zilliqaSha}\" -q \"${zilliqaSignature}\" -s \"${scillaDebFile}\" -p \"${scillaSha}\" -r \"${scillaSignature}\" -d \"${S3ReleaseTarBall}\""
}

# Set the context name explicitly
function setcontext()
{
    if [ -n "$current_cluster_name" ]
    then
        local_context=$(kubectl config current-context 2>/dev/null)
        # service-account-context means this script is running on a cluster node, so ignore the variable setting
        if [ "$local_context" = "service-account-context" ]
        then
            context_arg="--context $local_context"
        # "bastion-0" means this script is running from bastion pod
        elif [ "$HOSTNAME" == bastion-0 ]
        then
           # clear the context
           context_arg=""
        else
           # This script is running from user machine
           context_arg="--context $current_cluster_name"
       fi
    fi

    ulimit -n 65535
}

function run_cmd_for_all_in_parallel() {
    [ ! -x "$(command -v parallel)" ] && echo "command 'parallel' not found, please install it first" && return 1
    tmpfile=$(mktemp)

    if [ ! -z "$ALL_ARR_1" ];  then
    echo "${ALL_ARR_1}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_2" ];  then
    echo "${ALL_ARR_2}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_3" ];  then
    echo "${ALL_ARR_3}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_4" ];  then
    echo "${ALL_ARR_4}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_5" ];  then
    echo "${ALL_ARR_5}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_6" ];  then
    echo "${ALL_ARR_6}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_7" ];  then
    echo "${ALL_ARR_7}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_8" ];  then
    echo "${ALL_ARR_8}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_9" ];  then
    echo "${ALL_ARR_9}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_10" ];  then
    echo "${ALL_ARR_10}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_11" ];  then
    echo "${ALL_ARR_11}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$ALL_ARR_12" ];  then
    echo "${ALL_ARR_12}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    rm -f $tmpfile
}

# This right now also include non-ds guard. Ok for now.
function run_cmd_for_shards_in_parallel() {
    [ ! -x "$(command -v parallel)" ] && echo "command 'parallel' not found, please install it first" && return 1
    tmpfile=$(mktemp)

    if [ ! -z "$SHARD_ARR_1" ];  then
    echo "${SHARD_ARR_1}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_2" ];  then
    echo "${SHARD_ARR_2}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_3" ];  then
    echo "${SHARD_ARR_3}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_4" ];  then
    echo "${SHARD_ARR_4}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_5" ];  then
    echo "${SHARD_ARR_5}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_6" ];  then
    echo "${SHARD_ARR_6}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_7" ];  then
    echo "${SHARD_ARR_7}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_8" ];  then
    echo "${SHARD_ARR_8}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_9" ];  then
    echo "${SHARD_ARR_9}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_10" ];  then
    echo "${SHARD_ARR_10}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_11" ];  then
    echo "${SHARD_ARR_11}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    if [ ! -z "$SHARD_ARR_12" ];  then
    echo "${SHARD_ARR_12}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'" > $tmpfile
    fi

    rm -f $tmpfile
}

function run_cmd_for_dsguards_in_parallel() {
    [ ! -x "$(command -v parallel)" ] && echo "command 'parallel' not found, please install it first" && return 1
    echo "${DSGUARD_NODES}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'"
}

function run_cmd_for_lookups_in_parallel() {
    [ ! -x "$(command -v parallel)" ] && echo "command 'parallel' not found, please install it first" && return 1
    echo "${LOOKUP_NODES}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'"
}

function run_cmd_for_seeds_in_parallel() {
    [ ! -x "$(command -v parallel)" ] && echo "command 'parallel' not found, please install it first" && return 1
    echo "${SEED_NODES}" | \
        parallel --no-notice -j 50 --bar -k --tag --timeout 10 --retries 10 \
                "kubectl $context_arg exec {} -- bash -c '$1 || [ 1=1 ]'"
}

function run_cmd_for_lookup() {
    kubectl exec ${testnet}-lookup-${lookup_no} -- bash -c $1
}

function Add_suspend_to_allnodes() {
    echo "Adding SUSPEND_LAUNCH file to all nodes..."
    run_cmd_for_all_in_parallel "touch SUSPEND_LAUNCH"
}

function Remove_suspend_from_lookups() {
    echo "Removing SUSPEND_LAUNCH file from lookup nodes..."
    run_cmd_for_lookups_in_parallel "rm -f SUSPEND_LAUNCH"
}

function Remove_suspend_from_seeds() {
    echo "Removing SUSPEND_LAUNCH file from seed nodes..."
    run_cmd_for_seeds_in_parallel "rm -f SUSPEND_LAUNCH"
}

function Remove_suspend_from_dsguards() {
    echo "Removing SUSPEND_LAUNCH file from dsguard nodes..."
    run_cmd_for_dsguards_in_parallel "rm -f SUSPEND_LAUNCH"
}

function Remove_suspend_from_shards() {
    echo "Removing SUSPEND_LAUNCH file from shard nodes and non-dsguard nodes..."
    run_cmd_for_shards_in_parallel "rm -f SUSPEND_LAUNCH"
}

function kill_and_upgrade_dsguards()
{
    echo "Killing and upgrading all dsguard..."
    run_cmd_for_dsguards_in_parallel "${cmd_upgrade} && cp download/constants.xml constants.xml" 
}

function kill_and_upgrade_shards()
{
    echo "Killing and upgrading all shard nodes and ds-nonguard nodes..."
    run_cmd_for_shards_in_parallel "${cmd_upgrade} && cp download/constants.xml constants.xml"
}

function kill_and_upgrade_lookups()
{
    echo "Killing and upgrading all lookup nodes..."
    run_cmd_for_lookups_in_parallel "${cmd_upgrade} && cp download/constants.xml_lookup constants.xml"
}

function kill_and_upgrade_seeds()
{
    echo "Killing and upgrading all seed nodes..."
    run_cmd_for_seeds_in_parallel "${cmd_upgrade} && cp download/constants.xml_archivallookup constants.xml"
}

function download_s3db_on_allnodes()
{
   echo "Ask all nodes to download s3 database bucket : $S3PersistentDBFileName"
   run_cmd_for_all_in_parallel "[ ! -f download/fail ] && aws s3 cp s3://zilliqa-persistence/$S3PersistentDBFileName.tar.gz $S3PersistentDBFileName.tar.gz && rm -rf persistence && tar xzvf $S3PersistentDBFileName.tar.gz"
}

function upload_lookup_s3db()
{
   echo "Ask lookup nodes to upload s3 database bucket : $S3PersistentDBFileName"
   run_cmd_for_lookup "./backup.sh" "$lookup_no"
}

function upgrade()
{
    ## Create suspend flag for all pods
    Add_suspend_to_allnodes
    ## wait enough so that SUSPEND_LAUNCH file is created in all nodes.
    echo "waiting for 60 seconds"
    sleep 60

    ## kill all nodes in below sequence
    kill_and_upgrade_dsguards
    kill_and_upgrade_shards
    kill_and_upgrade_lookups
    kill_and_upgrade_seeds
    ## wait enough so that all nodes are killed and upgraded.      
    echo "waiting for 180 seconds"
    sleep 180

    
	## Download S3 Database on all nodes replacing their local persistence storage
    [ ! -z "$S3PersistentDBFileName" ] && download_s3db_on_allnodes && echo "waiting for 120 seconds" && sleep 120	

    ## Remove the suspend flag
    Remove_suspend_from_lookups
    Remove_suspend_from_seeds
    Remove_suspend_from_shards
    Remove_suspend_from_dsguards
    ## wait enough so that SUSPEND_LAUNCH file is removed from all nodes.
    echo "waiting for 60 seconds"
    sleep 60
}

# Validate input argument
if [ "$#" -ne 0 ]; then
    echo -e "\n\032[0;32mUsage: ./scripts/release.sh\033[0m\n"
    exit 0
fi

if [ "$GitHubToken" = "" ] || [ "$packageName" = "" ] || [ "$releaseTitle" = "" ] || [ "$releaseDescription" = "" ] || [ "$privKeyFile" = "" ] || [ "$pubKeyFile" = "" ] || [ "$constantFile" = "" ] || [ "$constantLookupFile" = "" ] || [ "$constantLevel2LookupFile" = "" ]; then
    echo -e "\n\033[0;31m*ERROR* Please input ALL [MUST BE FILLED IN] fields in release.sh!\033[0m\n"
    exit 0
fi

if [ ! -f "${privKeyFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Private key file : ${privKeyFile} not found, please confirm privKeyFile field in release.sh!\033[0m\n"
    exit 0
fi

if [ ! -f "${pubKeyFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Public key file : ${pubKeyFile} not found, please confirm pubKeyFile field in release.sh!\033[0m\n"
    exit 0
fi

if [ ! -f "${constantFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Constant file : ${constantFile} not found, please confirm constantFile field in release.sh!\033[0m\n"
    exit 0
fi

if [ ! -f "${constantLookupFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Lookup constant file : ${constantLookupFile} not found, please confirm constantLookupFile field in release.sh!\033[0m\n"
    exit 0
fi

if [ ! -z "$constantLevel2LookupFile" ] && [ ! -f "${constantLevel2LookupFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Archival lookup constant file : ${constantLevel2LookupFile} not found, please confirm constantLevel2LookupFile field in release.sh!\033[0m\n"
    exit 0
fi

if [ ! -z "$constantNewLookupFile" ] && [ ! -f "${constantNewLookupFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Archival lookup constant file : ${constantNewLookupFile} not found, please confirm constantNewLookupFile field in release.sh!\033[0m\n"
    exit 0
fi

if [ "$releaseZilliqa" = "true" ]; then
    echo -e "\n\033[0;32m*INFO* Zilliqa will be released.\033[0m\n"
else
    echo -e "\n\033[0;32m*INFO* Zilliqa will NOT be released.\033[0m\n"
fi

if [ -d "${scillaPath}" ]; then
    echo -e "\n\033[0;32m*INFO* Scilla will be released.\033[0m\n"
    scillaPath="$(realpath ${scillaPath})"
else
    echo -e "\n\033[0;32m*INFO* Scilla Path : ${scillaPath} not existed, Scilla will NOT be released.\033[0m\n"
    scillaPath=""
fi

if [ "$releaseZilliqa" = "false" ] && [ ! -d "${scillaPath}" ]; then
    echo -e "\n\033[0;31m*ERROR* Nothing will be released!\033[0m\n"
    exit 0
fi

# Read information from files
constantFile="$(realpath ${constantFile})"
constantLookupFile="$(realpath ${constantLookupFile})"
[ ! -z "$constantLevel2LookupFile" ] && constantLevel2LookupFile="$(realpath ${constantLevel2LookupFile})"
[ ! -z "$constantNewLookupFile" ] && constantNewLookupFile="$(realpath ${constantNewLookupFile})"
versionFile="$(realpath ${versionFile})"
accountName="$(grep -oPm1 "(?<=<UPGRADE_HOST_ACCOUNT>)[^<]+" ${constantFile})"
repoName="$(grep -oPm1 "(?<=<UPGRADE_HOST_REPO>)[^<]+" ${constantFile})"
scillaMultiVersion="$(grep -oPm1 "(?<=<ENABLE_SCILLA_MULTI_VERSION>)[^<]+" ${constantFile})"
zilliqaMajor="$(sed -n ${zilliqaMajorLine}p ${versionFile})"
zilliqaMinor="$(sed -n ${zilliqaMinorLine}p ${versionFile})"
zilliqaFix="$(sed -n ${zilliqaFixLine}p ${versionFile})"
zilliqaDS="$(sed -n ${zilliqaDSLine}p ${versionFile})"
zilliqaCommit="$(git describe --always)"
newVer=${zilliqaMajor}.${zilliqaMinor}.${zilliqaFix}.${zilliqaDS}.${zilliqaCommit}
export ZIL_VER=${newVer}
export ZIL_PACK_NAME=${packageName}

# Use cpack to making deb file
if [ "$releaseZilliqa" = "true" ]; then
    echo -e "\n\033[0;32mMaking Zilliqa deb package...\033[0m\n"
fi

rm -rf ${releaseDir}
cmake -H. -B${releaseDir} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local/
cmake --build ${releaseDir}
cd ${releaseDir}; cp ${versionFile} .

if [ "$releaseZilliqa" = "true" ]; then
    make package
    mv ${packageName}-${zilliqaMajor}.${zilliqaMinor}.${zilliqaFix}.${zilliqaDS}.${zilliqaCommit}-$(uname).deb ${packageName}-${zilliqaMajor}.${zilliqaMinor}.${zilliqaFix}.${zilliqaDS}.${zilliqaCommit}-$(uname)-Zilliqa.deb
    zilliqaDebFile="$(ls *.deb)"
    echo -e "\n\033[0;32mZilliqa deb packages are generated successfully.\033[0m\n"
fi

cd -

# Write Zilliqa new version information into version file and make SHA-256 & multi-signature
privKeyFile="$(realpath ${privKeyFile})"
pubKeyFile="$(realpath ${pubKeyFile})"
cd ${releaseDir}

if [ "$releaseZilliqa" = "true" ]; then
    sed -i "${zilliqaCommitLine}s/.*/${zilliqaCommit}/" $(basename ${versionFile})
    echo -e "\n\033[0;32mMaking Zilliqa SHA-256 & multi-signature...\033[0m\n"
    zilliqaSha="$(sha256sum ${zilliqaDebFile}|cut -d ' ' -f1|tr 'a-z' 'A-Z')"
    sed -i "${zilliqaShaLine}s/.*/${zilliqaSha}/" $(basename ${versionFile})
    zilliqaSignature="$(./bin/signmultisig -m ${zilliqaSha} -i ${privKeyFile} -u ${pubKeyFile})"
    sed -i "${zilliqaSigLine}s/.*/${zilliqaSignature}/" $(basename ${versionFile})
    echo -e "\n\033[0;32mZilliqa SHA-256 & multi-signature are written into $(basename ${versionFile}) successfully.\033[0m\n"
fi
if [ "$scillaPath" != "" ]; then
    scillaVersionFullPath=${scillaPath}${scillaVersionPath}

    # Read version information from lang/base/Syntax.ml, then write into VERSION
    if [ -f "${scillaVersionFullPath}" ]; then
        scillaMajor="$(grep -r ${scillaVersionKeyword} ${scillaVersionFullPath}|cut -d ',' -f1|cut -d '(' -f2)"
        scillaMinor="$(grep -r ${scillaVersionKeyword} ${scillaVersionFullPath}|cut -d ',' -f2)"
        scillaFix="$(grep -r ${scillaVersionKeyword} ${scillaVersionFullPath}|cut -d ',' -f3|cut -d ')' -f1)"
        scillaDS="$(sed -n ${scillaDSLine}p $(basename ${versionFile}))"
        scillaMajor="$(echo $scillaMajor | sed -e 's/^[ \t]*//')"
        scillaMinor="$(echo $scillaMinor | sed -e 's/^[ \t]*//')"
        scillaFix="$(echo $scillaFix | sed -e 's/^[ \t]*//')"
        sed -i "${scillaMajorLine}s/.*/${scillaMajor}/" $(basename ${versionFile})
        sed -i "${scillaMinorLine}s/.*/${scillaMinor}/" $(basename ${versionFile})
        sed -i "${scillaFixLine}s/.*/${scillaFix}/" $(basename ${versionFile})
    fi
fi

cd -

# Make scilla image, and pack to deb file
if [ "$scillaPath" != "" ]; then
    cd ${scillaPath}
    scillaCommit="$(git describe --always)"
    make
    cd -
    rm -rf ${scillaDebFolder}/scilla/*
    if [ "$scillaMultiVersion" = "true" ]; then
        mkdir ${scillaDebFolder}/scilla/${scillaMajor}
        cp -rf ${scillaPath}/* ${scillaDebFolder}/scilla/${scillaMajor}/
    else
        mkdir ${scillaDebFolder}/scilla
        cp -rf ${scillaPath}/* ${scillaDebFolder}/scilla/
    fi
    sed -i "/Version: /c\Version: ${scillaMajor}.${scillaMinor}.${scillaFix}" ${scillaDebFolder}/DEBIAN/control
    echo -e "\n\033[0;32mMaking Scilla deb package...\033[0m\n"
    scillaDebFile=${packageName}-${scillaMajor}.${scillaMinor}.${scillaFix}.${scillaDS}.${scillaCommit}-$(uname)-Scilla.deb
    if [ -f "${scillaDebFile}" ]; then
        rm ${scillaDebFile}
    fi
    dpkg-deb --build ${scillaDebFolder}
    mv ${scillaDebFolder}.deb ${scillaDebFile}
    scillaSha="$(sha256sum ${scillaDebFile}|cut -d ' ' -f1|tr 'a-z' 'A-Z')"
    echo -e "\n\033[0;32mScilla deb packages are generated successfully.\033[0m\n"
    echo -e "\n\033[0;32mMaking Scilla SHA-256 & multi-signature...\033[0m\n"
    cd ${releaseDir}
    sed -i "${scillaCommitLine}s/.*/${scillaCommit}/" $(basename ${versionFile})
    sed -i "${scillaShaLine}s/.*/${scillaSha}/" $(basename ${versionFile})
    scillaSignature="$(./bin/signmultisig -m ${scillaSha} -i ${privKeyFile} -u ${pubKeyFile})"
    sed -i "${scillaSigLine}s/.*/${scillaSignature}/" $(basename ${versionFile})
    cd -
    echo -e "\n\033[0;32mScilla SHA-256 & multi-signature are written into $(basename ${versionFile}) successfully.\033[0m\n"
fi

# Update the xml
cd ${releaseDir}
cp ../constants_local.xml ./constants.xml
cd -

if [ -z "$useNewUpgradeMethod" ]; then # Upload package onto GitHub
	echo -e "\n\033[0;32mCreating new release and uploading package onto GitHub...\033[0m\n"
	fullCommit="$(git rev-parse HEAD)"
	releaseLog="release.log"
	curl -v -s \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/json" "https://api.github.com/repos/${accountName}/${repoName}/releases" \
	  -d '{
	  "tag_name": "'"${newVer}"'", 
	  "target_commitish": "'"${fullCommit}"'",
	  "name": "'"${releaseTitle}"'",
	  "body": "'"${releaseDescription}"'",
	  "draft": true,
	  "prerelease": false
	}' > ${releaseLog}

	line="$(sed '6!d' ${releaseLog})"
	releaseId=${line:8:8}
	check='^[0-9]+$'
	if ! [[ $releaseId =~ $check ]] ; then
		echo -e "\n\032[0;32m*ERROR* Create new release fail! Please check input value and ${releaseLog}, then try again.\033[0m\n"
		exit 0
	fi
	curl -v -s \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/octet-stream" \
	  --data-binary @${pubKeyFile} \
	  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=$(basename ${pubKeyFile})"
	if [ "$releaseZilliqa" = "true" ]; then
		curl -v -s \
		  -H "Authorization: token ${GitHubToken}" \
		  -H "Content-Type:application/vnd.debian.binary-package" \
		  --data-binary @${releaseDir}/${zilliqaDebFile} \
		  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${zilliqaDebFile}"
	fi
	curl -v -s \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/octet-stream" \
	  --data-binary @${releaseDir}/$(basename ${versionFile}) \
	  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=$(basename ${versionFile})"
	curl -v -s  \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/octet-stream"  \
	  --data-binary @"${constantFile}" \
	  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${constantFile##*/}"
	curl -v -s  \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/octet-stream"  \
	  --data-binary @"${constantLookupFile}" \
	  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${constantLookupFile##*/}_lookup"
	[ ! -z "$constantLevel2LookupFile" ] && curl -v -s  \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/octet-stream"  \
	  --data-binary @"${constantLevel2LookupFile}" \
	  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${constantLevel2LookupFile##*/}_level2lookup"
	[ ! -z "$constantNewLookupFile" ] && curl -v -s  \
	  -H "Authorization: token ${GitHubToken}" \
	  -H "Content-Type:application/octet-stream"  \
	  --data-binary @"${constantNewLookupFile}" \
	  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${constantNewLookupFile##*/}_newlookup"
	if [ "$scillaPath" != "" ]; then
		curl -v -s \
		  -H "Authorization: token ${GitHubToken}" \
		  -H "Content-Type:application/vnd.debian.binary-package" \
		  --data-binary @${scillaDebFile} \
		  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${scillaDebFile}"
	fi
	rm ${releaseLog}
	echo -e "\n\033[0;32mA new draft release with package is created on Github successfully, please proceed to publishing the draft release on Github web page.\033[0m\n"
else
    ##################################################################################################  
    # Use the new upgrade mechanism :
    # Steps:
    #       1. Create the tar.gz file with $S3ReleaseTarBall
    #          tar.gz file contains zilliqa and/or scilla package.
    #       2. Upload it to S3.   
    #       3. Download latest tar.gz on all nodes from s3 in download/$S3ReleaseTarBall.tar.gz extract and verify packages.
    #       4. If only scilla, extract scilla deb 
    #       5. If only zilliqa or both zilliqa & scilla, 
    #           5.a Create SUSPEND_LAUNCH in all nodes.
    #           5.b Kill zilliqa process in all nodes in sequence : SHARD->DS->LOOKUP->SEED
    #           5.c Copy over constants files from /run/zilliqa/download to /run/zilliqa/ and
    #               extract zilliqa deb
    #               If also scilla release, extract scilla deb 
    #           5.d Remove SUSPEND_LAUNCH in sequence LOOKUP->SEED->SHARD->DS
    ################################################################################################## 

    setcontext

    #### Step 1 ####
    if [ "$releaseZilliqa" = "true" ]; then
        Zilliqa_Deb="$(realpath ${releaseDir}/${zilliqaDebFile})"
    fi
    if [ "$scillaPath" != "" ]; then
	Scilla_Deb="${scillaDebFile}"
    fi
	
    ## zip the release
    cp ${constantLookupFile} ${constantLookupFile}_lookup
    [ ! -z "$constantLevel2LookupFile" ] && cp ${constantLevel2LookupFile} ${constantLevel2LookupFile}_level2lookup
    [ ! -z "$constantNewLookupFile" ] && cp ${constantNewLookupFile} ${constantNewLookupFile}_newlookup
    cmd="tar cfz ${S3ReleaseTarBall}.tar.gz -C $(dirname ${pubKeyFile}) $(basename ${pubKeyFile}) -C $(realpath ./${releaseDir}) $(basename ${versionFile}) -C $(dirname ${constantFile}) $(basename ${constantFile}) -C $(dirname ${constantLookupFile}) $(basename ${constantLookupFile})_lookup"
    [ "$releaseZilliqa" = "true" ] && cmd="${cmd} -C $(dirname ${Zilliqa_Deb}) ${zilliqaDebFile}"
    [ ! -z "${constantLevel2LookupFile}" ] && cmd="${cmd} -C $(dirname ${constantLevel2LookupFile}) $(basename ${constantLevel2LookupFile})_level2lookup"
    [ ! -z "${constantNewLookupFile}" ] && cmd="${cmd} -C $(dirname ${constantNewLookupFile}) $(basename ${constantNewLookupFile})_newlookup"
    [ ! -z "${scillaPath}" ] && cmd="${cmd} -C $(realpath ./) ${Scilla_Deb}"

    $cmd

    #### Step 2 ####
    # upload to S3 the ${S3ReleaseTarBall}.tar.gz
    cat << EOF > UploadToS3Script.py
#!/usr/bin/env python
import boto3
from boto3.s3.transfer import S3Transfer
import sys

BUCKET_NAME = 'zilliqa-release-data'

transfer = boto3.client('s3')
key = sys.argv[1]
print(key)
transfer.upload_file(key, BUCKET_NAME, key,ExtraArgs={'ACL':'public-read'})
print("Uploaded")

EOF

    chmod 755 UploadToS3Script.py
    python ./UploadToS3Script.py "${S3ReleaseTarBall}.tar.gz"

    [ "$applyUpgrade" == "N" ] && exit

    #### Ask for confirmation from user if want to continue?? can check s3 if package is uploaded and is correct? #####
    read -p "Make sure release was uploaded to S3. Shall we continue [Yy]: " -n 1 -r
    if [[ ! $REPLY =~ ^[Yy]$ ]]
    then
 	exit
    fi
    echo ""

    ## Ask one of lookup to upload persistence data to S3
    [ "$shouldUploadPersistentDB" == "Y" ] && [ ! -z "$S3PersistentDBFileName" ] && upload_lookup_s3db && echo "waiting for 60 seconds" && sleep 60

    ## Gathering all pod names
    ALL_NODES=$(kubectl $context_arg get pods \
        -l 'type in (lookup, normal, newlookup, dsguard)',testnet=$testnet,app=zilliqa \
        --sort-by='.metadata.name' \
        -o custom-columns='Name:.metadata.name' --no-headers)

    ## Gathering dsguard pod names
    DSGUARD_NODES=$(kubectl $context_arg get pods \
        -l type=dsguard,testnet=$testnet,app=zilliqa \
        --sort-by='.metadata.name' \
        -o custom-columns='Name:.metadata.name' --no-headers)

    ## Gathering shard pod names
    SHARD_NODES=$(kubectl $context_arg get pods \
        -l type=normal,testnet=$testnet,app=zilliqa \
        --sort-by='.metadata.name' \
        -o custom-columns='Name:.metadata.name' --no-headers)

    ## Gathering lookup pod names
    LOOKUP_NODES=$(kubectl $context_arg get pods \
        -l type=lookup,testnet=$testnet,app=zilliqa \
        --sort-by='.metadata.name' \
        -o custom-columns='Name:.metadata.name' --no-headers)

    ## Gathering seed pod names
    SEED_NODES=$(kubectl $context_arg get pods \
        -l type=newlookup,testnet=$testnet,app=zilliqa \
        --sort-by='.metadata.name' \
        -o custom-columns='Name:.metadata.name' --no-headers)

    ALL_ARR_1=$(echo "${ALL_NODES}" | cut -d $'\n' -f1-250)
    ALL_ARR_2=$(echo "${ALL_NODES}" | cut -d $'\n' -f251-500)
    ALL_ARR_3=$(echo "${ALL_NODES}" | cut -d $'\n' -f501-750)
    ALL_ARR_4=$(echo "${ALL_NODES}" | cut -d $'\n' -f751-1000)
    ALL_ARR_5=$(echo "${ALL_NODES}" | cut -d $'\n' -f1001-1250)
    ALL_ARR_6=$(echo "${ALL_NODES}" | cut -d $'\n' -f1251-1500)
    ALL_ARR_7=$(echo "${ALL_NODES}" | cut -d $'\n' -f1501-1750)
    ALL_ARR_8=$(echo "${ALL_NODES}" | cut -d $'\n' -f1751-2000)
    ALL_ARR_9=$(echo "${ALL_NODES}" | cut -d $'\n' -f2001-2250)
    ALL_ARR_10=$(echo "${ALL_NODES}" | cut -d $'\n' -f2251-2500)
    ALL_ARR_11=$(echo "${ALL_NODES}" | cut -d $'\n' -f2501-2750)
    ALL_ARR_12=$(echo "${ALL_NODES}" | cut -d $'\n' -f2751-3000)

    SHARD_ARR_1=$(echo "${SHARD_NODES}" | cut -d $'\n' -f1-250)
    SHARD_ARR_2=$(echo "${SHARD_NODES}" | cut -d $'\n' -f251-500)
    SHARD_ARR_3=$(echo "${SHARD_NODES}" | cut -d $'\n' -f501-750)
    SHARD_ARR_4=$(echo "${SHARD_NODES}" | cut -d $'\n' -f751-1000)
    SHARD_ARR_5=$(echo "${SHARD_NODES}" | cut -d $'\n' -f1001-1250)
    SHARD_ARR_6=$(echo "${SHARD_NODES}" | cut -d $'\n' -f1251-1500)
    SHARD_ARR_7=$(echo "${SHARD_NODES}" | cut -d $'\n' -f1501-1750)
    SHARD_ARR_8=$(echo "${SHARD_NODES}" | cut -d $'\n' -f1751-2000)
    SHARD_ARR_9=$(echo "${SHARD_NODES}" | cut -d $'\n' -f2001-2250)
    SHARD_ARR_10=$(echo "${SHARD_NODES}" | cut -d $'\n' -f2251-2500)
    SHARD_ARR_11=$(echo "${SHARD_NODES}" | cut -d $'\n' -f2501-2750)
    SHARD_ARR_12=$(echo "${SHARD_NODES}" | cut -d $'\n' -f2751-3000)

    #### Step 3 ####

    # download and verify from S3
    if [ "$releaseZilliqa" = "true" ] && [ "$scillaPath" != "" ]; then
		download_verify_s3db_both
    elif [ "$releaseZilliqa" = "true" ]; then
		download_verify_s3db_zilliqa_only
    elif [ "$scillaPath" != "" ]; then
		# Step 4 included. 
		download_verify_replace_s3db_scilla_only
		sleep 60
        ## No need to kill and upgrade zilliqa  
        exit
	fi

    #### Step 5 ####

    cmd_upgrade_zilliqa_only="[ ! -f download/fail ] && pkill zilliqa && dpkg -i download/${zilliqaDebFile} > /dev/null 2>&1"
    cmd_upgrade_zilliqa_scilla="[ ! -f download/fail ] && pkill zilliqa && dpkg -i download/${zilliqaDebFile} && dpkg -i download/${scillaDebFile} > /dev/null 2>&1"
    cmd_upgrade_scilla_only="[ ! -f download/fail ] && dpkg -i download/${scillaDebFile} > /dev/null 2>&1"

    if [ "$releaseZilliqa" = "true" ] && [ "$scillaPath" != "" ]; then
        cmd_upgrade="${cmd_upgrade_zilliqa_scilla}"
    elif [ "$releaseZilliqa" = "true" ]; then
        cmd_upgrade="${cmd_upgrade_zilliqa_only}"
    elif [ "$scillaPath" != "" ]; then
        cmd_upgrade="${cmd_upgrade_scilla_only}"
    fi
    upgrade
    echo "Done!"
fi







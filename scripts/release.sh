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
testnet_to_be_upgraded=""
cluster_name="" # eg: xjfqp.dev.z7a.xyz.k8s.local
release_bucket_name="33cb0344-f847-4fe9-a982-4b409403bdf3"

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
constantsDir="constantsDir"
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

# Set the context name explicitly
function setcontext()
{
    if [ -n "$cluster_name" ]
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
           context_arg="--context $cluster_name"
       fi
    fi

    ulimit -n 65535
}

# Validate input argument
if [ "$#" -ne 0 ]; then
    echo -e "\n\032[0;32mUsage: ./scripts/release.sh\033[0m\n"
    exit 0
fi

if [ "$privKeyFile" = "" ] || [ "$pubKeyFile" = "" ] || [ "$testnet_to_be_upgraded" = "" ] || [ "$cluster_name" = "" ]; then
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
rm -rf ${constantsDir}; mkdir ${constantsDir}; cd ${constantsDir}; mkdir l; mkdir l2; mkdir n; cd -;
kubectl cp ${testnet_to_be_upgraded}-dsguard-0:/run/zilliqa/constants.xml ${constantsDir}/constants.xml
kubectl cp ${testnet_to_be_upgraded}-lookup-0:/run/zilliqa/constants.xml ${constantsDir}/l/constants.xml
kubectl cp ${testnet_to_be_upgraded}-level2lookup-0:/run/zilliqa/constants.xml ${constantsDir}/l2/constants.xml
kubectl cp ${testnet_to_be_upgraded}-newlookup-0:/run/zilliqa/constants.xml ${constantsDir}/n/constants.xml
constantFile="$(realpath ${constantsDir}/constants.xml)"
constantLookupFile="$(realpath ${constantsDir}/l/constants.xml)"
constantLevel2LookupFile="$(realpath ${constantsDir}/l2/constants.xml)"
constantNewLookupFile="$(realpath ${constantsDir}/n/constants.xml)"
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
export ZIL_PACK_NAME=${testnet_to_be_upgraded}

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
    mv ${testnet_to_be_upgraded}-${zilliqaMajor}.${zilliqaMinor}.${zilliqaFix}.${zilliqaDS}.${zilliqaCommit}-$(uname).deb ${testnet_to_be_upgraded}-${zilliqaMajor}.${zilliqaMinor}.${zilliqaFix}.${zilliqaDS}.${zilliqaCommit}-$(uname)-Zilliqa.deb
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
    scillaDebFile=${testnet_to_be_upgraded}-${scillaMajor}.${scillaMinor}.${scillaFix}.${scillaDS}.${scillaCommit}-$(uname)-Scilla.deb
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

    ##################################################################################################
    # Use the new upgrade mechanism :
    # Steps:
    #       1. Create the tar.gz file with $testnet_to_be_upgraded
    #          tar.gz file contains zilliqa and/or scilla package.
    #       2. Upload it to S3.   
    #       3. Download latest tar.gz on all nodes from s3 in download/$testnet_to_be_upgraded.tar.gz extract and verify packages.
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
    cp ${constantLevel2LookupFile} ${constantLevel2LookupFile}_level2lookup
    [ ! -z "$constantNewLookupFile" ] && cp ${constantNewLookupFile} ${constantNewLookupFile}_newlookup
    cmd="tar cfz ${testnet_to_be_upgraded}.tar.gz -C $(dirname ${pubKeyFile}) $(basename ${pubKeyFile}) -C $(realpath ./scripts) miner_info.py -C $(realpath ./scripts) auto_backup.py -C $(realpath ./scripts) download_incr_DB.py -C $(realpath ./scripts) download_and_verify.sh -C $(realpath ./scripts) upload_incr_DB.py -C $(realpath ./scripts) auto_Backup.py -C $(realpath ./${releaseDir}) $(basename ${versionFile}) -C $(dirname ${constantFile}) $(basename ${constantFile}) -C $(dirname ${constantLookupFile}) $(basename ${constantLookupFile})_lookup"
    if [ "$releaseZilliqa" = "true" ]; then
        cmd="${cmd} -C $(dirname ${Zilliqa_Deb}) ${zilliqaDebFile}"
        cmd="${cmd} -C $(dirname ${constantLevel2LookupFile}) $(basename ${constantLevel2LookupFile})_level2lookup"
        [ ! -z "${constantNewLookupFile}" ] && cmd="${cmd} -C $(dirname ${constantNewLookupFile}) $(basename ${constantNewLookupFile})_newlookup"
    fi
    [ ! -z "${scillaPath}" ] && cmd="${cmd} -C $(realpath ./) ${Scilla_Deb}"

    $cmd
    cd -

    #### Step 2 ####
    # upload to S3 the ${testnet_to_be_upgraded}.tar.gz
    aws s3 cp ${testnet_to_be_upgraded}.tar.gz s3://${release_bucket_name}/release/${testnet_to_be_upgraded}.tar.gz
    echo "Done!"

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
GitHubToken=""
packageName=""
releaseTitle=""
releaseDescription=""
privKeyFile=""
pubKeyFile=""
constantFile=""
constantLookupFile=""
constantArchivalFile=""

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

# Validate input argument
if [ "$#" -ne 0 ]; then
    echo -e "\n\032[0;32mUsage: source scripts/release.sh\033[0m\n"
    return 1
fi

if [ "$GitHubToken" = "" ] || [ "$packageName" = "" ] || [ "$releaseTitle" = "" ] || [ "$releaseDescription" = "" ] || [ "$privKeyFile" = "" ] || [ "$pubKeyFile" = "" ] || [ "$constantFile" = "" ] || [ "$constantLookupFile" = "" ] || [ "$constantArchivalFile" = "" ]; then
    echo -e "\n\033[0;31m*ERROR* Please input ALL [MUST BE FILLED IN] fields in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${privKeyFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Private key file : ${privKeyFile} not found, please confirm privKeyFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${pubKeyFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Public key file : ${pubKeyFile} not found, please confirm pubKeyFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${constantFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Constant file : ${constantFile} not found, please confirm constantFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${constantLookupFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Lookup constant file : ${constantLookupFile} not found, please confirm constantLookupFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${constantArchivalFile}" ]; then
    echo -e "\n\033[0;31m*ERROR* Archival constant file : ${constantArchivalFile} not found, please confirm constantArchivalFile field in release.sh!\033[0m\n"
    return 1
fi

if [ -d "${scillaPath}" ]; then
    echo -e "\n\033[0;32m*INFO* Scilla will be released.\033[0m\n"
    scillaPath="$(realpath ${scillaPath})"
else
    echo -e "\n\033[0;32m*INFO* Scilla Path : ${scillaPath} not existed, Scilla will NOT be released.\033[0m\n"
    scillaPath=""
fi

# Read information from files
constantFile="$(realpath ${constantFile})"
constantLookupFile="$(realpath ${constantLookupFile})"
constantArchivalFile="$(realpath ${constantArchivalFile})"
versionFile="$(realpath ${versionFile})"
accountName="$(grep -oPm1 "(?<=<UPGRADE_HOST_ACCOUNT>)[^<]+" ${constantFile})"
repoName="$(grep -oPm1 "(?<=<UPGRADE_HOST_REPO>)[^<]+" ${constantFile})"
zilliqaMajor="$(sed -n ${zilliqaMajorLine}p ${versionFile})"
zilliqaMinor="$(sed -n ${zilliqaMinorLine}p ${versionFile})"
zilliqaFix="$(sed -n ${zilliqaFixLine}p ${versionFile})"
zilliqaDS="$(sed -n ${zilliqaDSLine}p ${versionFile})"
zilliqaCommit="$(git describe --always)"
newVer=${zilliqaMajor}.${zilliqaMinor}.${zilliqaFix}.${zilliqaDS}.${zilliqaCommit}
export ZIL_VER=${newVer}
export ZIL_PACK_NAME=${packageName}

# Use cpack to making deb file
echo -e "\n\033[0;32mMake Zilliqa deb package...\033[0m\n"
rm -rf ${releaseDir}
cmake -H. -B${releaseDir} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local/
cmake --build ${releaseDir} --j4
cd ${releaseDir}; make package; cp ${versionFile} .; zilliqaDebFile="$(ls *.deb)"; cd -
echo -e "\n\033[0;32mZilliqa deb packages are generated successfully.\033[0m\n"

# Write new version information into version file and make SHA-256 & multi-signature
privKeyFile="$(realpath ${privKeyFile})"
pubKeyFile="$(realpath ${pubKeyFile})"
cd ${releaseDir}
sed -i "${zilliqaCommitLine}s/.*/${zilliqaCommit}/" $(basename ${versionFile})
echo -e "\n\033[0;32mMaking Zilliqa SHA-256 & multi-signature...\033[0m\n"
zilliqaSha="$(sha256sum ${zilliqaDebFile}|cut -d ' ' -f1|tr 'a-z' 'A-Z')"
sed -i "${zilliqaShaLine}s/.*/${zilliqaSha}/" $(basename ${versionFile})
zilliqaSignature="$(./bin/signmultisig ${zilliqaSha} ${privKeyFile} ${pubKeyFile})"
sed -i "${zilliqaSigLine}s/.*/${zilliqaSignature}/" $(basename ${versionFile})

if [ "$scillaPath" != "" ]; then
    scillaVersionFullPath=${scillaPath}${scillaVersionPath}

    # Read version information from lang/base/Syntax.ml, then write into VERSION
    if [ -f "${scillaVersionFullPath}" ]; then
        scillaMajor="$(grep -r ${scillaVersionKeyword} ${scillaVersionFullPath}|cut -d ',' -f1|cut -d '(' -f2)"
        scillaMinor="$(grep -r ${scillaVersionKeyword} ${scillaVersionFullPath}|cut -d ',' -f2)"
        scillaFix="$(grep -r ${scillaVersionKeyword} ${scillaVersionFullPath}|cut -d ',' -f3|cut -d ')' -f1)"
        scillaDS="$(sed -n ${scillaDSLine}p $(basename ${versionFile}))"
        scillaMajor="${scillaMajor##*( )}"
        scillaMinor="${scillaMinor##*( )}"
        scillaFix="${scillaFix##*( )}"
        sed -i "${scillaMajorLine}s/.*/${scillaMajor}/" $(basename ${versionFile})
        sed -i "${scillaMinorLine}s/.*/${scillaMinor}/" $(basename ${versionFile})
        sed -i "${scillaFixLine}s/.*/${scillaFix}/" $(basename ${versionFile})
    fi
fi

cd -
echo -e "\n\033[0;32mZilliqa SHA-256 & multi-signature are written into $(basename ${versionFile}) successfully.\033[0m\n"

# Make scilla image, and pack to deb file
if [ "$scillaPath" != "" ]; then
    cd ${scillaPath}
    scillaCommit="$(git describe --always)"
    make
    cd -
    rm -rf ${scillaDebFolder}/scilla/*
    mkdir ${scillaDebFolder}/scilla/${scillaMajor}
    cp -rf ${scillaPath}/* ${scillaDebFolder}/scilla/${scillaMajor}/
    sed -i "/Version: /c\Version: ${scillaMajor}.${scillaMinor}.${scillaFix}" ${scillaDebFolder}/DEBIAN/control
    echo -e "\n\033[0;32mMake Scilla deb package...\033[0m\n"
    scillaDebFile=scilla-${packageName}-${scillaMajor}.${scillaMinor}.${scillaFix}.${scillaDS}.${scillaCommit}-Linux.deb
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
    scillaSignature="$(./bin/signmultisig ${scillaSha} ${privKeyFile} ${pubKeyFile})"
    sed -i "${scillaSigLine}s/.*/${scillaSignature}/" $(basename ${versionFile})
    cd -
    echo -e "\n\033[0;32mScilla SHA-256 & multi-signature are written into $(basename ${versionFile}) successfully.\033[0m\n"
fi

# Update the xml
cd ${releaseDir}
cp ../constants_local.xml ./constants.xml
ret="$(./bin/gensigninitialds   ${privKeyFile} ${pubKeyFile})"
cd -

# Upload package onto GitHub
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
    return 1
fi
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/octet-stream" \
  --data-binary @${pubKeyFile} \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=$(basename ${pubKeyFile})"
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/vnd.debian.binary-package" \
  --data-binary @${releaseDir}/${zilliqaDebFile} \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${zilliqaDebFile}"
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
curl -v -s  \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/octet-stream"  \
  --data-binary @"${constantArchivalFile}" \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${constantArchivalFile##*/}_archival"
if [ "$scillaPath" != "" ]; then
    curl -v -s \
      -H "Authorization: token ${GitHubToken}" \
      -H "Content-Type:application/vnd.debian.binary-package" \
      --data-binary @${scillaDebFile} \
      "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${scillaDebFile}"
fi
rm ${releaseLog}
echo -e "\n\033[0;32mA new draft release with package is created on Github successfully, please proceed to publishing the draft release on Github web page.\033[0m\n"

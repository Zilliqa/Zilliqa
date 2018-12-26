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
scillaPath=""    # Scilla will NOT be released if leaving this field empty

# Environment variables
releaseDir="release"
versionFile="../VERSION"
dsNodeFile="dsnodes.xml"
majorLine=2
minorLine=4
fixLine=6
DSLine=8
commitLine=12
shaLine=14
sigLine=16

# Validate input argument
if [ "$#" -ne 0 ]; then
    echo "Usage: source ./scripts/release.sh"
    return 1
fi

if [ "$GitHubToken" = "" ] || [ "$packageName" = "" ] || [ "$releaseTitle" = "" ] || [ "$releaseDescription" = "" ] || [ "$privKeyFile" = "" ] || [ "$pubKeyFile" = "" ] || [ "$constantFile" = "" ] || [ "$constantLookupFile" = "" ] || [ "$constantArchivalFile" = "" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Please input ALL [MUST BE FILLED IN] fields in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${privKeyFile}" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Private key file : ${privKeyFile} not found, please confirm privKeyFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${pubKeyFile}" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Public key file : ${pubKeyFile} not found, please confirm pubKeyFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${constantFile}" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Constant file : ${constantFile} not found, please confirm constantFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${constantLookupFile}" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Lookup constant file : ${constantLookupFile} not found, please confirm constantLookupFile field in release.sh!\033[0m\n"
    return 1
fi

if [ ! -f "${constantArchivalFile}" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Archival constant file : ${constantArchivalFile} not found, please confirm constantArchivalFile field in release.sh!\033[0m\n"
    return 1
fi

if [ -d "${scillaPath}" ]; then
    echo -e "\n\n\033[0;32m*INFO* Scilla will be released.\033[0m\n"
else
    echo -e "\n\n\033[0;32m*INFO* Scilla Path : ${scillaPath} not existed, we will NOT release Scilla.\033[0m\n"
fi

# Read information from files
constantFile="$(realpath ${constantFile})"
constantLookupFile="$(realpath ${constantLookupFile})"
constantArchivalFile="$(realpath ${constantArchivalFile})"
accountName="$(grep -oPm1 "(?<=<UPGRADE_HOST_ACCOUNT>)[^<]+" ${constantFile})"
repoName="$(grep -oPm1 "(?<=<UPGRADE_HOST_REPO>)[^<]+" ${constantFile})"
major="$(sed -n ${majorLine}p ${versionFile})"
minor="$(sed -n ${minorLine}p ${versionFile})"
fix="$(sed -n ${fixLine}p ${versionFile})"
DS="$(sed -n ${DSLine}p ${versionFile})"
commit="$(git describe --always)"

# Write new version information into version file
echo -e "Writing new software version into ${versionFile}..."
sed -i "${commitLine}s/.*/${commit}/" ${versionFile}
newVer=${major}.${minor}.${fix}.${DS}.${commit}
export ZIL_VER=${newVer}
export ZIL_PACK_NAME=${packageName}
echo -e "New software version: ${newVer} is written into ${versionFile} successfully.\n"

# Use cpack to making deb file
echo -e "Make deb package..."
rm -rf ${releaseDir}
cmake -H. -B${releaseDir} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local/
cmake --build ${releaseDir} --j4
cd ${releaseDir}; make package; cp ../${versionFile} .; debFile="$(ls *.deb)"; cd -
echo -e "Deb packages are generated successfully.\n"

# Make SHA-256 & multi-signature
echo -e "Making SHA-256 & multi-signature..."
privKeyFile="$(realpath ${privKeyFile})"
pubKeyFile="$(realpath ${pubKeyFile})"
cd ${releaseDir}
sha="$(sha256sum ${debFile}|cut -d ' ' -f1|tr 'a-z' 'A-Z')"
sed -i "${shaLine}s/.*/${sha}/" ${versionFile}
signature="$(./bin/signmultisig ${sha} ${privKeyFile} ${pubKeyFile})"
sed -i "${sigLine}s/.*/${signature}/" ${versionFile}
cd -
echo -e "SHA-256 & multi-signature are written into ${versionFile} successfully.\n"

#Sign the ds nodes and update the xml
cd ${releaseDir}
echo -e "Updating ds xml file"
cp ../constants_local.xml ./constants.xml
cp ../${dsNodeFile} ./${dsNodeFile}
ret="$(./bin/gensigninitialds   ${privKeyFile} ${pubKeyFile})"
cp ./${dsNodeFile} ../${dsNodeFile}
cd - 

# Upload package onto GitHub
echo -e "Creating new release and uploading package onto GitHub..."
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
    echo -e "*ERROR* Create new release fail! Please check input value and ${releaseLog}, then try again."
    return 1
fi
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/octet-stream" \
  --data-binary @${pubKeyFile} \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=$(basename {pubKeyFile})"
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/vnd.debian.binary-package" \
  --data-binary @${releaseDir}/${debFile} \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${debFile}"
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/octet-stream" \
  --data-binary @${releaseDir}/${versionFile} \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${versionFile}"
curl -v -s  \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/octet-stream"  \
  --data-binary @"${dsNodeFile}" \
  "https://uploads.github.com/repos/${accountName}/${repoName}/releases/${releaseId}/assets?name=${dsNodeFile}"
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
rm ${releaseLog}
echo -e "\nA new draft release with package is created on Github sucessfully, please proceed to publishing the draft release on Github webpage.\n"

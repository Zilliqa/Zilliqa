#!/bin/bash
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your participation in
# testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
# the protocols and algorithms that are programmed into, and intended by, the code. You may
# not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
# including modifying or publishing the code (or any part of it), and developing or forming
# another public or private blockchain network. This source code is provided ‘as is’ and no
# warranties are given as to title or non-infringement, merchantability or fitness for purpose
# and, to the extent permitted by law, all liability for your use of the code is disclaimed.
# Some programs in this code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
# GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
# and which include a reference to GPLv3 in their program files.

# User configuration settings, mandatory to be filled in
GitHubToken=""
repoName="PreRelease"    # Change to Zilliqa after PreRelease is ok
packageName=""
releaseTitle=""
releaseDescription=""

# Environment variables
releaseDir="release"
versionFile="VERSION"
majorLine=2
minorLine=4
fixLine=6
DSLine=8
commitLine=12
shaLine=14
sigLine=16

# Validate input argument
if [ "$#" -ne 2 ]; then
    echo "Usage: source ./release.sh privateKeyFileName publicKeyFileName"
    return 1
fi

if [ ! -f "$1" ]; then
    echo "*ERROR* File : $1 not found!"
    return 1
fi

if [ ! -f "$2" ]; then
    echo "*ERROR* File : $2 not found!"
    return 1
fi

if [ "$GitHubToken" = "" ]; then
    echo "*ERROR* Please enter your own GitHub token in release.sh!"
    return 1
fi

if [ "$packageName" = "" ]; then
    echo "*ERROR* Please enter the package name in release.sh!"
    return 1
fi

if [ "$releaseTitle" = "" ] || [ "$releaseDescription" = "" ]; then
    echo "*ERROR* Please enter the release title and description in release.sh!"
    return 1
fi

# Read current version information from version file
defaultMajor="$(sed -n ${majorLine}p ${versionFile})"
defaultMinor="$(sed -n ${minorLine}p ${versionFile})"
defaultFix="$(sed -n ${fixLine}p ${versionFile})"
defaultDS="$(sed -n ${DSLine}p ${versionFile})"
defaultCommit="$(sed -n ${commitLine}p ${versionFile})"
currentVer=${defaultMajor}.${defaultMinor}.${defaultFix}.${defaultDS}.${defaultCommit}

# Ask user to input new version information
echo -e "Current software version:  ${currentVer}"
read -p "Please enter major version [${defaultMajor}]: " major
major=${major:-${defaultMajor}}
read -p "Please enter minor version [${defaultMinor}]: " minor
minor=${minor:-${defaultMinor}}
read -p "Please enter fix version [${defaultFix}]: " fix
fix=${fix:-${defaultFix}}
read -p "Please enter expected DS epoch [${defaultDS}]: " DS
DS=${DS:-${defaultDS}}
commit="$(git describe --always)"

# Write new version information into version file
echo -e "Writing new software version into ${versionFile}..."
sed -i "${majorLine}s/.*/${major}/" ${versionFile}
sed -i "${minorLine}s/.*/${minor}/" ${versionFile}
sed -i "${fixLine}s/.*/${fix}/" ${versionFile}
sed -i "${DSLine}s/.*/${DS}/" ${versionFile}
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
privKeyFile="$(realpath $1)"
pubKeyFile="$(realpath $2)"
cd ${releaseDir}
sha="$(sha256sum ${debFile}|cut -d ' ' -f1)"
sed -i "${shaLine}s/.*/${sha}/" ${versionFile}
signature="$(./bin/signmultisig ${sha} ${privKeyFile} ${pubKeyFile})"
sed -i "${sigLine}s/.*/${signature}/" ${versionFile}
cd -
echo -e "SHA-256 & multi-signature are written into ${versionFile} successfully.\n"

# Upload package onto GitHub
echo -e "Creating new release and uploading package onto GitHub..."
fullCommit="$(git rev-parse HEAD)"
releaseLog="release.log"
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/json" "https://api.github.com/repos/Zilliqa/${repoName}/releases" \
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
  -H "Content-Type:application/json" \
  --data-binary @${pubKeyFile} \
  "https://uploads.github.com/repos/Zilliqa/${repoName}/releases/${releaseId}/assets?name=$(basename {pubKeyFile})" \
  -d '{
  "Content-Type": "application/octet-stream",
  "name": "'"$(basename ${pubKeyFile})"'",
  "label": "'"${newVer}"'"
}'
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/json" \
  --data-binary @${releaseDir}/${debFile} \
  "https://uploads.github.com/repos/Zilliqa/${repoName}/releases/${releaseId}/assets?name=${debFile}" \
  -d '{
  "Content-Type": "application/vnd.debian.binary-package",
  "name": "'"${debFile}"'",
  "label": "'"${newVer}"'"
}'
curl -v -s \
  -H "Authorization: token ${GitHubToken}" \
  -H "Content-Type:application/json" \
  --data-binary @${releaseDir}/${versionFile} \
  "https://uploads.github.com/repos/Zilliqa/${repoName}/releases/${releaseId}/assets?name=${versionFile}" \
  -d '{
  "Content-Type": "application/octet-stream",
  "name": "'"${versionFile}"'",
  "label": "'"${newVer}"'"
}'
rm ${releaseLog}
echo -e "\nA new draft release with package is created on Github sucessfully, please proceed to publishing the draft release on Github webpage.\n"


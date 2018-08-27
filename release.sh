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

# Configuration settings

fileName="VERSION"
majorLine=2
minorLine=4
fixLine=6
DSLine=8
commitLine=10

# Read current version information from version file
defaultMajor="$(sed -n ${majorLine}p ${fileName})"
defaultMinor="$(sed -n ${minorLine}p ${fileName})"
defaultFix="$(sed -n ${fixLine}p ${fileName})"
defaultDS="$(sed -n ${DSLine}p ${fileName})"
defaultCommit="$(sed -n ${commitLine}p ${fileName})"

# Ask user to input new version information
echo -e "Current software version: ${defaultMajor}.${defaultMinor}.${defaultFix}.${defaultDS}.${defaultCommit}"

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
echo -e "Writing new software version into ${fileName}..."

sed -i "${majorLine}s/.*/${major}/" ${fileName}
sed -i "${minorLine}s/.*/${minor}/" ${fileName}
sed -i "${fixLine}s/.*/${fix}/" ${fileName}
sed -i "${DSLine}s/.*/${DS}/" ${fileName}
sed -i "${commitLine}s/.*/${commit}/" ${fileName}

echo -e "New software version: ${major}.${minor}.${fix}.${DS}.${commit} is written into ${fileName} successfully.\n"

# Use cpack to making deb file

echo -e "Make deb file..."

# Make SHA-256 & multi-signature

echo -e "Calculate SHA-256..."
echo -e "Make multi-signature..."


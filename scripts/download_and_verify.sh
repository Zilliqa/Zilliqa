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

function main()
{
    #download
    rm -rf download
    mkdir download

    aws s3 cp s3://zilliqa-release-data/${S3FileName}.tar.gz ${S3FileName}.tar.gz > /dev/null 2>&1
    [ $? -ne 0 ] &&  echo "Failed to download from S3" >> download/fail && exit 1

    tar xzvf ${S3FileName}.tar.gz --directory download > /dev/null 2>&1
    [ $? -ne 0 ] &&  echo "Failed to extract ${S3FileName}.tar.gz" >> download/fail && exit 1

    cd download

    # pubkey verify if same
    [ "${i_pubKeys}" != "$(cat pubKeyFile | tr '\n' ' ')" ] && echo "PubKey mismatch" >> fail && exit 1

    # verify SHA and signature for correctness (Zilliqa)
    if [ ${i_upgrade} == 'zilliqa' ] || [ ${i_upgrade} == 'both' ]; then
          [ ! -f "${i_zilliqaDebFile}" ] && echo "No expected Zilliqa deb file downloaded" >> fail && exit 1
          [ "$(sha256sum ${i_zilliqaDebFile} | cut -d ' ' -f1|tr 'a-z' 'A-Z')" != "${i_zilliqaSha}" ] && echo "Sha of zilliqa deb package mismatch" >> fail && exit 1
          [ "PASS" != "$(/usr/local/bin/verifymultisig -m ${i_zilliqaSha} -s ${i_zilliqaSignature} -u pubKeyFile)" ] && echo "Zilliqa Signature verification failed" >> fail && exit 1
    fi

    # verify SHA and signature for correctness (Scilla)
    if [ ${i_upgrade} == 'scilla' ]; then
          [ ! -f "${i_scillaDebFile}" ] && echo "No expected Scilla deb file downloaded" >> fail && exit 1
          [ "$(sha256sum ${i_scillaDebFile} |cut -d ' ' -f1|tr 'a-z' 'A-Z')" != "${i_scillaSha}" ] && echo "Sha of zilliqa deb package mismatch" >> fail && exit 1
          [ "PASS" != "$(/usr/local/bin/verifymultisig -m ${i_scillaSha} -s ${i_scillaSignature} -u pubKeyFile)" ] && echo "Scilla Signature verification failed" >> fail && exit 1

          # just replace scilla 
          cd ..
          dpkg -i "download/${i_scillaDebFile}"
    fi
}

## main starts here
while getopts "h?u:k:z:i:q:s:p:r:d:" opt; do
    case "$opt" in
    h|\?)
        echo "$0 -u <<zilliqa|scilla|both>> -k <<public keys space separated>> -z <<zilliqa deb name>> -i <<zilliqaSha>> -q <<zilliqaSig>> -s <<scilla deb name>> -p <<scillaSha>>  -r <<scillaSig>> -d <<S3 database bucket name>>" 
        exit 0
        ;;
    u)  i_upgrade=$OPTARG
        ;;
    k)  i_pubKeys=$OPTARG
        ;;
    z)  i_zilliqaDebFile=$OPTARG
        ;;
    i)  i_zilliqaSha=$OPTARG
        ;;
    q)  i_zilliqaSignature=$OPTARG
        ;;
    s)  i_scillaDebFile=$OPTARG
        ;;
    p)  i_scillaSha=$OPTARG
        ;;
    r)  i_scillaSignature=$OPTARG
        ;;
    d)  S3FileName=$OPTARG
        ;;
    esac
done

shift $((OPTIND-1))

main

exit 0




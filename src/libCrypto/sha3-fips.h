/**
* Copyright (c) 2017 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
* This is a wrapper around the implmentation for SHA3-256 and SHA3-512 implementation 
*    by the Keccak, Keyak and Ketje Teams.
*
*    Do not use this file.
*    It is a wrapper: depends/ethash -> libCrypto/sha3-fips.h -> depends/sha3
*/

#ifndef __SHA3_FIPS_H__
#define __SHA3_FIPS_H__

#include "depends/Sha3/Sha3.c"

#define SHA256_HASH_BYTES 32
#define SHA512_HASH_BYTES 64

void SHA3_256(struct ethash_h256 const* ret, uint8_t const* data, size_t const size)
{
    unsigned char output[SHA256_HASH_BYTES] = ""; 
    FIPS202_SHA3_256(data, (unsigned int)size, output);
    memcpy((uint8_t*)ret, output, SHA256_HASH_BYTES);
}


void SHA3_512(uint8_t* ret, uint8_t const* data, size_t const size)
{
    unsigned char output[SHA512_HASH_BYTES] = ""; 
    FIPS202_SHA3_512(data, (unsigned int)size, output);
    memcpy(ret, output, SHA512_HASH_BYTES);
}

#endif // __SHA3_FIPS_H__

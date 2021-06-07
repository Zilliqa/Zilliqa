/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ZILLIQA_SRC_COMMON_MESSAGES_H_
#define ZILLIQA_SRC_COMMON_MESSAGES_H_

enum MessageOffset : unsigned int { TYPE = 0, INST = 1, BODY = 2 };

enum NumberSign : unsigned char { POSITIVE = 0x00, NEGATIVE = 0x01 };

enum MessageType : unsigned char {
  PEER = 0x00,
  DIRECTORY = 0x01,
  NODE = 0x02,
  CONSENSUSUSER =
      0x03,  // Note: this is a test class only, to demonstrate consensus usage
  LOOKUP = 0x04
};

enum DSInstructionType : unsigned char {
  SETPRIMARY = 0x00,
  POWSUBMISSION = 0x01,
  DSBLOCKCONSENSUS = 0x02,
  MICROBLOCKSUBMISSION = 0x03,
  FINALBLOCKCONSENSUS = 0x04,
  VIEWCHANGECONSENSUS = 0x05,
  VCPUSHLATESTDSTXBLOCK = 0x06,
  POWPACKETSUBMISSION = 0x07,
  NEWDSGUARDIDENTITY = 0x08,
  SETCOSIGSREWARDSFROMSEED = 0x09,
};

enum NodeInstructionType : unsigned char {
  STARTPOW = 0x00,
  DSBLOCK = 0x01,
  SUBMITTRANSACTION = 0x02,
  MICROBLOCKCONSENSUS = 0x03,
  FINALBLOCK = 0x04,
  MBNFORWARDTRANSACTION = 0x05,
  VCBLOCK = 0x06,
  DOREJOIN = 0x07,
  FORWARDTXNPACKET = 0x08,
  UNUSED_FALLBACKCONSENSUS = 0x09,
  UNUSED_FALLBACKBLOCK = 0x0A,
  PROPOSEGASPRICE = 0x0B,
  DSGUARDNODENETWORKINFOUPDATE = 0x0C,
  REMOVENODEFROMBLACKLIST = 0x0D,
  PENDINGTXN = 0x0E,
  VCFINALBLOCK = 0x0F,
  NEWSHARDNODEIDENTITY = 0x10,
  GETVERSION = 0x11,
  SETVERSION = 0x12
};

enum LookupInstructionType : unsigned char {
  GETDSINFOFROMSEED = 0x00,              // ProcessGetDSInfoFromSeed,
  SETDSINFOFROMSEED = 0x01,              // ProcessSetDSInfoFromSeed,
  GETDSBLOCKFROMSEED = 0x02,             // ProcessGetDSBlockFromSeed,
  SETDSBLOCKFROMSEED = 0x03,             // ProcessSetDSBlockFromSeed,
  GETTXBLOCKFROMSEED = 0x04,             // ProcessGetTxBlockFromSeed,
  SETTXBLOCKFROMSEED = 0x05,             // ProcessSetTxBlockFromSeed,
  GETSTATEFROMSEED = 0x06,               // UNUSED GETSTATEFROMSEED
  SETSTATEFROMSEED = 0x07,               // UNUSED SETSTATEFROMSEED
  SETLOOKUPOFFLINE = 0x08,               // ProcessSetLookupOffline,
  SETLOOKUPONLINE = 0x09,                // ProcessSetLookupOnline,
  GETOFFLINELOOKUPS = 0x0A,              // ProcessGetOfflineLookups,
  SETOFFLINELOOKUPS = 0x0B,              // ProcessSetOfflineLookups,
  RAISESTARTPOW = 0x0C,                  // UNUSED ProcessRaiseStartPoW
  GETSTARTPOWFROMSEED = 0x0D,            // UNUSED ProcessGetStartPoWFromSeed
  SETSTARTPOWFROMSEED = 0x0E,            // UNUSED ProcessSetStartPoWFromSeed
  GETSHARDSFROMSEED = 0x0F,              // UNUSED ProcessGetShardFromSeed,
  SETSHARDSFROMSEED = 0x10,              // UNUSED ProcessSetShardFromSeed
  GETMICROBLOCKFROMLOOKUP = 0x11,        // ProcessGetMicroBlockFromLookup,
  SETMICROBLOCKFROMLOOKUP = 0x12,        // ProcessSetMicroBlockFromLookup,
  GETTXNFROMLOOKUP = 0x13,               // ProcessGetTxnsFromLookup,
  SETTXNFROMLOOKUP = 0x14,               // ProcessSetTxnsFromLookup,
  GETDIRBLOCKSFROMSEED = 0x15,           // ProcessGetDirectoryBlocksFromSeed,
  SETDIRBLOCKSFROMSEED = 0x16,           // ProcessSetDirectoryBlocksFromSeed,
  GETSTATEDELTAFROMSEED = 0x17,          // ProcessGetStateDeltaFromSeed,
  GETSTATEDELTASFROMSEED = 0x18,         // ProcessGetStateDeltasFromSeed,
  SETSTATEDELTAFROMSEED = 0x19,          // ProcessSetStateDeltaFromSeed,
  SETSTATEDELTASFROMSEED = 0x1A,         // ProcessSetStateDeltasFromSeed,
  VCGETLATESTDSTXBLOCK = 0x1B,           // ProcessVCGetLatestDSTxBlockFromSeed,
  FORWARDTXN = 0x1C,                     // ProcessForwardTxn,
  GETGUARDNODENETWORKINFOUPDATE = 0x1D,  // ProcessGetDSGuardNetworkInfo,
  UNUSED_SETHISTORICALDB = 0x1E,         // Previously for SETHISTORICALDB
  GETCOSIGSREWARDSFROMSEED = 0x1F,       // ProcessGetCosigsRewardsFromSeed,
  SETMINERINFOFROMSEED = 0x20,           // ProcessSetMinerInfoFromSeed,
  GETDSBLOCKFROML2LDATAPROVIDER = 0x21,  // ProcessGetDSBlockFromL2l,
  GETVCFINALBLOCKFROML2LDATAPROVIDER = 0x22,  // ProcessGetVCFinalBlockFromL2l,
  GETMBNFWDTXNFROML2LDATAPROVIDER = 0x23,     // ProcessGetMBnForwardTxnFromL2l,
  GETPENDINGTXNFROML2LDATAPROVIDER =
      0x24,  // UNUSED GETPENDINGTXNFROML2LDATAPROVIDER
  GETMICROBLOCKFROML2LDATAPROVIDER = 0x25,  // ProcessGetMicroBlockFromL2l,
  GETTXNSFROML2LDATAPROVIDER = 0x26         // ProcessGetTxnsFromL2l
};

enum TxSharingMode : unsigned char {
  IDLE = 0x00,
  SEND_ONLY = 0x01,
  DS_FORWARD_ONLY = 0x02,
  NODE_FORWARD_ONLY = 0x03,
  SEND_AND_FORWARD = 0x04
};

#endif  // ZILLIQA_SRC_COMMON_MESSAGES_H_

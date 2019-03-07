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

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

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
  NEWDSGUARDIDENTITY = 0x08
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
  FALLBACKCONSENSUS = 0x09,
  FALLBACKBLOCK = 0x0A,
  PROPOSEGASPRICE = 0x0B,
  DSGUARDNODENETWORKINFOUPDATE = 0x0C,
};

enum LookupInstructionType : unsigned char {
  GETDSINFOFROMSEED = 0x00,
  SETDSINFOFROMSEED = 0x01,
  GETDSBLOCKFROMSEED = 0x02,
  SETDSBLOCKFROMSEED = 0x03,
  GETTXBLOCKFROMSEED = 0x04,
  SETTXBLOCKFROMSEED = 0x05,
  GETSTATEFROMSEED = 0x06,
  SETSTATEFROMSEED = 0x07,
  SETLOOKUPOFFLINE = 0x08,
  SETLOOKUPONLINE = 0x09,
  GETOFFLINELOOKUPS = 0x0A,
  SETOFFLINELOOKUPS = 0x0B,
  RAISESTARTPOW = 0x0C,
  GETSTARTPOWFROMSEED = 0x0D,
  SETSTARTPOWFROMSEED = 0x0E,
  GETSHARDSFROMSEED = 0x0F,        // UNUSED
  SETSHARDSFROMSEED = 0x10,        // UNUSED
  GETMICROBLOCKFROMLOOKUP = 0x11,  // UNUSED
  SETMICROBLOCKFROMLOOKUP = 0x12,  // UNUSED
  GETTXNFROMLOOKUP = 0x13,         // UNUSED
  SETTXNFROMLOOKUP = 0x14,         // UNUSED
  GETDIRBLOCKSFROMSEED = 0x15,
  SETDIRBLOCKSFROMSEED = 0x16,
  GETSTATEDELTAFROMSEED = 0x17,
  GETSTATEDELTASFROMSEED = 0x18,
  SETSTATEDELTAFROMSEED = 0x19,
  SETSTATEDELTASFROMSEED = 0x1A,
  VCGETLATESTDSTXBLOCK = 0x1B,
  FORWARDTXN = 0x1C,
  GETGUARDNODENETWORKINFOUPDATE = 0x1D,
  SETHISTORICALDB = 0x1E
};

enum TxSharingMode : unsigned char {
  IDLE = 0x00,
  SEND_ONLY = 0x01,
  DS_FORWARD_ONLY = 0x02,
  NODE_FORWARD_ONLY = 0x03,
  SEND_AND_FORWARD = 0x04
};

#endif  // __MESSAGES_H__

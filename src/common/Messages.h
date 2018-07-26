/**
* Copyright (c) 2018 Zilliqa 
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
**/

#ifndef __MESSAGES_H__
#define __MESSAGES_H__

enum MessageOffset : unsigned int
{
    TYPE = 0,
    INST = 1,
    BODY = 2
};

enum NumberSign : unsigned char
{
    POSITIVE = 0x00,
    NEGATIVE = 0x01
};

enum MessageType : unsigned char
{
    PEER = 0x00,
    DIRECTORY = 0x01,
    NODE = 0x02,
    CONSENSUSUSER
    = 0x03, // Note: this is a test class only, to demonstrate consensus usage
    LOOKUP = 0x04
};

enum DSInstructionType : unsigned char
{
    SETPRIMARY = 0x00,
    POWSUBMISSION = 0x01,
    DSBLOCKCONSENSUS = 0x02,
    POW2SUBMISSION = 0x03,
    SHARDINGCONSENSUS = 0x04,
    MICROBLOCKSUBMISSION = 0x05,
    FINALBLOCKCONSENSUS = 0x06,
    VIEWCHANGECONSENSUS = 0X07
};

enum NodeInstructionType : unsigned char
{
    STARTPOW = 0x00,
    DSBLOCK = 0x01,
    SHARDING = 0x02,
    CREATETRANSACTION = 0x03,
    SUBMITTRANSACTION = 0x04,
    MICROBLOCKCONSENSUS = 0x05,
    FINALBLOCK = 0x06,
    FORWARDTRANSACTION = 0x07,
    CREATETRANSACTIONFROMLOOKUP = 0x08,
    VCBLOCK = 0x09,
    FORWARDSTATEDELTA = 0x0A,
    DOREJOIN = 0x0B
};

enum LookupInstructionType : unsigned char
{
    ENTIRESHARDINGSTRUCTURE = 0x00,
    GETSEEDPEERS = 0x01,
    SETSEEDPEERS = 0x02,
    GETDSINFOFROMSEED = 0x03,
    SETDSINFOFROMSEED = 0x04,
    GETDSBLOCKFROMSEED = 0x05,
    SETDSBLOCKFROMSEED = 0x06,
    GETTXBLOCKFROMSEED = 0x07,
    SETTXBLOCKFROMSEED = 0x08,
    GETTXBODYFROMSEED = 0x09,
    SETTXBODYFROMSEED = 0x0A,
    GETNETWORKIDFROMSEED = 0x0B,
    SETNETWORKIDFROMSEED = 0x0C,
    GETSTATEFROMSEED = 0x0D,
    SETSTATEFROMSEED = 0x0E,
    SETLOOKUPOFFLINE = 0x0F,
    SETLOOKUPONLINE = 0x10,
    GETOFFLINELOOKUPS = 0x11,
    SETOFFLINELOOKUPS = 0x12
};

enum TxSharingMode : unsigned char
{
    IDLE = 0x00,
    SEND_ONLY = 0x01,
    DS_FORWARD_ONLY = 0x02,
    NODE_FORWARD_ONLY = 0x03,
    SEND_AND_FORWARD = 0x04
};

#endif // __MESSAGES_H__

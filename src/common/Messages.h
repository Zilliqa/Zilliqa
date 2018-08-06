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
    MICROBLOCKSUBMISSION = 0x03,
    FINALBLOCKCONSENSUS = 0x04,
    VIEWCHANGECONSENSUS = 0X05
};

enum NodeInstructionType : unsigned char
{
    STARTPOW = 0x00,
    DSBLOCK = 0x01,
    CREATETRANSACTION = 0x02,
    SUBMITTRANSACTION = 0x03,
    MICROBLOCKCONSENSUS = 0x04,
    FINALBLOCK = 0x05,
    FORWARDTRANSACTION = 0x06,
    CREATETRANSACTIONFROMLOOKUP = 0x07,
    VCBLOCK = 0x08,
    DOREJOIN = 0x09
};

enum LookupInstructionType : unsigned char
{
    GETSEEDPEERS = 0x00,
    SETSEEDPEERS = 0x01,
    GETDSINFOFROMSEED = 0x02,
    SETDSINFOFROMSEED = 0x03,
    GETDSBLOCKFROMSEED = 0x04,
    SETDSBLOCKFROMSEED = 0x05,
    GETTXBLOCKFROMSEED = 0x06,
    SETTXBLOCKFROMSEED = 0x07,
    GETTXBODYFROMSEED = 0x08,
    SETTXBODYFROMSEED = 0x09,
    GETNETWORKIDFROMSEED = 0x0A,
    SETNETWORKIDFROMSEED = 0x0B,
    GETSTATEFROMSEED = 0x0C,
    SETSTATEFROMSEED = 0x0D,
    SETLOOKUPOFFLINE = 0x0E,
    SETLOOKUPONLINE = 0x0F,
    GETOFFLINELOOKUPS = 0x10,
    SETOFFLINELOOKUPS = 0x11
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

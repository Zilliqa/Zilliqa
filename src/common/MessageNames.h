/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef __MESSAGE_NAMES_H__
#define __MESSAGE_NAMES_H__

#include <string>

#include "Messages.h"

#define MAKE_LITERAL_STRING(s) \
  { #s }
#define ARRAY_SIZE(s) (sizeof(s) / sizeof(s[0]))

static const std::string DSInstructionStrings[]{
    MAKE_LITERAL_STRING(SETPRIMARY),
    MAKE_LITERAL_STRING(POWSUBMISSION),
    MAKE_LITERAL_STRING(DSBLOCKCONSENSUS),
    MAKE_LITERAL_STRING(MICROBLOCKSUBMISSION),
    MAKE_LITERAL_STRING(FINALBLOCKCONSENSUS),
    MAKE_LITERAL_STRING(VIEWCHANGECONSENSUS),
    MAKE_LITERAL_STRING(VCPUSHLATESTDSTXBLOCK),
    MAKE_LITERAL_STRING(POWPACKETSUBMISSION)};

static_assert(ARRAY_SIZE(DSInstructionStrings) == POWPACKETSUBMISSION + 1,
              "DSInstructionStrings definition is not correct");

static const std::string NodeInstructionStrings[]{
    MAKE_LITERAL_STRING(STARTPOW),
    MAKE_LITERAL_STRING(DSBLOCK),
    MAKE_LITERAL_STRING(SUBMITTRANSACTION),
    MAKE_LITERAL_STRING(MICROBLOCKCONSENSUS),
    MAKE_LITERAL_STRING(FINALBLOCK),
    MAKE_LITERAL_STRING(FORWARDTRANSACTION),
    MAKE_LITERAL_STRING(VCBLOCK),
    MAKE_LITERAL_STRING(DOREJOIN),
    MAKE_LITERAL_STRING(FORWARDTXNPACKET),
    MAKE_LITERAL_STRING(FALLBACKCONSENSUS),
    MAKE_LITERAL_STRING(FALLBACKBLOCK),
    MAKE_LITERAL_STRING(PROPOSEGASPRICE)};

static_assert(ARRAY_SIZE(NodeInstructionStrings) == PROPOSEGASPRICE + 1,
              "NodeInstructionStrings definition is not correct");

static const std::string LookupInstructionStrings[]{
    MAKE_LITERAL_STRING(GETSEEDPEERS),
    MAKE_LITERAL_STRING(SETSEEDPEERS),
    MAKE_LITERAL_STRING(GETDSINFOFROMSEED),
    MAKE_LITERAL_STRING(SETDSINFOFROMSEED),
    MAKE_LITERAL_STRING(GETDSBLOCKFROMSEED),
    MAKE_LITERAL_STRING(SETDSBLOCKFROMSEED),
    MAKE_LITERAL_STRING(GETTXBLOCKFROMSEED),
    MAKE_LITERAL_STRING(SETTXBLOCKFROMSEED),
    MAKE_LITERAL_STRING(GETTXBODYFROMSEED),
    MAKE_LITERAL_STRING(SETTXBODYFROMSEED),
    MAKE_LITERAL_STRING(GETNETWORKIDFROMSEED),
    MAKE_LITERAL_STRING(SETNETWORKIDFROMSEED),
    MAKE_LITERAL_STRING(GETSTATEFROMSEED),
    MAKE_LITERAL_STRING(SETSTATEFROMSEED),
    MAKE_LITERAL_STRING(SETLOOKUPOFFLINE),
    MAKE_LITERAL_STRING(SETLOOKUPONLINE),
    MAKE_LITERAL_STRING(GETOFFLINELOOKUPS),
    MAKE_LITERAL_STRING(SETOFFLINELOOKUPS),
    MAKE_LITERAL_STRING(RAISESTARTPOW),
    MAKE_LITERAL_STRING(GETSTARTPOWFROMSEED),
    MAKE_LITERAL_STRING(SETSTARTPOWFROMSEED),
    MAKE_LITERAL_STRING(GETSHARDSFROMSEED),
    MAKE_LITERAL_STRING(SETSHARDSFROMSEED),
    MAKE_LITERAL_STRING(SETMICROBLOCKFROMSEED),
    MAKE_LITERAL_STRING(GETMICROBLOCKFROMLOOKUP),
    MAKE_LITERAL_STRING(SETMICROBLOCKFROMLOOKUP),
    MAKE_LITERAL_STRING(GETTXNFROMLOOKUP),
    MAKE_LITERAL_STRING(SETTXNFROMLOOKUP),
    MAKE_LITERAL_STRING(GETDIRBLOCKSFROMSEED),
    MAKE_LITERAL_STRING(SETDIRBLOCKSFROMSEED),
    MAKE_LITERAL_STRING(GETSTATEDELTAFROMSEED),
    MAKE_LITERAL_STRING(SETSTATEDELTAFROMSEED),
    MAKE_LITERAL_STRING(VCGETLATESTDSTXBLOCK)};

static_assert(ARRAY_SIZE(LookupInstructionStrings) == VCGETLATESTDSTXBLOCK + 1,
              "LookupInstructionStrings definition is not correct");

static const std::string *MessageTypeInstructionStrings[]{
    NULL, DSInstructionStrings, NodeInstructionStrings, NULL,
    LookupInstructionStrings};

static const int MessageTypeInstructionSize[]{
    0, ARRAY_SIZE(DSInstructionStrings), ARRAY_SIZE(NodeInstructionStrings), 0,
    ARRAY_SIZE(LookupInstructionStrings)};

static_assert(ARRAY_SIZE(MessageTypeInstructionStrings) ==
                  ARRAY_SIZE(MessageTypeInstructionSize),
              "Size of MessageTypeInstructionSize and "
              "MessageTypeInstructionStrings is not same");

static const std::string MessageTypeStrings[]{"PM", "DS", "NODE", "NULL",
                                              "LOOKUP"};

static_assert(
    ARRAY_SIZE(MessageTypeInstructionStrings) == ARRAY_SIZE(MessageTypeStrings),
    "Size of MessageTypeInstructionStrings and MessageTypeStrings is not same");

static const std::string MessageSizeKeyword = "Size of message ";
static const std::string MessgeTimeKeyword = "Time to process message ";

#endif  // __MESSAGE_NAMES_H__

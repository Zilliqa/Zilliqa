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

#ifndef ZILLIQA_SRC_COMMON_MESSAGENAMES_H_
#define ZILLIQA_SRC_COMMON_MESSAGENAMES_H_

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
    MAKE_LITERAL_STRING(MBNFORWARDTRANSACTION),
    MAKE_LITERAL_STRING(VCBLOCK),
    MAKE_LITERAL_STRING(DOREJOIN),
    MAKE_LITERAL_STRING(FORWARDTXNPACKET),
    MAKE_LITERAL_STRING(UNUSED_FALLBACKCONSENSUS),
    MAKE_LITERAL_STRING(UNUSED_FALLBACKBLOCK),
    MAKE_LITERAL_STRING(PROPOSEGASPRICE)};

static_assert(ARRAY_SIZE(NodeInstructionStrings) == PROPOSEGASPRICE + 1,
              "NodeInstructionStrings definition is not correct");

static const std::string LookupInstructionStrings[]{
    MAKE_LITERAL_STRING(GETDSINFOFROMSEED),
    MAKE_LITERAL_STRING(SETDSINFOFROMSEED),
    MAKE_LITERAL_STRING(GETDSBLOCKFROMSEED),
    MAKE_LITERAL_STRING(SETDSBLOCKFROMSEED),
    MAKE_LITERAL_STRING(GETTXBLOCKFROMSEED),
    MAKE_LITERAL_STRING(SETTXBLOCKFROMSEED),
    MAKE_LITERAL_STRING(GETSTATEFROMSEED),  // UNUSED
    MAKE_LITERAL_STRING(SETSTATEFROMSEED),  // UNUSED
    MAKE_LITERAL_STRING(SETLOOKUPOFFLINE),
    MAKE_LITERAL_STRING(SETLOOKUPONLINE),
    MAKE_LITERAL_STRING(GETOFFLINELOOKUPS),
    MAKE_LITERAL_STRING(SETOFFLINELOOKUPS),
    MAKE_LITERAL_STRING(RAISESTARTPOW),        // UNUSED
    MAKE_LITERAL_STRING(GETSTARTPOWFROMSEED),  // UNUSED
    MAKE_LITERAL_STRING(SETSTARTPOWFROMSEED),  // UNUSED
    MAKE_LITERAL_STRING(GETSHARDSFROMSEED),    // UNUSED
    MAKE_LITERAL_STRING(SETSHARDSFROMSEED),    // UNUSED
    MAKE_LITERAL_STRING(GETMICROBLOCKFROMLOOKUP),
    MAKE_LITERAL_STRING(SETMICROBLOCKFROMLOOKUP),
    MAKE_LITERAL_STRING(GETTXNFROMLOOKUP),
    MAKE_LITERAL_STRING(SETTXNFROMLOOKUP),
    MAKE_LITERAL_STRING(GETDIRBLOCKSFROMSEED),
    MAKE_LITERAL_STRING(SETDIRBLOCKSFROMSEED),
    MAKE_LITERAL_STRING(GETSTATEDELTAFROMSEED),
    MAKE_LITERAL_STRING(GETSTATEDELTASFROMSEED),
    MAKE_LITERAL_STRING(SETSTATEDELTAFROMSEED),
    MAKE_LITERAL_STRING(SETSTATEDELTASFROMSEED),
    MAKE_LITERAL_STRING(VCGETLATESTDSTXBLOCK),
    MAKE_LITERAL_STRING(FORWARDTXN),
    MAKE_LITERAL_STRING(GETGUARDNODENETWORKINFOUPDATE),
    MAKE_LITERAL_STRING(UNUSED_SETHISTORICALDB),
    MAKE_LITERAL_STRING(GETCOSIGSREWARDSFROMSEED),
    MAKE_LITERAL_STRING(SETMINERINFOFROMSEED),
    MAKE_LITERAL_STRING(GETDSBLOCKFROML2LDATAPROVIDER),
    MAKE_LITERAL_STRING(GETVCFINALBLOCKFROML2LDATAPROVIDER),
    MAKE_LITERAL_STRING(GETMBNFWDTXNFROML2LDATAPROVIDER),
    MAKE_LITERAL_STRING(GETPENDINGTXNFROML2LDATAPROVIDER),
    MAKE_LITERAL_STRING(GETMICROBLOCKFROML2LDATAPROVIDER),
    MAKE_LITERAL_STRING(GETTXNSFROML2LDATAPROVIDER)};

static_assert(ARRAY_SIZE(LookupInstructionStrings) ==
                  GETTXNSFROML2LDATAPROVIDER + 1,
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

#endif  // ZILLIQA_SRC_COMMON_MESSAGENAMES_H_

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

#ifndef __DSCOMPOSITION_H__
#define __DSCOMPOSITION_H__

#include "libCrypto/Schnorr.h"
#include "libData/BlockData/Block.h"
#include "libNetwork/Guard.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"

void InternalUpdateDSCommitteeComposition(const PubKey& selfKeyPub,
                                          DequeOfNode& dsComm,
                                          const DSBlock& dsblock);

#endif  // __DSCOMPOSITION_H__

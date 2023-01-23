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

#ifndef ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCK_H_
#define ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCK_H_

#include "BlockHashSet.h"
#include "DSBlock.h"
#include "DSBlockHeader.h"
#include "MicroBlock.h"
#include "MicroBlockHeader.h"
#include "TxBlock.h"
#include "TxBlockHeader.h"
#include "VCBlock.h"
#include "VCBlockHeader.h"

enum BlockType : unsigned int { DS = 0, Tx = 1, VC = 2 };

#endif  // ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCK_H_

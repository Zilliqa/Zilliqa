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

#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "Block/DSBlock.h"
#include "Block/FallbackBlock.h"
#include "Block/MicroBlock.h"
#include "Block/TxBlock.h"
#include "Block/VCBlock.h"
#include "BlockHeader/BlockHashSet.h"
#include "BlockHeader/DSBlockHeader.h"
#include "BlockHeader/FallbackBlockHeader.h"
#include "BlockHeader/MicroBlockHeader.h"
#include "BlockHeader/TxBlockHeader.h"
#include "BlockHeader/VCBlockHeader.h"

enum BlockType : unsigned int { DS = 0, Tx = 1, VC = 2, FB = 3 };

#endif  // __BLOCK_H__

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
#ifndef ZILLIQA_SRC_LIBMESSAGE_MESSENGERSWINFO_H_
#define ZILLIQA_SRC_LIBMESSAGE_MESSENGERSWINFO_H_

#include "common/BaseType.h"

class SWInfo;

class MessengerSWInfo {
 public:
  // ============================================================================
  // Primitives
  // ============================================================================

  static bool GetSWInfo(const zbytes& src, const unsigned int offset,
                        SWInfo& swInfo);

  static bool SetSWInfo(zbytes& dst, const unsigned int offset,
                        const SWInfo& swInfo);
};

#endif  // ZILLIQA_SRC_LIBMESSAGE_MESSENGERSWINFO_H_

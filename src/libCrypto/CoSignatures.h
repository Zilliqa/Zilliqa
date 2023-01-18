/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBCRYPTO_COSIGNATURES_H_
#define ZILLIQA_SRC_LIBCRYPTO_COSIGNATURES_H_

#include <Schnorr.h>

struct CoSignatures {
  Signature m_CS1;
  std::vector<bool> m_B1;
  Signature m_CS2;
  std::vector<bool> m_B2;

  CoSignatures(unsigned int bitmaplen = 1) : m_B1(bitmaplen), m_B2(bitmaplen) {}
  CoSignatures(const Signature& CS1, const std::vector<bool>& B1,
               const Signature& CS2, const std::vector<bool>& B2)
      : m_CS1(CS1), m_B1(B1), m_CS2(CS2), m_B2(B2) {}
};

#endif  // ZILLIQA_SRC_LIBCRYPTO_COSIGNATURES_H_

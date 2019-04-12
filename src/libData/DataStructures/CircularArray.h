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

#ifndef __CIRCULARARRAY_H__
#define __CIRCULARARRAY_H__

#include "common/BaseType.h"
#include "libUtils/Logger.h"

/// Utility class - circular array data queue.
template <class T>
class CircularArray {
 protected:
  std::vector<T> m_array;

  size_t m_capacity;

  /// return the actual size of how many blocks being stored in the array
  uint64_t m_size;

  /// return the index of the latest block inserted
  uint64_t m_index;

 public:
  /// Default constructor.
  CircularArray() {
    m_capacity = 0;
    m_size = 0;
  }

  /// Changes the array capacity.
  void resize(size_t capacity) {
    m_array.clear();
    m_array.resize(capacity);
    fill(m_array.begin(), m_array.end(), T());
    m_size = 0;
    m_index = 0;
    m_capacity = capacity;
  }

  CircularArray(const CircularArray<T>& circularArray) = delete;

  CircularArray& operator=(const CircularArray<T>& circularArray) = delete;

  /// Destructor.
  ~CircularArray() {}

  /// Index operator.
  T& operator[](uint64_t index) {
    if (!m_array.size()) {
      LOG_GENERAL(WARNING, "m_array is empty")
      throw;
    }
    return m_array[index % m_capacity];
  }

  /// Adds an element to the array at the specified index.
  void insert_new(uint64_t index, const T& element) {
    if (!m_array.size()) {
      LOG_GENERAL(WARNING, "m_array is empty")
      throw;
    }
    m_index = index % m_capacity;
    m_array[m_index] = element;
    m_size++;
  }

  /// Returns the element at the back of the array.
  T& back() {
    if (!m_array.size()) {
      LOG_GENERAL(WARNING, "m_array is empty")
      throw;
    }
    return m_array[m_index];
  }

  /// Returns the number of elements stored till now in the array.
  uint64_t size() { return m_size; }

  /// Increase size
  void increase_size(uint64_t size) { m_size += size; }

  /// Returns the storage capacity of the array.
  size_t capacity() { return m_capacity; }
};

#endif  // __CIRCULARARRAY_H__

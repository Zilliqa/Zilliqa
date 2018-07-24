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

#ifndef __CIRCULARARRAY_H__
#define __CIRCULARARRAY_H__

#include <boost/multiprecision/cpp_int.hpp>
#include <vector>

#include "libUtils/Logger.h"

/// Utility class - circular array data queue.
template<class T> class CircularArray
{
protected:
    std::vector<T> m_array;

    int m_capacity;

    /// return the actual size of how many blocks being stored in the array
    uint64_t m_size;

    /// return the index of the latest block inserted
    int m_index;

public:
    /// Default constructor.
    CircularArray()
    {
        m_capacity = 0;
        m_size = 0;
    }

    /// Changes the array capacity.
    void resize(int capacity)
    {
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
    T& operator[](uint64_t index)
    {
        if (!m_array.size())
        {
            LOG_GENERAL(WARNING, "m_array is empty")
            throw;
        }
        return m_array[(int)(index % m_capacity)];
    }

    /// Adds an element to the array at the specified index.
    void insert_new(uint64_t index, const T& element)
    {
        if (!m_array.size())
        {
            LOG_GENERAL(WARNING, "m_array is empty")
            throw;
        }
        m_index = (int)index % m_capacity;
        m_array[m_index] = element;
        m_size++;
    }

    /// Returns the element at the back of the array.
    T& back()
    {
        if (!m_array.size())
        {
            LOG_GENERAL(WARNING, "m_array is empty")
            throw;
        }
        return m_array[m_index];
    }

    /// Returns the number of elements stored till now in the array.
    uint64_t size() { return m_size; }

    /// Returns the storage capacity of the array.
    int capacity() { return m_capacity; }
};

#endif // __CIRCULARARRAY_H__

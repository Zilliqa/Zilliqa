/*
	This file is part of cpp-ethereum.
	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Common.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * Very common stuff (i.e. that every other header needs except vector_ref.h).
 */
// adapted from https://github.com/ethereum/cpp-ethereum/blob/develop/libdevcore/Common.h

#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "vector_ref.h"

// Binary data types.
using byte = unsigned char;

namespace dev
{
    using bytes = std::vector<byte>;
    using bytesRef = vector_ref<byte>;
    using bytesConstRef = vector_ref<byte const>;

    template<class T>
    class secure_vector {
    public:
        secure_vector() {}

        secure_vector(
                secure_vector<T> const & /*_c*/) = default;  // See https://github.com/ethereum/libweb3core/pull/44
        explicit secure_vector(size_t _size) : m_data(_size) {}

        explicit secure_vector(size_t _size, T _item) : m_data(_size, _item) {}

        explicit secure_vector(std::vector<T> const &_c) : m_data(_c) {}

        explicit secure_vector(vector_ref<T> _c) : m_data(_c.data(), _c.data() + _c.size()) {}

        explicit secure_vector(vector_ref<const T> _c) : m_data(_c.data(), _c.data() + _c.size()) {}

        ~secure_vector() { ref().cleanse(); }

        secure_vector<T> &operator=(secure_vector<T> const &_c) {
            if (&_c == this)
                return *this;

            ref().cleanse();
            m_data = _c.m_data;
            return *this;
        }

        std::vector<T> &writable() {
            clear();
            return m_data;
        }

        std::vector<T> const &makeInsecure() const { return m_data; }

        void clear() { ref().cleanse(); }

        vector_ref<T> ref() { return vector_ref<T>(&m_data); }

        vector_ref<T const> ref() const { return vector_ref<T const>(&m_data); }

        size_t size() const { return m_data.size(); }

        bool empty() const { return m_data.empty(); }

        void swap(secure_vector<T> &io_other) { m_data.swap(io_other.m_data); }

    private:
        std::vector<T> m_data;
    };

    using bytesSec = secure_vector<byte>;

    // Numeric types.
    using bigint = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>>;
    using u64 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<64, 64, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
    using u128 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<128, 128, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
    using u256 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
    using s256 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
    using u160 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<160, 160, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
    using s160 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<160, 160, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
    using u512 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<512, 512, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
    using s512 =  boost::multiprecision::number<boost::multiprecision::cpp_int_backend<512, 512, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
    using u256s = std::vector<u256>;
    using u160s = std::vector<u160>;
    using u256Set = std::set<u256>;
    using u160Set = std::set<u160>;

    // Map types.
    using StringMap = std::map<std::string, std::string>;
    using BytesMap = std::map<bytes, bytes>;
    using u256Map = std::map<u256, u256>;
    using HexMap = std::map<bytes, bytes>;

    // Hash types.
    using StringHashMap = std::unordered_map<std::string, std::string>;
    using u256HashMap = std::unordered_map<u256, u256>;

    // String types.
    using strings = std::vector<std::string>;

    // Fixed-length string types.
    using string32 = std::array<char, 32>;

    // Null/Invalid values for convenience.
    extern bytes const NullBytes;
    extern u256 const Invalid256;

    /// Get the current time in seconds since the epoch in UTC
    uint64_t utcTime();
}

namespace std
{
    template <> struct hash<dev::u256>
    {
        size_t operator()(dev::u256 const & _a) const
        {
            unsigned size = _a.backend().size();
            auto limbs = _a.backend().limbs();
            return boost::hash_range(limbs, limbs + size);
        }
    };
}

#endif // __COMMON_H__

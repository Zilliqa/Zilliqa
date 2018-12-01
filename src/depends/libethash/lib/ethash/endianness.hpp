// Copyright 2018 Pawel Bylica.
// Licensed under the Apache License, Version 2.0. See the LICENSE file.

/// @file
/// This file contains helper functions to handle big-endian architectures.
/// The Ethash algorithm is naturally defined for little-endian architectures
/// so for those the helpers are just no-op empty functions.
/// For big-endian architectures we need 32-bit and 64-bit byte swapping in
/// some places.

#pragma once

#include <ethash/ethash.hpp>

#if _WIN32

#include <stdlib.h>

#define bswap32 _byteswap_ulong
#define bswap64 _byteswap_uint64

// On Windows assume little endian.
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#elif __APPLE__

#include <machine/endian.h>

#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64

#else

#include <endian.h>

#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64

#endif

namespace ethash
{
#if __BYTE_ORDER == __LITTLE_ENDIAN

inline uint32_t fix_endianness(uint32_t x)
{
    return x;
}

inline uint64_t fix_endianness(uint64_t x)
{
    return x;
}

inline const hash1024& fix_endianness32(const hash1024& h)
{
    return h;
}

inline const hash512& fix_endianness32(const hash512& h)
{
    return h;
}

inline const hash512& fix_endianness64(const hash512& h)
{
    return h;
}

inline const hash256& fix_endianness32(const hash256& h)
{
    return h;
}

inline const hash256& fix_endianness64(const hash256& h)
{
    return h;
}

inline uint64_t from_be(uint64_t x)
{
    return bswap64(x);
}

#elif __BYTE_ORDER == __BIG_ENDIAN

inline uint32_t fix_endianness(uint32_t x)
{
    return bswap32(x);
}

inline uint64_t fix_endianness(uint64_t x)
{
    return bswap64(x);
}

inline hash1024 fix_endianness32(hash1024 hash)
{
    for (auto& w : hash.hwords)
        w = fix_endianness(w);
    return hash;
}

inline hash512 fix_endianness32(hash512 hash)
{
    for (auto& w : hash.half_words)
        w = fix_endianness(w);
    return hash;
}

inline hash512 fix_endianness64(hash512 hash)
{
    for (auto& w : hash.words)
        w = fix_endianness(w);
    return hash;
}

inline hash256 fix_endianness32(hash256 hash)
{
    for (auto& w : hash.hwords)
        w = fix_endianness(w);
    return hash;
}

inline hash256 fix_endianness64(hash256 hash)
{
    for (auto& w : hash.words)
        w = fix_endianness(w);
    return hash;
}

inline uint64_t from_be(uint64_t x)
{
    return x;
}

#endif
}  // namespace ethash
// ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
// Copyright 2018 Pawel Bylica.
// Licensed under the Apache License, Version 2.0. See the LICENSE file.

#include "ethash-internal.hpp"

#include "endianness.hpp"
#include "primes.h"

#include <ethash/keccak.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <limits>

#if __clang__
#define ATTRIBUTE_NO_SANITIZE_UNSIGNED_INTEGER_OVERFLOW \
    __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define ATTRIBUTE_NO_SANITIZE_UNSIGNED_INTEGER_OVERFLOW
#endif

namespace ethash
{
// Internal constants:
constexpr static int light_cache_init_size = 1 << 24;
constexpr static int light_cache_growth = 1 << 17;
constexpr static int light_cache_rounds = 3;
constexpr static int full_dataset_init_size = 1 << 30;
constexpr static int full_dataset_growth = 1 << 23;
constexpr static int full_dataset_item_parents = 256;

// Verify constants:
static_assert(sizeof(hash512) == ETHASH_LIGHT_CACHE_ITEM_SIZE, "");
static_assert(sizeof(hash1024) == ETHASH_FULL_DATASET_ITEM_SIZE, "");
static_assert(light_cache_item_size == ETHASH_LIGHT_CACHE_ITEM_SIZE, "");
static_assert(full_dataset_item_size == ETHASH_FULL_DATASET_ITEM_SIZE, "");


namespace
{
ATTRIBUTE_NO_SANITIZE_UNSIGNED_INTEGER_OVERFLOW
inline uint32_t fnv(uint32_t u, uint32_t v) noexcept
{
    return (u * 0x01000193) ^ v;
}

inline hash512 fnv(const hash512& u, const hash512& v) noexcept
{
    hash512 r;
    for (size_t i = 0; i < sizeof(r) / sizeof(r.half_words[0]); ++i)
        r.half_words[i] = fnv(u.half_words[i], v.half_words[i]);
    return r;
}

inline hash512 bitwise_xor(const hash512& x, const hash512& y) noexcept
{
    hash512 z;
    for (size_t i = 0; i < sizeof(z) / sizeof(z.words[0]); ++i)
        z.words[i] = x.words[i] ^ y.words[i];
    return z;
}
}  // namespace

hash256 calculate_seed(int epoch_number) noexcept
{
    hash256 seed = {};
    for (int i = 0; i < epoch_number; ++i)
        seed = keccak256(seed);
    return seed;
}

int find_epoch_number(const hash256& seed) noexcept
{
    static constexpr int num_tries = 30000;  // Divisible by 16.

    // Thread-local cache of the last search.
    static thread_local int cached_epoch_number = 0;
    static thread_local hash256 cached_seed = {};

    // Load from memory once (memory will be clobbered by keccak256()).
    uint32_t seed_part = seed.hwords[0];
    const int e = cached_epoch_number;
    hash256 s = cached_seed;

    if (s.hwords[0] == seed_part)
        return e;

    // Try the next seed, will match for sequential epoch access.
    s = keccak256(s);
    if (s.hwords[0] == seed_part)
    {
        cached_seed = s;
        cached_epoch_number = e + 1;
        return e + 1;
    }

    // Search for matching seed starting from epoch 0.
    s = {};
    for (int i = 0; i < num_tries; ++i)
    {
        if (s.hwords[0] == seed_part)
        {
            cached_seed = s;
            cached_epoch_number = i;
            return i;
        }

        s = keccak256(s);
    }

    return -1;
}

bool is_less_or_equal(const hash256& a, const hash256& b) noexcept
{
    for (size_t i = 0; i < (sizeof(a) / sizeof(a.words[0])); ++i)
    {
        if (from_be(a.words[i]) > from_be(b.words[i]))
            return false;
        if (from_be(a.words[i]) < from_be(b.words[i]))
            return true;
    }
    return true;
}

void build_light_cache(hash512* cache, int num_items, const hash256& seed) noexcept
{
    hash512 item = keccak512(seed.bytes, sizeof(seed));
    cache[0] = item;
    for (int i = 1; i < num_items; ++i)
    {
        item = keccak512(item);
        cache[i] = item;
    }

    for (int q = 0; q < light_cache_rounds; ++q)
    {
        for (int i = 0; i < num_items; ++i)
        {
            const uint32_t index_limit = static_cast<uint32_t>(num_items);

            // Fist index: 4 first bytes of the item as little-endian integer.
            uint32_t t = fix_endianness(cache[i].half_words[0]);
            uint32_t v = t % index_limit;

            // Second index.
            uint32_t w = static_cast<uint32_t>(num_items + (i - 1)) % index_limit;

            // Pipelining functions returning structs gives small performance boost.
            cache[i] = keccak512(bitwise_xor(cache[v], cache[w]));
        }
    }
}


/// Calculates a full dataset item
///
/// This consist of two 512-bit items produced by calculate_dataset_item_partial().
/// Here the computation is done interleaved for better performance.
hash1024 calculate_dataset_item(const epoch_context& context, uint32_t index) noexcept
{
    const hash512* const cache = context.light_cache;

    static constexpr size_t num_half_words = sizeof(hash512) / sizeof(uint32_t);
    const int64_t num_cache_items = context.light_cache_num_items;

    const int64_t index0 = int64_t(index) * 2;
    const int64_t index1 = int64_t(index) * 2 + 1;

    const uint32_t init0 = static_cast<uint32_t>(index0);
    const uint32_t init1 = static_cast<uint32_t>(index1);

    hash512 mix0 = cache[index0 % num_cache_items];
    hash512 mix1 = cache[index1 % num_cache_items];

    mix0.half_words[0] ^= fix_endianness(init0);
    mix1.half_words[0] ^= fix_endianness(init1);

    // Hash and convert to little-endian 32-bit words.
    mix0 = fix_endianness32(keccak512(mix0));
    mix1 = fix_endianness32(keccak512(mix1));

    for (uint32_t j = 0; j < full_dataset_item_parents; ++j)
    {
        uint32_t t0 = fnv(init0 ^ j, mix0.half_words[j % num_half_words]);
        int64_t parent_index0 = t0 % num_cache_items;
        mix0 = fnv(mix0, fix_endianness32(cache[parent_index0]));

        uint32_t t1 = fnv(init1 ^ j, mix1.half_words[j % num_half_words]);
        int64_t parent_index1 = t1 % num_cache_items;
        mix1 = fnv(mix1, fix_endianness32(cache[parent_index1]));
    }

    // Covert 32-bit words back to bytes and hash.
    mix0 = keccak512(fix_endianness32(mix0));
    mix1 = keccak512(fix_endianness32(mix1));

    return hash1024{{mix0, mix1}};
}

namespace
{
using lookup_fn = hash1024 (*)(const epoch_context&, uint32_t);

inline hash512 hash_seed(const hash256& header_hash, uint64_t nonce) noexcept
{
    nonce = fix_endianness(nonce);
    uint8_t init_data[sizeof(header_hash) + sizeof(nonce)];
    std::memcpy(&init_data[0], &header_hash, sizeof(header_hash));
    std::memcpy(&init_data[sizeof(header_hash)], &nonce, sizeof(nonce));

    return keccak512(init_data, sizeof(init_data));
}

inline hash256 hash_final(const hash512& seed, const hash256& mix_hash)
{
    uint8_t final_data[sizeof(seed) + sizeof(mix_hash)];
    std::memcpy(&final_data[0], seed.bytes, sizeof(seed));
    std::memcpy(&final_data[sizeof(seed)], mix_hash.bytes, sizeof(mix_hash));
    return keccak256(final_data, sizeof(final_data));
}

inline hash256 hash_kernel(
    const epoch_context& context, const hash512& seed, lookup_fn lookup) noexcept
{
    static constexpr size_t mix_hwords = sizeof(hash1024) / sizeof(uint32_t);
    const uint32_t index_limit = static_cast<uint32_t>(context.full_dataset_num_items);
    const uint32_t seed_init = fix_endianness(seed.half_words[0]);

    hash1024 mix{{fix_endianness32(seed), fix_endianness32(seed)}};

    for (uint32_t i = 0; i < num_dataset_accesses; ++i)
    {
        const uint32_t p = fnv(i ^ seed_init, mix.hwords[i % mix_hwords]) % index_limit;
        const hash1024 newdata = fix_endianness32(lookup(context, p));

        for (size_t j = 0; j < mix_hwords; ++j)
            mix.hwords[j] = fnv(mix.hwords[j], newdata.hwords[j]);
    }

    hash256 mix_hash;
    for (size_t i = 0; i < mix_hwords; i += 4)
    {
        const uint32_t h1 = fnv(mix.hwords[i], mix.hwords[i + 1]);
        const uint32_t h2 = fnv(h1, mix.hwords[i + 2]);
        const uint32_t h3 = fnv(h2, mix.hwords[i + 3]);
        mix_hash.hwords[i / 4] = h3;
    }

    return fix_endianness32(mix_hash);
}
}  // namespace

result hash(const epoch_context& context, const hash256& header_hash, uint64_t nonce) noexcept
{
    const hash512 seed = hash_seed(header_hash, nonce);
    const hash256 mix_hash = hash_kernel(context, seed, calculate_dataset_item);
    return {hash_final(seed, mix_hash), mix_hash};
}

result hash(const epoch_context_full& context, const hash256& header_hash, uint64_t nonce) noexcept
{
    static const auto lazy_lookup = [](const epoch_context& context, uint32_t index) noexcept
    {
        auto full_dataset = static_cast<const epoch_context_full&>(context).full_dataset;
        hash1024& item = full_dataset[index];
        if (item.words[0] == 0)
        {
            // TODO: Copy elision here makes it thread-safe?
            item = calculate_dataset_item(context, index);
        }

        return item;
    };

    const hash512 seed = hash_seed(header_hash, nonce);
    const hash256 mix_hash = hash_kernel(context, seed, lazy_lookup);
    return {hash_final(seed, mix_hash), mix_hash};
}

bool verify_final_hash(const hash256& header_hash, const hash256& mix_hash, uint64_t nonce,
    const hash256& boundary) noexcept
{
    const hash512 seed = hash_seed(header_hash, nonce);
    return is_less_or_equal(hash_final(seed, mix_hash), boundary);
}

bool verify(const epoch_context& context, const hash256& header_hash, const hash256& mix_hash,
    uint64_t nonce, const hash256& boundary) noexcept
{
    const hash512 seed = hash_seed(header_hash, nonce);
    if (!is_less_or_equal(hash_final(seed, mix_hash), boundary))
        return false;

    const hash256 expected_mix_hash = hash_kernel(context, seed, calculate_dataset_item);
    return std::memcmp(expected_mix_hash.bytes, mix_hash.bytes, sizeof(mix_hash)) == 0;
}

uint64_t search_light(const epoch_context& context, const hash256& header_hash,
    const hash256& boundary, uint64_t start_nonce, size_t iterations) noexcept
{
    const uint64_t end_nonce = start_nonce + iterations;
    for (uint64_t nonce = start_nonce; nonce < end_nonce; ++nonce)
    {
        result r = hash(context, header_hash, nonce);
        if (is_less_or_equal(r.final_hash, boundary))
            return nonce;
    }
    return 0;
}

uint64_t search(const epoch_context_full& context, const hash256& header_hash,
    const hash256& boundary, uint64_t start_nonce, size_t iterations) noexcept
{
    const uint64_t end_nonce = start_nonce + iterations;
    for (uint64_t nonce = start_nonce; nonce < end_nonce; ++nonce)
    {
        result r = hash(context, header_hash, nonce);
        if (is_less_or_equal(r.final_hash, boundary))
            return nonce;
    }
    return 0;
}
}  // namespace ethash

using namespace ethash;

extern "C" {

int ethash_calculate_light_cache_num_items(int epoch_number) noexcept
{
    static constexpr int item_size = sizeof(hash512);
    static constexpr int num_items_init = light_cache_init_size / item_size;
    static constexpr int num_items_growth = light_cache_growth / item_size;
    static_assert(
        light_cache_init_size % item_size == 0, "light_cache_init_size not multiple of item size");
    static_assert(
        light_cache_growth % item_size == 0, "light_cache_growth not multiple of item size");

    int num_items_upper_bound = num_items_init + epoch_number * num_items_growth;
    int num_items = ethash_find_largest_prime(num_items_upper_bound);
    return num_items;
}

int ethash_calculate_full_dataset_num_items(int epoch_number) noexcept
{
    static constexpr int item_size = sizeof(hash1024);
    static constexpr int num_items_init = full_dataset_init_size / item_size;
    static constexpr int num_items_growth = full_dataset_growth / item_size;
    static_assert(full_dataset_init_size % item_size == 0,
        "full_dataset_init_size not multiple of item size");
    static_assert(
        full_dataset_growth % item_size == 0, "full_dataset_growth not multiple of item size");

    int num_items_upper_bound = num_items_init + epoch_number * num_items_growth;
    int num_items = ethash_find_largest_prime(num_items_upper_bound);
    return num_items;
}

namespace
{
epoch_context_full* create_epoch_context(int epoch_number, bool full) noexcept
{
    static_assert(sizeof(epoch_context_full) < sizeof(hash512), "epoch_context too big");
    static constexpr size_t context_alloc_size = sizeof(hash512);

    const int light_cache_num_items = calculate_light_cache_num_items(epoch_number);
    const size_t light_cache_size = get_light_cache_size(light_cache_num_items);
    const size_t alloc_size = context_alloc_size + light_cache_size;

    char* const alloc_data = static_cast<char*>(std::malloc(alloc_size));
    if (!alloc_data)
        return nullptr;  // Signal out-of-memory by returning null pointer.

    hash512* const light_cache = reinterpret_cast<hash512*>(alloc_data + context_alloc_size);
    const hash256 seed = calculate_seed(epoch_number);
    build_light_cache(light_cache, light_cache_num_items, seed);

    const int full_dataset_num_items = calculate_full_dataset_num_items(epoch_number);
    hash1024* full_dataset = nullptr;
    if (full)
    {
        // TODO: This can be "optimized" by doing single allocation for light and full caches.
        const size_t num_items = static_cast<size_t>(full_dataset_num_items);
        full_dataset = static_cast<hash1024*>(std::calloc(num_items, sizeof(hash1024)));
        if (!full_dataset)
        {
            std::free(alloc_data);
            return nullptr;
        }
    }

    epoch_context_full* const context = new (alloc_data) epoch_context_full{
        epoch_number,
        light_cache_num_items,
        light_cache,
        full_dataset_num_items,
        full_dataset,
    };
    return context;
}
}  // namespace

epoch_context* ethash_create_epoch_context(int epoch_number) noexcept
{
    return create_epoch_context(epoch_number, false);
}

epoch_context_full* ethash_create_epoch_context_full(int epoch_number) noexcept
{
    return create_epoch_context(epoch_number, true);
}

void ethash_destroy_epoch_context_full(epoch_context_full* context) noexcept
{
    std::free(context->full_dataset);
    ethash_destroy_epoch_context(context);
}

void ethash_destroy_epoch_context(epoch_context* context) noexcept
{
    context->~epoch_context();
    std::free(context);
}

}  // extern "C"

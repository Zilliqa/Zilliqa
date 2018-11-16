// ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
// Copyright 2018 Pawel Bylica.
// Licensed under the Apache License, Version 2.0. See the LICENSE file.

/// @file
/// Contains declarations of internal ethash functions to allow them to be
/// unit-tested.

#pragma once

#include <ethash/ethash.hpp>

#include "endianness.hpp"

#include <memory>
#include <vector>

extern "C" struct ethash_epoch_context_full : ethash_epoch_context
{
    ethash_hash1024* full_dataset;

    constexpr ethash_epoch_context_full(int epoch_number, int light_cache_num_items,
        const ethash_hash512* light_cache, int full_dataset_num_items,
        ethash_hash1024* full_dataset) noexcept
      : ethash_epoch_context{epoch_number, light_cache_num_items, light_cache,
            full_dataset_num_items},
        full_dataset{full_dataset}
    {}
};

namespace ethash
{

hash256 calculate_seed(int epoch_number) noexcept;

void build_light_cache(hash512 cache[], int num_items, const hash256& seed) noexcept;

hash1024 calculate_dataset_item(const epoch_context& context, uint32_t index) noexcept;

}  // namespace ethash

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

#ifndef ZILLIQA_SRC_LIBCRYPTO_HASH_CALCULATOR_H_
#define ZILLIQA_SRC_LIBCRYPTO_HASH_CALCULATOR_H_

#include <openssl/evp.h>

#include <cassert>
#include <iterator>
#include <memory>
#include <ranges>

namespace zil {

namespace detail {

// The base version of the digest that should be specialized
// for each time of hash.
// In order to extend this to SHA-X, one basically needs to define:
//
// template <>
// struct EVPDigest<X> {
//   static const EVP_MD* impl() noexcept { return EVP_shaX(); }
// };
template <std::size_t N>
struct EVPDigest;

// Specialization for SHA256.
template <>
struct EVPDigest<256> {
  static const EVP_MD* impl() noexcept { return EVP_sha256(); }
};

}  // namespace detail

/**
 * @brief Calculates SHA2 hashes.
 * @tparam N The number of bits in the SHA2 hash (e.g 256).
 *
 * HashCalculator is a simple wrapper around OpenSSL's hash calculation
 * functions. Internally it uses SHA<N>_Init/Update/Final to calculate the hash.
 */
template <std::size_t N>
class HashCalculator final {
 public:
  ~HashCalculator() {
    if (m_owned) OPENSSL_free(m_ptr);
  }

  /**
   * @brief Default constructor.
   *
   * Will allocate & use internal storage (of DigestByteCount() bytes) to write
   * the hash to.
   */
  HashCalculator() : HashCalculator{nullptr, nullptr} {}

  /**
   * @brief Constructor.
   * @param outputFirst An iterator to the beginning of the storage.
   * @param outputLast An iterator to the end of the storage.
   * @throw std::runtime_error if the storage isn't large enough.
   *
   * Will use the storage in [outputFirst, outputLast) (which must be
   * contiguous) to write the hash to. HashCalculator doesn't take ownership of
   * the storage and it's the user's responsiblity to manage the memory.
   */
  template <typename IterT>
    requires std::contiguous_iterator<IterT>
  HashCalculator(IterT outputFirst, IterT outputLast)
      : HashCalculator{&(*outputFirst), &(*outputLast)} {}

  /**
   * @brief Constructor.
   * @param output An array-like object used for storage.
   * @throw std::runtime_error if the storage isn't large enough.
   *
   * Will use the storage in [std::ranges::begin(output),
   * std::ranges::end(output)) (which must be contiguous) to write the hash to.
   * HashCalculator doesn't take ownership of the storage and it's the user's
   * responsiblity to manage the memory.
   */
  template <typename ArrayLikeT>
  HashCalculator(ArrayLikeT& output)
      : HashCalculator{std::ranges::begin(output), std::ranges::end(output)} {}

  /**
   * @brief Constructor.
   * @param output An array used for storage.
   * @throw std::runtime_error if the storage isn't large enough.
   *
   * Will use the storage in [std::ranges::begin(output),
   * std::ranges::end(output)) (which must be contiguous) to write the hash to.
   * HashCalculator doesn't take ownership of the storage and it's the user's
   * responsiblity to manage the memory.
   */
  template <typename S, std::size_t M>
  HashCalculator(std::array<S, M>& output)
      : HashCalculator{std::ranges::begin(output), std::ranges::end(output)} {
    static_assert(M >= DigestByteCount(), "bad storage size");
  }

  /// Returns the number of bits in the digest.
  static constexpr auto DigestBitCount() noexcept { return N; }

  /// Returns the number of bytes in the digest.
  static constexpr auto DigestByteCount() noexcept {
    return DigestBitCount() / 8;
  }

  /**
   * @brief Updates the input to the hash.
   * @param first An iterator to the beginning of the input.
   * @param last An iterator to the end of the input.
   */
  template <typename IterT>
    requires std::contiguous_iterator<IterT>
  constexpr void Update(IterT first, IterT last) {
    EVP_DigestUpdate(context(), &(*first), std::distance(first, last));
  }

  /**
   * @brief Updates the input to the hash.
   * @param input Array-like input object.
   */
  template <typename ArrayLikeT>
  constexpr void Update(const ArrayLikeT& input) {
    // Don't allow C-style arrays because they will have a null-terminating character
    // at the end which will be used in the calculation (which is error-prone).
    static_assert(!std::is_array_v<ArrayLikeT>, "input mustn't be an array");
    Update(std::ranges::begin(input), std::ranges::end(input));
  }

  /**
   * @brief Finalizes the hash calculation.
   * @return A view of the storage of size DigestBitCount().
   */
  constexpr auto Finalize() {
    unsigned int outputLength = 0;
    EVP_DigestFinal_ex(context(), m_ptr, &outputLength);
    return std::ranges::views::counted(m_ptr, DigestByteCount());
  }

 private:
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> m_context;
  unsigned char* m_ptr = nullptr;
  bool m_owned = false;

  static auto evp() noexcept { return detail::EVPDigest<N>::impl(); }
  auto context() noexcept { return m_context.get(); }

  HashCalculator(unsigned char* first, unsigned char* last)
      : m_context{EVP_MD_CTX_new(), &EVP_MD_CTX_free},
        m_owned{first == nullptr} {
    assert(EVP_MD_size(evp()) == DigestByteCount());

    if (first) {
      if (std::distance(first, last) < EVP_MD_size(evp()))
        throw std::runtime_error{"bad storage size"};

      m_ptr = first;
    } else {
      m_ptr =
          reinterpret_cast<unsigned char*>(OPENSSL_malloc(EVP_MD_size(evp())));
    }

    EVP_DigestInit_ex(context(), evp(), nullptr);
  }
};

using SHA256Calculator = HashCalculator<256>;

}  // namespace zil

#endif  // ZILLIQA_SRC_LIBCRYPTO_HASH_CALCULATOR_H_

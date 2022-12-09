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
 *
 * Test cases obtained from https://www.di-mgt.com.au/sha_testvectors.html
 */

#include "libCrypto/HashCalculator.h"
#include "libUtils/DataConversion.h"

#define BOOST_TEST_MODULE hashcalculatortest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(hashcalculatortest)

BOOST_AUTO_TEST_CASE(BitByteCount) {
  zil::SHA256Calculator calc;

  BOOST_CHECK_EQUAL(EVP_MD_size(EVP_sha256()), 256 / 8);
  BOOST_CHECK_EQUAL(calc.DigestBitCount(), 256);
  BOOST_CHECK_EQUAL(calc.DigestByteCount(), 256 / 8);
}

BOOST_AUTO_TEST_CASE(EmptyMessageWithExternalStorage) {
  std::array<unsigned char, 32> hash;
  for (std::size_t i = 0; i < hash.size(); ++i) hash[i] = i;

  zil::SHA256Calculator calc{hash};

  for (std::size_t i = 0; i < hash.size(); ++i) BOOST_CHECK_EQUAL(hash[i], i);
}

struct Fixture {
  Fixture() : m_input{m_str} {}

  // We pass the string as input, not the array above because it would
  // include the additional null pointer at the end.
  const auto& input() const noexcept { return m_input; }

  template <typename SHA256T>
  void TestSHA256(const SHA256T& sha256, const std::string& hex) {
    BOOST_CHECK_EQUAL(sha256.size(), zil::SHA256Calculator::DigestByteCount());
    zbytes output;
    DataConversion::HexStrToUint8Vec(hex, output);
    BOOST_TEST(sha256 == output, boost::test_tools::per_element());
  }

 private:
  static constexpr const unsigned char m_str[] =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  std::basic_string_view<unsigned char> m_input;
};

BOOST_FIXTURE_TEST_CASE(EmptyMessageWithOwnStorage, Fixture) {
  zil::SHA256Calculator calc;
  auto sha256 = calc.Finalize();
  TestSHA256(
      sha256,
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

BOOST_FIXTURE_TEST_CASE(NotEnoughStorageThrows, Fixture) {
  std::vector<unsigned char> storage(16, 0);
  BOOST_CHECK_THROW(zil::SHA256Calculator{storage}, std::runtime_error);
}

BOOST_FIXTURE_TEST_CASE(CalcSHA256WithOwnStorage, Fixture) {
  zil::SHA256Calculator calc;
  calc.Update(input());
  calc.Update(input());
  calc.Update(input());
  auto sha256 = calc.Finalize();

  TestSHA256(
      sha256,
      "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C");
}

BOOST_FIXTURE_TEST_CASE(CalcSHA256WithExternalStorage, Fixture) {
  std::array<unsigned char, 32> sha256 = {};
  zil::SHA256Calculator calc{sha256};
  calc.Update(input());
  calc.Finalize();

  TestSHA256(
      sha256,
      "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");
}

BOOST_FIXTURE_TEST_CASE(CalcSHA256WithExternalStorageAndOffset, Fixture) {
  std::vector<unsigned char> storage(128, 0);
  zil::SHA256Calculator calc{storage.begin() + 50,
                                            storage.end()};
  calc.Update(input());
  calc.Update(input());
  calc.Update(input());
  auto sha256 = calc.Finalize();

  TestSHA256(
      sha256,
      "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C");

  BOOST_CHECK_EQUAL(&(*sha256.begin()), &(*(storage.begin() + 50)));
}

BOOST_FIXTURE_TEST_CASE(CalcSHA256EmptyInput, Fixture) {
  zil::SHA256Calculator calc;
  std::array<unsigned char, 3> s = {'a', 'b', 'c'};
  calc.Update(input());
  calc.Update(std::string{});
  calc.Update(input());
  calc.Update(s.begin(), s.begin());
  calc.Update(input());
  auto sha256 = calc.Finalize();

  TestSHA256(
      sha256,
      "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C");
}

BOOST_AUTO_TEST_SUITE_END()

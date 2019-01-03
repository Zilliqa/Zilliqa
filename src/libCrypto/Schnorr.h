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

#ifndef __SCHNORR_H__
#define __SCHNORR_H__

#include <openssl/bn.h>
#include <openssl/ec.h>

#include <array>
#include <memory>
#include <mutex>
#include <vector>

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libUtils/DataConversion.h"

/// Stores the NID_secp256k1 curve parameters for the elliptic curve scheme used
/// in Zilliqa.
struct Curve {
  /// EC group.
  std::shared_ptr<EC_GROUP> m_group;

  /// Order of the group.
  std::shared_ptr<BIGNUM> m_order;

  /// Constructor.
  Curve();

  /// Destructor.
  ~Curve();
};

/// EC-Schnorr utility for serializing BIGNUM data type.
struct BIGNUMSerialize {
  static std::mutex m_mutexBIGNUM;

  /// Deserializes a BIGNUM from specified byte stream.
  static std::shared_ptr<BIGNUM> GetNumber(const bytes& src,
                                           unsigned int offset,
                                           unsigned int size);

  /// Serializes a BIGNUM into specified byte stream.
  static void SetNumber(bytes& dst, unsigned int offset, unsigned int size,
                        std::shared_ptr<BIGNUM> value);
};

/// EC-Schnorr utility for serializing ECPOINT data type.
struct ECPOINTSerialize {
  static std::mutex m_mutexECPOINT;

  /// Deserializes an ECPOINT from specified byte stream.
  static std::shared_ptr<EC_POINT> GetNumber(const bytes& src,
                                             unsigned int offset,
                                             unsigned int size);

  /// Serializes an ECPOINT into specified byte stream.
  static void SetNumber(bytes& dst, unsigned int offset, unsigned int size,
                        std::shared_ptr<EC_POINT> value);
};

/// Stores information on an EC-Schnorr private key.
struct PrivKey : public Serializable {
  /// The scalar in the underlying field.
  std::shared_ptr<BIGNUM> m_d;

  /// Flag to indicate if parameters have been initialized.
  bool m_initialized;

  /// Default constructor for generating a new key.
  PrivKey();

  /// Constructor for loading existing key from a byte stream.
  PrivKey(const bytes& src, unsigned int offset);

  /// Copy constructor.
  PrivKey(const PrivKey& src);

  /// Destructor.
  ~PrivKey();

  /// Indicates if key parameters have been initialized.
  bool Initialized() const;

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset);

  /// Assignment operator.
  PrivKey& operator=(const PrivKey&);

  /// Utility std::string conversion function for private key info.
  explicit operator std::string() const {
    return "0x" + DataConversion::SerializableToHexStr(*this);
  }

  /// Equality comparison operator.
  bool operator==(const PrivKey& r) const;
};

inline std::ostream& operator<<(std::ostream& os, const PrivKey& p) {
  os << "0x" << DataConversion::SerializableToHexStr(p);
  return os;
}

/// Stores information on an EC-Schnorr public key.
struct PubKey : public Serializable {
  /// The point on the curve.
  std::shared_ptr<EC_POINT> m_P;

  /// Flag to indicate if parameters have been initialized.
  bool m_initialized;

  /// Default constructor for an uninitialized key.
  PubKey();

  /// Constructor for generating a new key from specified PrivKey.
  PubKey(const PrivKey& privkey);

  /// Constructor for loading existing key from a byte stream.
  PubKey(const bytes& src, unsigned int offset);

  /// Copy constructor.
  PubKey(const PubKey&);

  /// Destructor.
  ~PubKey();

  /// Indicates if key parameters have been initialized.
  bool Initialized() const;

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset);

  /// Assignment operator.
  PubKey& operator=(const PubKey& src);

  /// Less-than comparison operator (for sorting keys in lookup table).
  bool operator<(const PubKey& r) const;

  /// Greater-than comparison operator.
  bool operator>(const PubKey& r) const;

  /// Equality operator.
  bool operator==(const PubKey& r) const;

  /// Utility std::string conversion function for public key info.
  explicit operator std::string() const {
    return "0x" + DataConversion::SerializableToHexStr(*this);
  }
};

inline std::ostream& operator<<(std::ostream& os, const PubKey& p) {
  os << "0x" << DataConversion::SerializableToHexStr(p);
  return os;
}

/// Stores information on an EC-Schnorr signature.
struct Signature : public Serializable {
  /// Challenge scalar.
  std::shared_ptr<BIGNUM> m_r;

  /// Response scalar.
  std::shared_ptr<BIGNUM> m_s;

  /// Flag to indicate if parameters have been initialized.
  bool m_initialized;

  /// Default constructor.
  Signature();

  /// Constructor for loading existing signature from a byte stream.
  Signature(const bytes& src, unsigned int offset);

  /// Copy constructor.
  Signature(const Signature&);

  /// Destructor.
  ~Signature();

  /// Indicates if signature parameters have been initialized.
  bool Initialized() const;

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset);

  /// Assignment operator.
  Signature& operator=(const Signature&);

  /// Equality comparison operator.
  bool operator==(const Signature& r) const;

  /// Utility std::string conversion function for signature info.
  explicit operator std::string() const {
    return "0x" + DataConversion::SerializableToHexStr(*this);
  }
};

inline std::ostream& operator<<(std::ostream& os, const Signature& s) {
  os << "0x" << DataConversion::SerializableToHexStr(s);
  return os;
}

/// Implements the Elliptic Curve Based Schnorr Signature algorithm.
class Schnorr {
  Curve m_curve;

  Schnorr();
  ~Schnorr();

  Schnorr(Schnorr const&) = delete;
  void operator=(Schnorr const&) = delete;

 public:
  /// Public key is a point (x, y) on the curve.
  /// Each coordinate requires 32 bytes.
  /// In its compressed form it suffices to store the x co-ordinate and the sign
  /// for y. Hence a total of 33 bytes.
  static const unsigned int PUBKEY_COMPRESSED_SIZE_BYTES = 33;

  std::mutex m_mutexSchnorr;

  /// Returns the singleton Schnorr instance.
  static Schnorr& GetInstance();

  /// Returns the EC curve used.
  const Curve& GetCurve() const;

  /// Generates a new PrivKey and PubKey pair.
  std::pair<PrivKey, PubKey> GenKeyPair();

  /// Signs a message using the EC curve parameters and the specified key pair.
  bool Sign(const bytes& message, const PrivKey& privkey, const PubKey& pubkey,
            Signature& result);

  /// Signs a message using the EC curve parameters and the specified key pair.
  bool Sign(const bytes& message, unsigned int offset, unsigned int size,
            const PrivKey& privkey, const PubKey& pubkey, Signature& result);

  /// Checks the signature validity using the EC curve parameters and the
  /// specified PubKey.
  bool Verify(const bytes& message, const Signature& toverify,
              const PubKey& pubkey);

  /// Checks the signature validity using the EC curve parameters and the
  /// specified PubKey.
  bool Verify(const bytes& message, unsigned int offset, unsigned int size,
              const Signature& toverify, const PubKey& pubkey);

  /// Utility function for printing EC_POINT coordinates.
  void PrintPoint(const EC_POINT* point);
};

#endif  // __SCHNORR_H__

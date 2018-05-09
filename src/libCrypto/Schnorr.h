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

/// Stores the NID_secp256k1 curve parameters for the elliptic curve scheme used in Zilliqa.
struct Curve
{
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
struct BIGNUMSerialize
{
    static std::mutex m_mutexBIGNUM;

    /// Deserializes a BIGNUM from specified byte stream.
    static std::shared_ptr<BIGNUM>
    GetNumber(const std::vector<unsigned char>& src, unsigned int offset,
              unsigned int size);

    /// Serializes a BIGNUM into specified byte stream.
    static void SetNumber(std::vector<unsigned char>& dst, unsigned int offset,
                          unsigned int size, std::shared_ptr<BIGNUM> value);
};

/// EC-Schnorr utility for serializing ECPOINT data type.
struct ECPOINTSerialize
{
    static std::mutex m_mutexECPOINT;

    /// Deserializes an ECPOINT from specified byte stream.
    static std::shared_ptr<EC_POINT>
    GetNumber(const std::vector<unsigned char>& src, unsigned int offset,
              unsigned int size);

    /// Serializes an ECPOINT into specified byte stream.
    static void SetNumber(std::vector<unsigned char>& dst, unsigned int offset,
                          unsigned int size, std::shared_ptr<EC_POINT> value);
};

/// Stores information on an EC-Schnorr private key.
struct PrivKey : public Serializable
{
    /// The scalar in the underlying field.
    std::shared_ptr<BIGNUM> m_d;

    /// Flag to indicate if parameters have been initialized.
    bool m_initialized;

    /// Default constructor for generating a new key.
    PrivKey();

    /// Constructor for loading existing key from a byte stream.
    PrivKey(const std::vector<unsigned char>& src, unsigned int offset);

    /// Copy constructor.
    PrivKey(const PrivKey& src);

    /// Destructor.
    ~PrivKey();

    /// Indicates if key parameters have been initialized.
    bool Initialized() const;

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Assignment operator.
    PrivKey& operator=(const PrivKey&);

    /// Utility std::string conversion function for private key info.
    explicit operator std::string() const
    {
        return "0x" + DataConversion::SerializableToHexStr(*this);
    }

    /// Equality comparison operator.
    bool operator==(const PrivKey& r) const;
};

inline std::ostream& operator<<(std::ostream& os, const PrivKey& p)
{
    os << "0x" << DataConversion::SerializableToHexStr(p);
    return os;
}

/// Stores information on an EC-Schnorr public key.
struct PubKey : public Serializable
{
    /// The point on the curve.
    std::shared_ptr<EC_POINT> m_P;

    /// Flag to indicate if parameters have been initialized.
    bool m_initialized;

    /// Default constructor for an uninitialized key.
    PubKey();

    /// Constructor for generating a new key from specified PrivKey.
    PubKey(const PrivKey& privkey);

    /// Constructor for loading existing key from a byte stream.
    PubKey(const std::vector<unsigned char>& src, unsigned int offset);

    /// Copy constructor.
    PubKey(const PubKey&);

    /// Destructor.
    ~PubKey();

    /// Indicates if key parameters have been initialized.
    bool Initialized() const;

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Assignment operator.
    PubKey& operator=(const PubKey& src);

    /// Less-than comparison operator (for sorting keys in lookup table).
    bool operator<(const PubKey& r) const;

    /// Greater-than comparison operator.
    bool operator>(const PubKey& r) const;

    /// Equality operator.
    bool operator==(const PubKey& r) const;

    /// Utility std::string conversion function for public key info.
    explicit operator std::string() const
    {
        return "0x" + DataConversion::SerializableToHexStr(*this);
    }
};

inline std::ostream& operator<<(std::ostream& os, const PubKey& p)
{
    os << "0x" << DataConversion::SerializableToHexStr(p);
    return os;
}

/// Stores information on an EC-Schnorr signature.
struct Signature : public Serializable
{
    /// Challenge scalar.
    std::shared_ptr<BIGNUM> m_r;

    /// Response scalar.
    std::shared_ptr<BIGNUM> m_s;

    /// Flag to indicate if parameters have been initialized.
    bool m_initialized;

    /// Default constructor.
    Signature();

    /// Constructor for loading existing signature from a byte stream.
    Signature(const std::vector<unsigned char>& src, unsigned int offset);

    /// Copy constructor.
    Signature(const Signature&);

    /// Destructor.
    ~Signature();

    /// Indicates if signature parameters have been initialized.
    bool Initialized() const;

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Assignment operator.
    Signature& operator=(const Signature&);

    /// Equality comparison operator.
    bool operator==(const Signature& r) const;

    /// Utility std::string conversion function for signature info.
    explicit operator std::string() const
    {
        return "0x" + DataConversion::SerializableToHexStr(*this);
    }
};

inline std::ostream& operator<<(std::ostream& os, const Signature& s)
{
    os << "0x" << DataConversion::SerializableToHexStr(s);
    return os;
}

/// Implements the Elliptic Curve Based Schnorr Signature algorithm.
class Schnorr
{
    Curve m_curve;

    Schnorr();
    ~Schnorr();

public:
    /// Public key is a point (x, y) on the curve.
    /// Each coordinate requires 32 bytes.
    /// In its compressed form it suffices to store the x co-ordinate and the sign for y.
    /// Hence a total of 33 bytes.
    static const unsigned int PUBKEY_COMPRESSED_SIZE_BYTES = 33;

    std::mutex m_mutexSchnorr;

    /// Returns the singleton Schnorr instance.
    static Schnorr& GetInstance();

    /// Returns the EC curve used.
    const Curve& GetCurve() const;

    /// Generates a new PrivKey and PubKey pair.
    std::pair<PrivKey, PubKey> GenKeyPair();

    /// Signs a message using the EC curve parameters and the specified key pair.
    bool Sign(const std::vector<unsigned char>& message, const PrivKey& privkey,
              const PubKey& pubkey, Signature& result);

    /// Signs a message using the EC curve parameters and the specified key pair.
    bool Sign(const std::vector<unsigned char>& message, unsigned int offset,
              unsigned int size, const PrivKey& privkey, const PubKey& pubkey,
              Signature& result);

    /// Checks the signature validity using the EC curve parameters and the specified PubKey.
    bool Verify(const std::vector<unsigned char>& message,
                const Signature& toverify, const PubKey& pubkey);

    /// Checks the signature validity using the EC curve parameters and the specified PubKey.
    bool Verify(const std::vector<unsigned char>& message, unsigned int offset,
                unsigned int size, const Signature& toverify,
                const PubKey& pubkey);

    /// Utility function for printing EC_POINT coordinates.
    void PrintPoint(const EC_POINT* point);
};

#endif // __SCHNORR_H__

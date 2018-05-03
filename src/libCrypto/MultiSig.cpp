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

#include "MultiSig.h"
#include "Sha2.h"
#include "libUtils/Logger.h"

using namespace std;

CommitSecret::CommitSecret()
    : m_s(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    // commit->secret should be in [2,...,order-1]
    // -1 means no constraint on the MSB of kpriv->d
    // 0 means no constraint on the LSB of kpriv->d

    if (m_s == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }

    bool err = false;

    do
    {
        const Curve& curve = Schnorr::GetInstance().GetCurve();

        err = (BN_rand(m_s.get(), BN_num_bits(curve.m_order.get()), -1, 0)
               == 0);
        if (err)
        {
            LOG_GENERAL(WARNING, "Value to commit rand failed");
            break;
        }

        err = (BN_nnmod(m_s.get(), m_s.get(), curve.m_order.get(), NULL) == 0);
        if (err)
        {
            LOG_GENERAL(WARNING, "Value to commit gen failed");
            break;
        }
    } while (BN_is_zero(m_s.get()) || BN_is_one(m_s.get()));

    m_initialized = (err == false);
}

CommitSecret::CommitSecret(const vector<unsigned char>& src,
                           unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init CommitSecret.");
    }
}

CommitSecret::CommitSecret(const CommitSecret& src)
    : m_s(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    if (m_s != nullptr)
    {
        if (BN_copy(m_s.get(), src.m_s.get()) == NULL)
        {
            LOG_GENERAL(WARNING, "CommitSecret copy failed");
        }
        else
        {
            m_initialized = true;
        }
    }
    else
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
}

CommitSecret::~CommitSecret() {}

bool CommitSecret::Initialized() const { return m_initialized; }

unsigned int CommitSecret::Serialize(vector<unsigned char>& dst,
                                     unsigned int offset) const
{
    // LOG_MARKER();

    if (m_initialized)
    {
        BIGNUMSerialize::SetNumber(dst, offset, COMMIT_SECRET_SIZE, m_s);
    }

    return COMMIT_SECRET_SIZE;
}

int CommitSecret::Deserialize(const vector<unsigned char>& src,
                              unsigned int offset)
{
    // LOG_MARKER();

    try
    {
        m_s = BIGNUMSerialize::GetNumber(src, offset, COMMIT_SECRET_SIZE);
        if (m_s == nullptr)
        {
            LOG_GENERAL(WARNING, "Deserialization failure");
            m_initialized = false;
        }
        else
        {
            m_initialized = true;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with CommitSecret::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

CommitSecret& CommitSecret::operator=(const CommitSecret& src)
{
    m_initialized = (BN_copy(m_s.get(), src.m_s.get()) == m_s.get());
    return *this;
}

bool CommitSecret::operator==(const CommitSecret& r) const
{
    return (m_initialized && r.m_initialized
            && (BN_cmp(m_s.get(), r.m_s.get()) == 0));
}

CommitPoint::CommitPoint()
    : m_p(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free)
    , m_initialized(false)
{
    if (m_p == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
}

CommitPoint::CommitPoint(const CommitSecret& secret)
    : m_p(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free)
    , m_initialized(false)
{
    if (m_p == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }

    Set(secret);
}

CommitPoint::CommitPoint(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init CommitPoint.");
    }
}

CommitPoint::CommitPoint(const CommitPoint& src)
    : m_p(EC_POINT_new(Schnorr::GetInstance().GetCurve().m_group.get()),
          EC_POINT_clear_free)
    , m_initialized(false)
{
    if (m_p == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
    else
    {
        if (EC_POINT_copy(m_p.get(), src.m_p.get()) != 1)
        {
            LOG_GENERAL(WARNING, "CommitPoint copy failed");
        }
        else
        {
            m_initialized = true;
        }
    }
}

CommitPoint::~CommitPoint() {}

bool CommitPoint::Initialized() const { return m_initialized; }

unsigned int CommitPoint::Serialize(vector<unsigned char>& dst,
                                    unsigned int offset) const
{
    // LOG_MARKER();

    if (m_initialized)
    {
        ECPOINTSerialize::SetNumber(dst, offset, COMMIT_POINT_SIZE, m_p);
    }

    return COMMIT_POINT_SIZE;
}

int CommitPoint::Deserialize(const vector<unsigned char>& src,
                             unsigned int offset)
{
    // LOG_MARKER();

    try
    {
        m_p = ECPOINTSerialize::GetNumber(src, offset, COMMIT_POINT_SIZE);
        if (m_p == nullptr)
        {
            LOG_GENERAL(WARNING, "Deserialization failure");
            m_initialized = false;
        }
        else
        {
            m_initialized = true;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with CommitPoint::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

void CommitPoint::Set(const CommitSecret& secret)
{
    if (!secret.Initialized())
    {
        LOG_GENERAL(WARNING, "Commitment secret value not initialized");
        return;
    }

    if (EC_POINT_mul(Schnorr::GetInstance().GetCurve().m_group.get(), m_p.get(),
                     secret.m_s.get(), NULL, NULL, NULL)
        != 1)
    {
        LOG_GENERAL(WARNING, "Commit gen failed");
        m_initialized = false;
    }
    else
    {
        m_initialized = true;
    }
}

CommitPoint& CommitPoint::operator=(const CommitPoint& src)
{
    m_initialized = (EC_POINT_copy(m_p.get(), src.m_p.get()) == 1);
    return *this;
}

bool CommitPoint::operator==(const CommitPoint& r) const
{
    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
    if (ctx == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return false;
    }

    return (m_initialized && r.m_initialized
            && (EC_POINT_cmp(Schnorr::GetInstance().GetCurve().m_group.get(),
                             m_p.get(), r.m_p.get(), ctx.get())
                == 0));
}

Challenge::Challenge()
    : m_c(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    if (m_c == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
}

Challenge::Challenge(const CommitPoint& aggregatedCommit,
                     const PubKey& aggregatedPubkey,
                     const vector<unsigned char>& message)
    : Challenge(aggregatedCommit, aggregatedPubkey, message, 0, message.size())
{
}

Challenge::Challenge(const CommitPoint& aggregatedCommit,
                     const PubKey& aggregatedPubkey,
                     const vector<unsigned char>& message, unsigned int offset,
                     unsigned int size)
    : m_c(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    if (m_c == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }

    Set(aggregatedCommit, aggregatedPubkey, message, offset, size);
}

Challenge::Challenge(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init Challenge.");
    }
}

Challenge::Challenge(const Challenge& src)
    : m_c(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    if (m_c != nullptr)
    {
        if (BN_copy(m_c.get(), src.m_c.get()) == NULL)
        {
            LOG_GENERAL(WARNING, "Challenge copy failed");
        }
        else
        {
            m_initialized = true;
        }
    }
    else
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
}

Challenge::~Challenge() {}

bool Challenge::Initialized() const { return m_initialized; }

unsigned int Challenge::Serialize(vector<unsigned char>& dst,
                                  unsigned int offset) const
{
    // LOG_MARKER();

    if (m_initialized)
    {
        BIGNUMSerialize::SetNumber(dst, offset, CHALLENGE_SIZE, m_c);
    }

    return CHALLENGE_SIZE;
}

int Challenge::Deserialize(const vector<unsigned char>& src,
                           unsigned int offset)
{
    // LOG_MARKER();

    try
    {
        m_c = BIGNUMSerialize::GetNumber(src, offset, CHALLENGE_SIZE);
        if (m_c == nullptr)
        {
            LOG_GENERAL(WARNING, "Deserialization failure");
            m_initialized = false;
        }
        else
        {
            m_initialized = true;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with Challenge::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

void Challenge::Set(const CommitPoint& aggregatedCommit,
                    const PubKey& aggregatedPubkey,
                    const vector<unsigned char>& message, unsigned int offset,
                    unsigned int size)
{
    // Initial checks

    if (!aggregatedCommit.Initialized())
    {
        LOG_GENERAL(WARNING, "Aggregated commit not initialized");
        return;
    }

    if (!aggregatedPubkey.Initialized())
    {
        LOG_GENERAL(WARNING, "Public key not initialized");
        return;
    }

    if (message.size() == 0)
    {
        LOG_GENERAL(WARNING, "Empty message");
        return;
    }

    if (message.size() < (offset + size))
    {
        LOG_GENERAL(WARNING, "Offset and size outside message length");
        return;
    }

    // Compute the challenge c = H(r, kpub, m)

    m_initialized = false;

    vector<unsigned char> buf(Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

    const Curve& curve = Schnorr::GetInstance().GetCurve();

    // Convert the committment to octets first
    if (EC_POINT_point2oct(curve.m_group.get(), aggregatedCommit.m_p.get(),
                           POINT_CONVERSION_COMPRESSED, buf.data(),
                           Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES, NULL)
        != Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES)
    {
        LOG_GENERAL(WARNING, "Could not convert commitment to octets");
        return;
    }

    // Hash commitment
    sha2.Update(buf);

    // Clear buffer
    fill(buf.begin(), buf.end(), 0x00);

    // Convert the public key to octets
    if (EC_POINT_point2oct(curve.m_group.get(), aggregatedPubkey.m_P.get(),
                           POINT_CONVERSION_COMPRESSED, buf.data(),
                           Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES, NULL)
        != Schnorr::PUBKEY_COMPRESSED_SIZE_BYTES)
    {
        LOG_GENERAL(WARNING, "Could not convert public key to octets");
        return;
    }

    // Hash public key
    sha2.Update(buf);

    // Hash message
    sha2.Update(message, offset, size);
    vector<unsigned char> digest = sha2.Finalize();

    // Build the challenge
    if ((BN_bin2bn(digest.data(), digest.size(), m_c.get())) == NULL)
    {
        LOG_GENERAL(WARNING, "Digest to challenge failed");
        return;
    }

    if (BN_nnmod(m_c.get(), m_c.get(), curve.m_order.get(), NULL) == 0)
    {
        LOG_GENERAL(WARNING, "Could not reduce challenge modulo group order");
        return;
    }

    m_initialized = true;
}

Challenge& Challenge::operator=(const Challenge& src)
{
    m_initialized = (BN_copy(m_c.get(), src.m_c.get()) == m_c.get());
    return *this;
}

bool Challenge::operator==(const Challenge& r) const
{
    return (m_initialized && r.m_initialized
            && (BN_cmp(m_c.get(), r.m_c.get()) == 0));
}

Response::Response()
    : m_r(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    if (m_r == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
}

Response::Response(const CommitSecret& secret, const Challenge& challenge,
                   const PrivKey& privkey)
    : m_r(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    // Initial checks

    if (m_r == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }

    Set(secret, challenge, privkey);
}

Response::Response(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init Response.");
    }
}

Response::Response(const Response& src)
    : m_r(BN_new(), BN_clear_free)
    , m_initialized(false)
{
    if (m_r != nullptr)
    {
        if (BN_copy(m_r.get(), src.m_r.get()) == NULL)
        {
            LOG_GENERAL(WARNING, "Response copy failed");
        }
        else
        {
            m_initialized = true;
        }
    }
    else
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
    }
}

Response::~Response() {}

bool Response::Initialized() const { return m_initialized; }

unsigned int Response::Serialize(vector<unsigned char>& dst,
                                 unsigned int offset) const
{
    // LOG_MARKER();

    if (m_initialized)
    {
        BIGNUMSerialize::SetNumber(dst, offset, RESPONSE_SIZE, m_r);
    }

    return RESPONSE_SIZE;
}

int Response::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{
    // LOG_MARKER();

    try
    {
        m_r = BIGNUMSerialize::GetNumber(src, offset, RESPONSE_SIZE);
        if (m_r == nullptr)
        {
            LOG_GENERAL(WARNING, "Deserialization failure");
            m_initialized = false;
        }
        else
        {
            m_initialized = true;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with Response::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

void Response::Set(const CommitSecret& secret, const Challenge& challenge,
                   const PrivKey& privkey)
{
    // Initial checks

    if (m_initialized)
    {
        LOG_GENERAL(WARNING, "Response already initialized");
        return;
    }

    if (!secret.Initialized())
    {
        LOG_GENERAL(WARNING, "Commit secret not initialized");
        return;
    }

    if (!challenge.Initialized())
    {
        LOG_GENERAL(WARNING, "Challenge not initialized");
        return;
    }

    if (!privkey.Initialized())
    {
        LOG_GENERAL(WARNING, "Private key not initialized");
        return;
    }

    m_initialized = false;

    // Compute s = k - krpiv*c
    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
    if (ctx == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return;
    }

    const Curve& curve = Schnorr::GetInstance().GetCurve();

    // kpriv*c
    if (BN_mod_mul(m_r.get(), challenge.m_c.get(), privkey.m_d.get(),
                   curve.m_order.get(), ctx.get())
        == 0)
    {
        LOG_GENERAL(WARNING, "BIGNUM mod mul failed");
        return;
    }

    // k-kpriv*c
    if (BN_mod_sub(m_r.get(), secret.m_s.get(), m_r.get(), curve.m_order.get(),
                   ctx.get())
        == 0)
    {
        LOG_GENERAL(WARNING, "BIGNUM mod add failed");
        return;
    }

    m_initialized = true;
}

Response& Response::operator=(const Response& src)
{
    m_initialized = (BN_copy(m_r.get(), src.m_r.get()) == m_r.get());
    return *this;
}

bool Response::operator==(const Response& r) const
{
    return (m_initialized && r.m_initialized
            && (BN_cmp(m_r.get(), r.m_r.get()) == 0));
}

shared_ptr<PubKey> MultiSig::AggregatePubKeys(const vector<PubKey>& pubkeys)
{
    const Curve& curve = Schnorr::GetInstance().GetCurve();

    if (pubkeys.size() == 0)
    {
        LOG_GENERAL(WARNING, "Empty list of public keys");
        return nullptr;
    }

    shared_ptr<PubKey> aggregatedPubkey(new PubKey(pubkeys.at(0)));
    if (aggregatedPubkey == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return nullptr;
    }

    for (unsigned int i = 1; i < pubkeys.size(); i++)
    {
        if (EC_POINT_add(curve.m_group.get(), aggregatedPubkey->m_P.get(),
                         aggregatedPubkey->m_P.get(), pubkeys.at(i).m_P.get(),
                         NULL)
            == 0)
        {
            LOG_GENERAL(WARNING, "Pubkey aggregation failed");
            return nullptr;
        }
    }

    return aggregatedPubkey;
}

shared_ptr<CommitPoint>
MultiSig::AggregateCommits(const vector<CommitPoint>& commitPoints)
{
    const Curve& curve = Schnorr::GetInstance().GetCurve();

    if (commitPoints.size() == 0)
    {
        LOG_GENERAL(WARNING, "Empty list of commits");
        return nullptr;
    }

    shared_ptr<CommitPoint> aggregatedCommit(
        new CommitPoint(commitPoints.at(0)));
    if (aggregatedCommit == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return nullptr;
    }

    for (unsigned int i = 1; i < commitPoints.size(); i++)
    {
        if (EC_POINT_add(curve.m_group.get(), aggregatedCommit->m_p.get(),
                         aggregatedCommit->m_p.get(),
                         commitPoints.at(i).m_p.get(), NULL)
            == 0)
        {
            LOG_GENERAL(WARNING, "Commit aggregation failed");
            return nullptr;
        }
    }

    return aggregatedCommit;
}

shared_ptr<Response>
MultiSig::AggregateResponses(const vector<Response>& responses)
{
    const Curve& curve = Schnorr::GetInstance().GetCurve();

    if (responses.size() == 0)
    {
        LOG_GENERAL(WARNING, "Empty list of responses");
        return nullptr;
    }

    shared_ptr<Response> aggregatedResponse(new Response(responses.at(0)));
    if (aggregatedResponse == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return nullptr;
    }

    unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
    if (ctx == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return nullptr;
    }

    for (unsigned int i = 1; i < responses.size(); i++)
    {
        if (BN_mod_add(aggregatedResponse->m_r.get(),
                       aggregatedResponse->m_r.get(), responses.at(i).m_r.get(),
                       curve.m_order.get(), ctx.get())
            == 0)
        {
            LOG_GENERAL(WARNING, "Response aggregation failed");
            return nullptr;
        }
    }

    return aggregatedResponse;
}

shared_ptr<Signature>
MultiSig::AggregateSign(const Challenge& challenge,
                        const Response& aggregatedResponse)
{
    if (!challenge.Initialized())
    {
        LOG_GENERAL(WARNING, "Challenge not initialized");
        return nullptr;
    }

    if (!aggregatedResponse.Initialized())
    {
        LOG_GENERAL(WARNING, "Response not initialized");
        return nullptr;
    }

    shared_ptr<Signature> result(new Signature());
    if (result == nullptr)
    {
        LOG_GENERAL(WARNING, "Memory allocation failure");
        // throw exception();
        return nullptr;
    }

    if (BN_copy(result->m_r.get(), challenge.m_c.get()) == NULL)
    {
        LOG_GENERAL(WARNING, "Signature generation (copy challenge) failed");
        return nullptr;
    }

    if (BN_copy(result->m_s.get(), aggregatedResponse.m_r.get()) == NULL)
    {
        LOG_GENERAL(WARNING, "Signature generation (copy response) failed");
        return nullptr;
    }

    return result;
}

// bool MultiSig::SignResponse(const CommitSecret & commitSecret, const Challenge & challenge, const PrivKey & privkey, Signature & result)
// {
//     LOG_MARKER();

//     // Initial checks

//     if (!commitSecret.Initialized())
//     {
//         LOG_GENERAL(WARNING, "Commit secret not initialized");
//         return false;
//     }

//     if (!challenge.Initialized())
//     {
//         LOG_GENERAL(WARNING, "Challenge not initialized");
//         return false;
//     }

//     if (!privkey.Initialized())
//     {
//         LOG_GENERAL(WARNING, "Privkey key not initialized");
//         return false;
//     }

//     if (!result.Initialized())
//     {
//         LOG_GENERAL(WARNING, "Signature not initialized");
//         return false;
//     }

//     // The algorithm takes the following steps:
//     // Compute s = k - r*kpriv mod(order)
//     // Signature on m is (r, s)

//     bool err = false; // detect error

//     unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

//     if (ctx != nullptr)
//     {

//         err = false;

//         err = BN_copy(result.m_r.get(), challenge.m_c.get());
//         if (err)
//         {
//             LOG_GENERAL(WARNING, "Challenge copy failed");
//             return false;
//         }

//         // Compute s = k - r*krpiv
//         // r*kpriv
//         err = (BN_mod_mul(result.m_s.get(), challenge.m_c.get(), privkey.m_d.get(), m_curve.m_order.get(), ctx.get()) == 0);
//         if (err)
//         {
//             LOG_GENERAL(WARNING, "Response mod mul failed");
//             return false;
//         }

//         // k-r*kpriv
//         err = (BN_mod_sub(result.m_s.get(), commitSecret.m_s.get(), result.m_s.get(), m_curve.m_order.get(), ctx.get()) == 0);
//         if (err)
//         {
//             LOG_GENERAL(WARNING, "BIGNUM mod sub failed");
//             return false;
//         }

//     }
//     else
//     {
//         LOG_GENERAL(WARNING, "Memory allocation failure");
//         throw exception();
//     }

//     return true;
// }

bool MultiSig::VerifyResponse(const Response& response,
                              const Challenge& challenge, const PubKey& pubkey,
                              const CommitPoint& commitPoint)
{
    LOG_MARKER();

    try
    {
        // Initial checks

        if (!response.Initialized())
        {
            LOG_GENERAL(WARNING, "Response not initialized");
            return false;
        }

        if (!challenge.Initialized())
        {
            LOG_GENERAL(WARNING, "Challenge not initialized");
            return false;
        }

        if (!pubkey.Initialized())
        {
            LOG_GENERAL(WARNING, "Public key not initialized");
            return false;
        }

        if (!commitPoint.Initialized())
        {
            LOG_GENERAL(WARNING, "Commit point not initialized");
            return false;
        }

        const Curve& curve = Schnorr::GetInstance().GetCurve();

        // The algorithm to check whether the commit point generated from its resopnse is the same one received in the commit phase
        // Check if s is in [1, ..., order-1]
        // Compute Q = sG + r*kpub
        // return Q == commitPoint

        bool err = false;

        // Regenerate the commitmment part of the signature
        unique_ptr<EC_POINT, void (*)(EC_POINT*)> Q(
            EC_POINT_new(curve.m_group.get()), EC_POINT_clear_free);
        unique_ptr<BN_CTX, void (*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);

        if ((ctx != nullptr) && (Q != nullptr))
        {
            // 1. Check if s is in [1, ..., order-1]
            err = (BN_is_zero(response.m_r.get())
                   || (BN_cmp(response.m_r.get(), curve.m_order.get()) != -1));
            if (err)
            {
                LOG_GENERAL(WARNING, "Response not in range");
                return false;
            }

            // 2. Compute Q = sG + r*kpub
            err = (EC_POINT_mul(curve.m_group.get(), Q.get(),
                                response.m_r.get(), pubkey.m_P.get(),
                                challenge.m_c.get(), ctx.get())
                   == 0);
            if (err)
            {
                LOG_GENERAL(WARNING, "Commit regenerate failed");
                return false;
            }

            // 3. Q == commitPoint
            err = (EC_POINT_cmp(curve.m_group.get(), Q.get(),
                                commitPoint.m_p.get(), ctx.get())
                   != 0);
            if (err)
            {
                LOG_GENERAL(WARNING,
                            "Generated commit point doesn't match the "
                            "given one");
                return false;
            }
        }
        else
        {
            LOG_GENERAL(WARNING, "Memory allocation failure");
            // throw exception();
            return false;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with MultiSig::VerifyResponse." << ' ' << e.what());
        return false;
    }
    return true;
}

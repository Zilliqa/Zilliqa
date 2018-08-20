#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "schnorr.h"
#include "libCrypto/Schnorr.h"

// OCaml CTypes does not support handling exceptions. So just abort.
void err_abort(const char* msg)
{
    fprintf(stderr, "%s\n", msg);
    abort();
}

extern "C" {

// Generate a private/public key pair.
// Memory must already be allocated by caller.
void genKeyPair_Z(RawBytes_Z* privKey, RawBytes_Z* pubKey)
{
    // Get key pair from C++ lib.
    Schnorr& s = Schnorr::GetInstance();
    std::pair<PrivKey, PubKey> kpair = s.GenKeyPair();

    // Make sure we can pass on the result back.
    std::vector<unsigned char> privK, pubK;
    int privKSize = kpair.first.Serialize(privK, 0);
    int pubKSize = kpair.second.Serialize(pubK, 0);
    assert(privKSize == privK.size() && pubKSize == pubK.size()
           && "Output size of generate key mismatches reported size");
    if (privKey->len != privKSize)
        err_abort("Schnorr::genKeyPair_Z: Incorrect memory allocated for "
                  "private key");
    if (pubKey->len != pubKSize)
        err_abort(
            "Schnorr::genKeyPair_Z: Incorrect memory allocated for public key");

    // Pass on the result.
    std::memcpy(privKey->data, privK.data(), privKSize);
    std::memcpy(pubKey->data, pubK.data(), pubKSize);
}

// Sign message with privKey/pubKey. Memory for signature must be allocated by caller.
void sign_Z(const RawBytes_Z* privKey, const RawBytes_Z* pubKey,
            const RawBytes_Z* message, RawBytes_Z* signature)
{
    std::vector<unsigned char> privK(privKey->len), pubK(pubKey->len),
        M(message->len), S;

    if (privKey->len != privkey_len)
        err_abort("Schnorr::sign_Z: Incorrect memory allocated for "
                  "private key");
    if (pubKey->len != pubkey_len)
        err_abort("Schnorr::sign_Z: Incorrect memory allocated for public key");

    // Copy inputs to vectors for use by Schnorr.
    std::memcpy(privK.data(), privKey->data, privKey->len);
    std::memcpy(pubK.data(), pubKey->data, pubKey->len);
    std::memcpy(M.data(), message->data, message->len);

    Schnorr& s = Schnorr::GetInstance();
    PrivKey keyPriv(privK, 0);
    PubKey keyPub(pubK, 0);
    Signature sig;

    // Sign the message.
    s.Sign(M, keyPriv, keyPub, sig);
    // Extract signature into byte array.
    sig.Serialize(S, 0);

    if (S.size() != (unsigned)signature->len)
        err_abort("Schnorr::size_Z: Incorrect memory allocated for signature");

    // Copy the results for use by caller.
    std::memcpy(signature->data, S.data(), S.size());
}

// Verify message with signature and public key of signer
int verify_Z(const RawBytes_Z* pubKey, const RawBytes_Z* message,
             RawBytes_Z* signature)
{
    std::vector<unsigned char> pubK(pubKey->len), M(message->len),
        S(signature->len);

    if (pubKey->len != pubkey_len)
        err_abort(
            "Schnorr::verify_Z: Incorrect memory allocated for public key");
    if (signature->len != signature_len)
        err_abort(
            "Schnorr::verify_Z: Incorrect memory allocated for signature");

    // Copy inputs to vectors for use by Schnorr.
    std::memcpy(pubK.data(), pubKey->data, pubKey->len);
    std::memcpy(M.data(), message->data, message->len);
    std::memcpy(S.data(), signature->data, signature->len);

    Schnorr& s = Schnorr::GetInstance();
    PubKey keyPub(pubK, 0);
    Signature sig(S, 0);

    // Sign the message.
    if (s.Verify(M, sig, keyPub))
        return 1;
    else
        return 0;
}
}

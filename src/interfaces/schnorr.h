#ifndef SCHNORR_H
#define SCHNORR_H

#ifdef __cplusplus
extern "C" {
#endif

#define privkey_len 32
#define pubkey_len 33
#define signature_len 64

typedef struct
{
    char* data;
    int len;
} RawBytes_Z;

// Generate a private/public key pair.
// Memory must already be allocated by caller.
void genKeyPair_Z(RawBytes_Z* privKey, RawBytes_Z* pubKey);

// Sign message with privKey/pubKey. Memory for signature must be allocated by caller.
void sign_Z(const RawBytes_Z* privKey, const RawBytes_Z* pubKey,
            const RawBytes_Z* message, RawBytes_Z* signature);

// Verify message with signature and public key of signer
int verify_Z(const RawBytes_Z* pubKey, const RawBytes_Z* message,
             RawBytes_Z* signature);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SCHNORR_H

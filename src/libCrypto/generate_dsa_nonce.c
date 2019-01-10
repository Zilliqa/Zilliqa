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

/*
 * Copyright 1995-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <memory.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ossl_typ.h>
#include "generate_dsa_nonce.h"

/*
 * ported from
 * https://github.com/openssl/openssl/blob/7835e97b6ff5cd94a10c5aeac439f4aa145a77b2/crypto/bn/bn_rand.c#L205
 *
 */

/*
 * BN_generate_dsa_nonce generates a random number 0 <= out < range. Unlike
 * BN_rand_range, it also includes the contents of |priv| and |message| in
 * the generation so that an RNG failure isn't fatal as long as |priv|
 * remains secret. This is intended for use in DSA and ECDSA where an RNG
 * weakness leads directly to private key exposure unless this function is
 * used.
 */
#ifdef __cplusplus
extern "C" {
#endif

int BN_generate_dsa_nonce(BIGNUM *out, const BIGNUM *range,
                          const BIGNUM *priv, const unsigned char *message,
                          size_t message_len, BN_CTX *ctx)
{
    SHA512_CTX sha;
    /*
     * We use 512 bits of random data per iteration to ensure that we have at
     * least |range| bits of randomness.
     */
    unsigned char random_bytes[64];
    unsigned char digest[SHA512_DIGEST_LENGTH];
    unsigned done, todo;
    /* We generate |range|+8 bytes of random output. */
    const unsigned num_k_bytes = BN_num_bytes(range) + 8;
    unsigned char private_bytes[96];
    unsigned char *k_bytes;
    int ret = 0;

    k_bytes = (unsigned char *)OPENSSL_malloc(num_k_bytes);
    if (k_bytes == NULL)
        goto err;

    /* We copy |priv| into a local buffer to avoid exposing its length. */
    todo = sizeof(priv->d[0]) * priv->top;
    if (todo > sizeof(private_bytes)) {
        /*
         * No reasonable DSA or ECDSA key should have a private key this
         * large and we don't handle this case in order to avoid leaking the
         * length of the private key.
         */
        //FIXME: error reporting disabled
        /* BNerr(BN_F_BN_GENERATE_DSA_NONCE, BN_R_PRIVATE_KEY_TOO_LARGE); */
        goto err;
    }
    memcpy(private_bytes, priv->d, todo);
    memset(private_bytes + todo, 0, sizeof(private_bytes) - todo);

    for (done = 0; done < num_k_bytes;) {
        //FIXME: RAND_priv_bytes replaced with RAND_bytes(), see the difference:
        // https://www.openssl.org/docs/man1.1.1/man3/RAND_priv_bytes.html
        if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1)
            goto err;
        SHA512_Init(&sha);
        SHA512_Update(&sha, &done, sizeof(done));
        SHA512_Update(&sha, private_bytes, sizeof(private_bytes));
        SHA512_Update(&sha, message, message_len);
        SHA512_Update(&sha, random_bytes, sizeof(random_bytes));
        SHA512_Final(digest, &sha);

        todo = num_k_bytes - done;
        if (todo > SHA512_DIGEST_LENGTH)
            todo = SHA512_DIGEST_LENGTH;
        memcpy(k_bytes + done, digest, todo);
        done += todo;
    }

    if (!BN_bin2bn(k_bytes, num_k_bytes, out))
        goto err;
    if (BN_mod(out, out, range, ctx) != 1)
        goto err;
    ret = 1;

 err:
    OPENSSL_free(k_bytes);
    OPENSSL_cleanse(private_bytes, sizeof(private_bytes));
    return ret;
}
#ifdef __cplusplus
}
#endif

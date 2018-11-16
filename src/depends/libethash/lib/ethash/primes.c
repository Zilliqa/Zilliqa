/* ethash: C/C++ implementation of Ethash, the Ethereum Proof of Work algorithm.
 * Copyright 2018 Pawel Bylica.
 * Licensed under the Apache License, Version 2.0. See the LICENSE file.
 */

#include "primes.h"

static int is_prime(int number)
{
    int d;

    if (number <= 1)
        return 0;

    if (number % 2 == 0 && number > 2)
        return 0;

    /* Check factors up to sqrt(number).
       To avoid computing sqrt, compare d*d <= number with 64-bit precision. */
    for (d = 3; (int64_t)d * (int64_t)d <= (int64_t)number; d += 2)
    {
        if (number % d == 0)
            return 0;
    }

    return 1;
}

int ethash_find_largest_prime(int upper_bound)
{
    int n = upper_bound;

    if (n < 2)
        return 0;

    if (n == 2)
        return 2;

    /* If even number, skip it. */
    if (n % 2 == 0)
        --n;

    /* Test descending odd numbers. */
    while (!is_prime(n))
        n -= 2;

    return n;
}

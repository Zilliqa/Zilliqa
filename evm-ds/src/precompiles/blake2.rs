use std::borrow::Cow;

use evm::executor::stack::{PrecompileFailure, PrecompileOutput};
use evm::{Context, ExitError, ExitSucceed};

/// Blake2 constants.
mod consts {
    pub(super) const INPUT_LENGTH: usize = 213;

    /// The precomputed SIGMA.
    ///
    /// See [RFC 7693](https://datatracker.ietf.org/doc/html/rfc7693#section-2.7) specification for more details.
    pub(super) const SIGMA: [[usize; 16]; 10] = [
        [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
        [14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3],
        [11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4],
        [7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8],
        [9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13],
        [2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9],
        [12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11],
        [13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10],
        [6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5],
        [10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0],
    ];

    /// The initialization vector.
    ///
    /// See [RFC 7693](https://tools.ietf.org/html/rfc7693#section-2.6) specification for more details.
    pub(super) const IV: [u64; 8] = [
        0x6a09e667f3bcc908,
        0xbb67ae8584caa73b,
        0x3c6ef372fe94f82b,
        0xa54ff53a5f1d36f1,
        0x510e527fade682d1,
        0x9b05688c2b3e6c1f,
        0x1f83d9abfb41bd6b,
        0x5be0cd19137e2179,
    ];

    // G rotation constants.

    /// G rotation 1.
    pub(super) const R1: u32 = 32;

    /// G rotation 2.
    pub(super) const R2: u32 = 24;

    /// G rotation 3.
    pub(super) const R3: u32 = 16;

    /// G rotation 4.
    pub(super) const R4: u32 = 63;
}

/// The G primitive function which mixes two input worlds, "x" and "y", into
/// four words indexed by "a", "b", "c", and "d" in the working vector v[0..15].
///
/// See [RFC 7693](https://datatracker.ietf.org/doc/html/rfc7693#section-3.1) specification for more
/// details.
fn g(v: &mut [u64], a: usize, b: usize, c: usize, d: usize, x: u64, y: u64) {
    v[a] = v[a].wrapping_add(v[b]).wrapping_add(x);
    v[d] = (v[d] ^ v[a]).rotate_right(consts::R1);
    v[c] = v[c].wrapping_add(v[d]);
    v[b] = (v[b] ^ v[c]).rotate_right(consts::R2);
    v[a] = v[a].wrapping_add(v[b]).wrapping_add(y);
    v[d] = (v[d] ^ v[a]).rotate_right(consts::R3);
    v[c] = v[c].wrapping_add(v[d]);
    v[b] = (v[b] ^ v[c]).rotate_right(consts::R4);
}

/// Takes as an argument the state vector `h`, message block vector `m` (the last block is padded
/// with zeros to full block size, if required), 2w-bit offset counter `t`, and final block
/// indicator flag `f`. Local vector v[0..15] is used in processing. F returns a new state vector.
/// The number of rounds, `r`, is 12 for BLAKE2b and 10 for BLAKE2s. Rounds are numbered from 0 to
/// r - 1.
///
/// See [RFC 7693](https://datatracker.ietf.org/doc/html/rfc7693#section-3.2) specification for more
/// details.
fn f(mut h: [u64; 8], m: [u64; 16], t: [u64; 2], f: bool, rounds: u32) -> Vec<u8> {
    // Initialize the work vector.
    let mut v = [0u64; 16];
    v[0..8].copy_from_slice(&h); // First half from state.
    v[8..16].copy_from_slice(&consts::IV); // Second half from IV.

    v[12] ^= t[0]; // Low word of the offset.
    v[13] ^= t[1]; // High word.

    if f {
        // last block flag?
        v[14] = !v[14] // Invert all bits.
    }

    for i in 0..rounds {
        // Typically twelve rounds for blake2b.
        // Message word selection permutation for this round.
        let s = &consts::SIGMA[usize::try_from(i).expect("Round can convert to usize") % 10];
        g(&mut v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
        g(&mut v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
        g(&mut v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
        g(&mut v, 3, 7, 11, 15, m[s[6]], m[s[7]]);

        g(&mut v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
        g(&mut v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
        g(&mut v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
        g(&mut v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
    }

    for i in 0..8 {
        // XOR the two halves.
        h[i] ^= v[i] ^ v[i + 8];
    }

    let mut result = Vec::with_capacity(64);
    for value in h {
        result.extend_from_slice(&value.to_le_bytes());
    }

    result
}

pub(crate) fn blake2(
    input: &[u8],
    target_gas: Option<u64>,
    _context: &Context,
    _is_static: bool,
) -> std::result::Result<PrecompileOutput, PrecompileFailure> {
    if input.len() != consts::INPUT_LENGTH {
        return Err(PrecompileFailure::Error { exit_status: ExitError::Other(Cow::Borrowed("ERR_BLAKE2F_INVALID_LEN")) });
    }

    let cost = required_gas(input).unwrap();
    if let Some(target_gas) = target_gas {
        if cost > target_gas {
            return Err(PrecompileFailure::Error { exit_status: ExitError::OutOfGas });
        }
    }

    let mut rounds_bytes = [0u8; 4];
    rounds_bytes.copy_from_slice(&input[0..4]);
    let rounds = u32::from_be_bytes(rounds_bytes);

    let mut h = [0u64; 8];
    for (mut x, value) in h.iter_mut().enumerate() {
        let mut word: [u8; 8] = [0u8; 8];
        x = x * 8 + 4;
        word.copy_from_slice(&input[x..(x + 8)]);
        *value = u64::from_le_bytes(word);
    }

    let mut m = [0u64; 16];
    for (mut x, value) in m.iter_mut().enumerate() {
        let mut word: [u8; 8] = [0u8; 8];
        x = x * 8 + 68;
        word.copy_from_slice(&input[x..(x + 8)]);
        *value = u64::from_le_bytes(word);
    }

    let mut t: [u64; 2] = [0u64; 2];
    for (mut x, value) in t.iter_mut().enumerate() {
        let mut word: [u8; 8] = [0u8; 8];
        x = x * 8 + 196;
        word.copy_from_slice(&input[x..(x + 8)]);
        *value = u64::from_le_bytes(word);
    }

    if input[212] != 0 && input[212] != 1 {
        return Err(PrecompileFailure::Error { exit_status: ExitError::Other(Cow::Borrowed("ERR_BLAKE2F_FINAL_FLAG")) });
    }
    let finished = input[212] != 0;

    let output = f(h, m, t, finished, rounds);
    Ok(PrecompileOutput {
        cost,
        exit_status: ExitSucceed::Returned,
        logs: vec![],
        output
    })
}

pub(super) const F_ROUND: u64 = 1;

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let (int_bytes, _) = input.split_at(std::mem::size_of::<u32>());
    let num_rounds = u32::from_be_bytes(
        // Unwrap is fine here as it can not fail
        int_bytes.try_into().unwrap(),
    );
    Ok(num_rounds as u64 * F_ROUND)
}

// #[cfg(test)]
// mod tests {
//     use super::super::utils::new_context;
//     use crate::prelude::Vec;

//     use super::*;

//     // [4 bytes for rounds]
//     // [64 bytes for h]
//     // [128 bytes for m]
//     // [8 bytes for t_0]
//     // [8 bytes for t_1]
//     // [1 byte for f]
//     const INPUT: &str = "\
//             0000000c\
//             48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5\
//             d182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b\
//             6162630000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0300000000000000\
//             0000000000000000\
//             01";

//     fn test_blake2f_out_of_gas() -> EvmPrecompileResult {
//         let input = hex::decode(INPUT).unwrap();
//         Blake2F.run(&input, Some(EthGas::new(11)), &new_context(), false)
//     }

//     fn test_blake2f_empty() -> EvmPrecompileResult {
//         let input = [0u8; 0];
//         Blake2F.run(&input, Some(EthGas::new(0)), &new_context(), false)
//     }

//     fn test_blake2f_invalid_len_1() -> EvmPrecompileResult {
//         let input = hex::decode(
//             "\
//             00000c\
//             48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5\
//             d182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b\
//             6162630000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0300000000000000\
//             0000000000000000\
//             01",
//         )
//         .unwrap();
//         Blake2F.run(&input, Some(EthGas::new(12)), &new_context(), false)
//     }

//     fn test_blake2f_invalid_len_2() -> EvmPrecompileResult {
//         let input = hex::decode(
//             "\
//             000000000c\
//             48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5\
//             d182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b\
//             6162630000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0300000000000000\
//             0000000000000000\
//             01",
//         )
//         .unwrap();
//         Blake2F.run(&input, Some(EthGas::new(12)), &new_context(), false)
//     }

//     fn test_blake2f_invalid_flag() -> EvmPrecompileResult {
//         let input = hex::decode(
//             "\
//             0000000c\
//             48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5\
//             d182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b\
//             6162630000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0300000000000000\
//             0000000000000000\
//             02",
//         )
//         .unwrap();
//         Blake2F.run(&input, Some(EthGas::new(12)), &new_context(), false)
//     }

//     fn test_blake2f_r_0() -> Vec<u8> {
//         let input = hex::decode(
//             "\
//             00000000\
//             48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5\
//             d182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b\
//             6162630000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0300000000000000\
//             0000000000000000\
//             01",
//         )
//         .unwrap();
//         Blake2F
//             .run(&input, Some(EthGas::new(12)), &new_context(), false)
//             .unwrap()
//             .output
//     }

//     fn test_blake2f_r_12() -> Vec<u8> {
//         let input = hex::decode(INPUT).unwrap();
//         Blake2F
//             .run(&input, Some(EthGas::new(12)), &new_context(), false)
//             .unwrap()
//             .output
//     }

//     fn test_blake2f_final_block_false() -> Vec<u8> {
//         let input = hex::decode(
//             "\
//             0000000c\
//             48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5\
//             d182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b\
//             6162630000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0000000000000000000000000000000000000000000000000000000000000000\
//             0300000000000000\
//             0000000000000000\
//             00",
//         )
//         .unwrap();
//         Blake2F
//             .run(&input, Some(EthGas::new(12)), &new_context(), false)
//             .unwrap()
//             .output
//     }

//     #[test]
//     fn test_blake2f() {
//         assert!(matches!(
//             test_blake2f_out_of_gas(),
//             Err(ExitError::OutOfGas)
//         ));

//         assert!(matches!(
//             test_blake2f_empty(),
//             Err(ExitError::Other(Borrowed("ERR_BLAKE2F_INVALID_LEN")))
//         ));

//         assert!(matches!(
//             test_blake2f_invalid_len_1(),
//             Err(ExitError::Other(Borrowed("ERR_BLAKE2F_INVALID_LEN")))
//         ));

//         assert!(matches!(
//             test_blake2f_invalid_len_2(),
//             Err(ExitError::Other(Borrowed("ERR_BLAKE2F_INVALID_LEN")))
//         ));

//         assert!(matches!(
//             test_blake2f_invalid_flag(),
//             Err(ExitError::Other(Borrowed("ERR_BLAKE2F_FINAL_FLAG",)))
//         ));

//         let expected = hex::decode(
//             "08c9bcf367e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5d\
//             282e6ad7f520e511f6c3e2b8c68059b9442be0454267ce079217e1319cde05b",
//         )
//         .unwrap();
//         assert_eq!(test_blake2f_r_0(), expected);

//         let expected = hex::decode(
//             "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1\
//                 7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923",
//         )
//         .unwrap();
//         assert_eq!(test_blake2f_r_12(), expected);

//         let expected = hex::decode(
//             "75ab69d3190a562c51aef8d88f1c2775876944407270c42c9844252c26d28752\
//             98743e7f6d5ea2f2d3e8d226039cd31b4e426ac4f2d3d666a610c2116fde4735",
//         )
//         .unwrap();
//         assert_eq!(test_blake2f_final_block_false(), expected);
//     }
// }

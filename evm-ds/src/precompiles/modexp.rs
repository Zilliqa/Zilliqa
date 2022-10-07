use evm::executor::stack::{PrecompileFailure, PrecompileOutput};
use evm::{Context, ExitError, ExitSucceed};
use num_bigint::BigUint;
use num_integer::Integer;
use primitive_types::U256;
use std::borrow::Cow;

pub(crate) fn modexp(
    input: &[u8],
    target_gas: Option<u64>,
    _context: &Context,
    _is_static: bool,
) -> std::result::Result<PrecompileOutput, PrecompileFailure> {
    let cost = match required_gas(input) {
        Ok(i) => i,
        Err(err) => return Err(PrecompileFailure::Error { exit_status: err }),
    };

    if let Some(target_gas) = target_gas {
        if cost > target_gas {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::OutOfGas,
            });
        }
    }

    let result = match run_inner(input) {
        Ok(out) => Ok(PrecompileOutput {
            cost,
            exit_status: ExitSucceed::Returned,
            logs: vec![],
            output: out,
        }),
        Err(err) => Err(PrecompileFailure::Error { exit_status: err }),
    };
    result
}

fn calc_iter_count(exp_len: u64, base_len: u64, bytes: &[u8]) -> Result<U256, ExitError> {
    let start = usize::try_from(base_len)
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    let exp_len = usize::try_from(exp_len)
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    // #[allow(clippy::redundant_closure)]
    let exp = parse_bytes(
        bytes,
        start.saturating_add(96),
        core::cmp::min(32, exp_len),
        // I don't understand why I need a closure here, but doesn't compile without one
        |x| U256::from(x),
    );

    if exp_len <= 32 && exp.is_zero() {
        Ok(U256::zero())
    } else if exp_len <= 32 {
        Ok(U256::from(exp.bits()) - U256::from(1))
    } else {
        // else > 32
        Ok(U256::from(8) * U256::from(exp_len - 32) + U256::from(exp.bits()) - U256::from(1))
    }
}

fn run_inner(input: &[u8]) -> Result<Vec<u8>, ExitError> {
    let (base_len, exp_len, mod_len) = parse_lengths(input);
    let base_len = usize::try_from(base_len)
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    let exp_len = usize::try_from(exp_len)
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    let mod_len = usize::try_from(mod_len)
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;

    let base_start = 96;
    let base_end = base_len.saturating_add(base_start);

    let exp_start = base_end;
    let exp_end = exp_len.saturating_add(exp_start);

    let mod_start = exp_end;

    let base = parse_bytes(input, base_start, base_len, BigUint::from_bytes_be);
    let exponent = parse_bytes(input, exp_start, exp_len, BigUint::from_bytes_be);
    let modulus = parse_bytes(input, mod_start, mod_len, BigUint::from_bytes_be);

    let output = {
        let computed_result = if modulus == BigUint::from(0u32) {
            Vec::new()
        } else {
            base.modpow(&exponent, &modulus).to_bytes_be()
        };
        // The result must be the same length as the input modulus.
        // To ensure this we pad on the left with zeros.
        if mod_len > computed_result.len() {
            let diff = mod_len - computed_result.len();
            let mut padded_result = Vec::with_capacity(mod_len);
            padded_result.extend(core::iter::repeat(0).take(diff));
            padded_result.extend_from_slice(&computed_result);
            padded_result
        } else {
            computed_result
        }
    };

    Ok(output)
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let (base_len, exp_len, mod_len) = parse_lengths(input);

    let mul = mul_complexity(base_len, mod_len);
    let iter_count = calc_iter_count(exp_len, base_len, input)?;
    // mul * iter_count bounded by 2^189 (so no overflow)
    let gas = mul * iter_count / U256::from(3);

    Ok(core::cmp::max(200, saturating_round(gas)))
}

fn mul_complexity(base_len: u64, mod_len: u64) -> U256 {
    let max_len = core::cmp::max(mod_len, base_len);
    let words = U256::from(Integer::div_ceil(&max_len, &8));
    words * words
}

fn parse_bytes<T, F: FnOnce(&[u8]) -> T>(input: &[u8], start: usize, size: usize, f: F) -> T {
    let len = input.len();
    if start >= len {
        return f(&[]);
    }
    let end = start + size;
    if end > len {
        // Pad on the right with zeros if input is too short
        let bytes: Vec<u8> = input[start..]
            .iter()
            .copied()
            .chain(core::iter::repeat(0u8))
            .take(size)
            .collect();
        f(&bytes)
    } else {
        f(&input[start..end])
    }
}

fn saturating_round(x: U256) -> u64 {
    if x.bits() > 64 {
        u64::MAX
    } else {
        x.as_u64()
    }
}

fn parse_lengths(input: &[u8]) -> (u64, u64, u64) {
    let parse = |start: usize| -> u64 {
        // I don't understand why I need a closure here, but doesn't compile without one
        #[allow(clippy::redundant_closure)]
        saturating_round(parse_bytes(input, start, 32, |x| U256::from(x)))
    };
    let base_len = parse(0);
    let exp_len = parse(32);
    let mod_len = parse(64);

    (base_len, exp_len, mod_len)
}

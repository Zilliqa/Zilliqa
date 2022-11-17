//! Arithmetic operations on the elliptic curve 'alt_bn128'
//! References:
//! * https://eips.ethereum.org/EIPS/eip-196
//! * https://eips.ethereum.org/EIPS/eip-197
//! * https://eips.ethereum.org/EIPS/eip-1108.

use evm::{
    executor::stack::{PrecompileFailure, PrecompileOutput},
    Context, ExitError, ExitSucceed,
};
use witnet_bn::{AffineG1, AffineG2, FieldError, Fq, Fq2, Fr, Group, Gt, G1, G2};

const ADD_COST: u64 = 150;
const MUL_COST: u64 = 6000;
const PAIR_COST_BASE: u64 = 45_000;
const PAIR_COST_PER_UNIT: u64 = 34_000;

pub(crate) fn ec_add(
    input: &[u8],
    gas_limit: Option<u64>,
    _context: &Context,
    _is_static: bool,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
    if let Some(gas_limit) = gas_limit {
        if ADD_COST > gas_limit {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::OutOfGas,
            });
        }
    }

    // Resize input to the expected size, padding with zeroes.
    let mut input = input.to_vec();
    input.resize(128, 0);

    let p1 = parse_point(input[0..64].try_into().unwrap())?;
    let p2 = parse_point(input[64..128].try_into().unwrap())?;

    let sum = match AffineG1::from_jacobian(p1 + p2) {
        Some(s) => s,
        None => {
            return Err(err("sum is not representable"));
        }
    };

    // Encode output.
    let mut output = [0; 64];
    // Unwraps are infallible because the slice lengths are 32.
    sum.x().to_big_endian(&mut output[0..32]).unwrap();
    sum.y().to_big_endian(&mut output[32..64]).unwrap();

    Ok((PrecompileOutput {
        exit_status: ExitSucceed::Returned,
        output: output.to_vec(),
    }, ADD_COST))
}

pub(crate) fn ec_mul(
    input: &[u8],
    gas_limit: Option<u64>,
    _context: &Context,
    _is_static: bool,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
    if let Some(gas_limit) = gas_limit {
        if MUL_COST > gas_limit {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::OutOfGas,
            });
        }
    }

    // Resize input to the expected size, padding with zeroes.
    let mut input = input.to_vec();
    input.resize(96, 0);

    let p = parse_point(input[0..64].try_into().unwrap())?;
    let s = parse_scalar(input[64..96].try_into().unwrap());

    let result = match AffineG1::from_jacobian(p * s) {
        Some(r) => r,
        None => {
            return Err(err("result is not representable"));
        }
    };

    // Encode output.
    let mut output = [0; 64];
    // Unwraps are infallible because the slice lengths are 32.
    result.x().to_big_endian(&mut output[0..32]).unwrap();
    result.y().to_big_endian(&mut output[32..64]).unwrap();

    Ok((PrecompileOutput {
        exit_status: ExitSucceed::Returned,
        output: output.to_vec(),
    }, MUL_COST))
}

pub(crate) fn ec_pairing(
    input: &[u8],
    gas_limit: Option<u64>,
    _context: &Context,
    _is_static: bool,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
    if input.len() % 192 != 0 {
        return Err(err("invalid input"));
    }

    let cost = pair_cost(input.len() / 192);

    if let Some(gas_limit) = gas_limit {
        if cost > gas_limit {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::OutOfGas,
            });
        }
    }

    let points: Result<Vec<(G1, G2)>, _> = input
        .chunks(192)
        .map(|p| {
            let a = parse_point(p[0..64].try_into().unwrap())?;
            let b = parse_point_g2(p[64..192].try_into().unwrap())?;

            Ok::<(G1, G2), PrecompileFailure>((a, b))
        })
        .collect();
    let points = points?;

    let pairing = witnet_bn::pairing_batch(&points);

    // Encode output.
    let mut output = [0; 32];
    if pairing == Gt::one() {
        output[31] = 1;
    }
    // Otherwise, return 0.

    Ok((PrecompileOutput {
        exit_status: ExitSucceed::Returned,
        output: output.to_vec(),
    }, cost))
}

fn pair_cost(num_points: usize) -> u64 {
    PAIR_COST_PER_UNIT * (num_points as u64) + PAIR_COST_BASE
}

fn parse_point(input: &[u8; 64]) -> Result<G1, PrecompileFailure> {
    let x = match Fq::from_slice(&input[0..32]) {
        Ok(x) => x,
        Err(FieldError::NotMember) => {
            return Err(err("x coordinate is larger than field modulus"));
        }
        Err(FieldError::InvalidSliceLength | FieldError::InvalidU512Encoding) => unreachable!(),
    };
    let y = match Fq::from_slice(&input[32..64]) {
        Ok(y) => y,
        Err(FieldError::NotMember) => {
            return Err(err("y coordinate is larger than field modulus"));
        }
        Err(FieldError::InvalidSliceLength | FieldError::InvalidU512Encoding) => unreachable!(),
    };

    // Special case for point at infinity.
    if x.is_zero() && y.is_zero() {
        return Ok(G1::zero());
    }

    let point = if let Ok(p) = AffineG1::new(x, y) {
        p
    } else {
        return Err(err("point not on curve or in subgroup"));
    };

    Ok(point.into())
}

fn parse_point_g2(input: &[u8; 128]) -> Result<G2, PrecompileFailure> {
    let x_b = match Fq::from_slice(&input[0..32]) {
        Ok(x) => x,
        Err(FieldError::NotMember) => {
            return Err(err("x_b coordinate is larger than field modulus"));
        }
        Err(FieldError::InvalidSliceLength | FieldError::InvalidU512Encoding) => unreachable!(),
    };
    let x_a = match Fq::from_slice(&input[32..64]) {
        Ok(y) => y,
        Err(FieldError::NotMember) => {
            return Err(err("x_a coordinate is larger than field modulus"));
        }
        Err(FieldError::InvalidSliceLength | FieldError::InvalidU512Encoding) => unreachable!(),
    };

    let y_b = match Fq::from_slice(&input[64..96]) {
        Ok(x) => x,
        Err(FieldError::NotMember) => {
            return Err(err("y_b coordinate is larger than field modulus"));
        }
        Err(FieldError::InvalidSliceLength | FieldError::InvalidU512Encoding) => unreachable!(),
    };
    let y_a = match Fq::from_slice(&input[96..128]) {
        Ok(y) => y,
        Err(FieldError::NotMember) => {
            return Err(err("y_a coordinate is larger than field modulus"));
        }
        Err(FieldError::InvalidSliceLength | FieldError::InvalidU512Encoding) => unreachable!(),
    };

    let x = Fq2::new(x_a, x_b);
    let y = Fq2::new(y_a, y_b);

    // Special case for point at infinity.
    if x.is_zero() && y.is_zero() {
        return Ok(G2::zero());
    }

    let point = if let Ok(p) = AffineG2::new(x, y) {
        p
    } else {
        return Err(err("point not on curve or in subgroup"));
    };

    Ok(point.into())
}

fn parse_scalar(input: &[u8; 32]) -> Fr {
    // Unwrap is infallible because the slice length is 32.
    Fr::from_slice(&input[0..32]).unwrap()
}

fn err(msg: &'static str) -> PrecompileFailure {
    PrecompileFailure::Error {
        exit_status: ExitError::Other(msg.into()),
    }
}

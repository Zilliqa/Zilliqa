use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;

const RIPEMD160_BASE: u64 = 600;
const RIPEMD160_PER_WORD: u64 = 120;

const RIPEMD_WORD_LEN: u64 = 32;

pub(crate) fn ripemd160(
    input: &[u8],
    target_gas: Option<u64>,
    _context: &Context,
    _is_static: bool,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
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

    let hash = internal_impl(input);
    let mut output = vec![0u8; 32];
    output[12..].copy_from_slice(&hash);

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Exit(ExitSucceed::Returned),
            output,
        },
        cost,
    ))
}

fn internal_impl(input: &[u8]) -> [u8; 20] {
    use ripemd::{Digest, Ripemd160};

    let hash = Ripemd160::digest(input);
    let mut output = [0u8; 20];
    output.copy_from_slice(&hash);
    output
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok((input_len + RIPEMD_WORD_LEN - 1) / RIPEMD_WORD_LEN * RIPEMD160_PER_WORD + RIPEMD160_BASE)
}

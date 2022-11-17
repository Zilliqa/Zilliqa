use evm::executor::stack::{PrecompileFailure, PrecompileOutput};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;

const SHA256_BASE: u64 = 60;
const SHA256_PER_WORD: u64 = 12;

const SHA256_WORD_LEN: u64 = 32;

pub(crate) fn sha2_256(
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

    use sha2::Digest;
    let output = sha2::Sha256::digest(input).to_vec();
    Ok((PrecompileOutput {
        exit_status: ExitSucceed::Returned,
        output,
    }, cost))
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok((input_len + SHA256_WORD_LEN - 1) / SHA256_WORD_LEN * SHA256_PER_WORD + SHA256_BASE)
}

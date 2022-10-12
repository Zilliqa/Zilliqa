use evm::executor::stack::{PrecompileFailure, PrecompileOutput};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;

const IDENTITY_BASE: u64 = 15;
const IDENTITY_PER_WORD: u64 = 3;

const IDENTITY_WORD_LEN: u64 = 32;

pub(crate) fn identity(
    input: &[u8],
    gas_limit: Option<u64>,
    _contex: &Context,
    _is_static: bool,
) -> std::result::Result<PrecompileOutput, PrecompileFailure> {
    let gas_needed = match required_gas(input) {
        Ok(i) => i,
        Err(err) => return Err(PrecompileFailure::Error { exit_status: err }),
    };

    if let Some(gas_limit) = gas_limit {
        if gas_limit < gas_needed {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::OutOfGas,
            });
        }
    }

    Ok(PrecompileOutput {
        cost: gas_needed,
        exit_status: ExitSucceed::Returned,
        logs: vec![],
        output: input.to_vec(),
    })
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok((input_len + IDENTITY_WORD_LEN - 1) / IDENTITY_WORD_LEN * IDENTITY_PER_WORD + IDENTITY_BASE)
}

use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;

use ethabi::decode;
use ethabi::param_type::ParamType;
use ethabi::token::Token;

const BASE_COST: u64 = 15;
const PER_BYTE_COST: u64 = 3;

pub(crate) fn scilla_call(
    input: &[u8],
    gas_limit: Option<u64>,
    _contex: &Context,
    _backend: &dyn Backend,
    _is_static: bool,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
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

    let partial_types = vec![ParamType::Address, ParamType::String];
    let partial_tokens = decode(&partial_types, input);
    let Ok(partial_tokens) = partial_tokens  else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    };

    if partial_types.len() < 2 {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    }

    let Token::Address(code_address)  = partial_tokens[0] else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    };
    let code = _backend.code(code_address);
    if code.is_empty() {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("There so code under given address")),
        });
    }

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Exit(ExitSucceed::Returned),
            output: vec![],
        },
        gas_needed,
    ))
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok(input_len * PER_BYTE_COST + BASE_COST)
}

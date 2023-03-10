use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;
use std::io::Read;
use std::str::FromStr;

use ethabi::ethereum_types::Address;
use ethabi::param_type::ParamType;
use ethabi::token::Token;
use ethabi::{decode, encode};
use hex::ToHex;
use primitive_types::H160;

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

    let (code_address, _transition) = get_contract_addr_and_transition(input)?;

    let code = _backend.code_as_json(code_address);
    if code.is_empty() {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("There so code under given address")),
        });
    }

    println!("Received code with size: {}", code.len());
    println!("Code is: {}", hex::encode(code));

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Exit(ExitSucceed::Returned),
            output: vec![],
        },
        gas_needed,
    ))
}

fn get_contract_addr_and_transition(input: &[u8]) -> Result<(Address, String), PrecompileFailure> {
    let to_enc = vec![
        Token::String("0xED577d28D7D790eE7AFBc38D5d9346530F5aC1a8".to_string()),
        Token::String("getString".to_string()),
    ];

    let partial_types = vec![ParamType::String, ParamType::String];

    let encoded = encode(&to_enc);
    let partial_decode = decode(&partial_types, &encoded);

    println!("Hex input: {}", hex::encode(input));
    println!("Hex encoded: {}", hex::encode(encoded));
    if let Ok(partial_decode) = partial_decode {
        println!("Correctly decoded");
        println!("Len of decoded: {}", partial_decode.len());
    }

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

    let (Token::String(code_address), Token::String(transition))  = (&partial_tokens[0], &partial_tokens[1]) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    };

    let Ok(code_address) = H160::from_str(&code_address) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect scilla contract address")),
        });
    };

    println!(
        "Got code addres: {:x} and tran: {}",
        code_address, transition
    );
    Ok((code_address, transition.to_owned()))
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok(input_len * PER_BYTE_COST + BASE_COST)
}

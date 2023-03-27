use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError};
use std::borrow::Cow;

use crate::precompiles::scilla_common::get_contract_addr_and_name;
use crate::precompiles::scilla_common::substitute_scilla_type_with_sol;
use ethabi::decode;
use ethabi::param_type::ParamType;
use ethabi::token::Token;
use hex::ToHex;
use serde_json::{json, Value};

const BASE_COST: u64 = 15;
const PER_BYTE_COST: u64 = 3;

pub(crate) fn scilla_call(
    input: &[u8],
    gas_limit: Option<u64>,
    _contex: &Context,
    backend: &dyn Backend,
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

    let (code_address, passed_transition_name) = get_contract_addr_and_name(input)?;

    let code = backend.code_as_json(code_address);
    if code.is_empty() {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("There so code under given address")),
        });
    }
    let Ok(code) = serde_json::from_slice::<serde_json::Value>(&code) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to parse scilla contract code")),
        });
    };

    let transitions = code["contract_info"]["transitions"].to_owned();
    let Some(transitions) = transitions.as_array() else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to get transitions array from contract json")),
        });
    };
    let mut output_json = build_result_json(input, &passed_transition_name, transitions)?;
    output_json["_address"] = Value::String(code_address.encode_hex());

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Trap,
            output: serde_json::to_vec(&output_json).unwrap(),
        },
        gas_needed,
    ))
}

fn build_result_json(
    input: &[u8],
    expected_transition: &str,
    transitions: &Vec<Value>,
) -> Result<Value, PrecompileFailure> {
    let mut solidity_args = vec![ParamType::Address, ParamType::String];
    let mut scilla_args = vec![];

    for transition in transitions {
        let name = transition["vname"].as_str().unwrap_or_default();
        if name.eq(expected_transition) {
            let Some(args) = transition["params"].as_array() else {
                return Err(PrecompileFailure::Error {
                    exit_status: ExitError::Other(Cow::Borrowed("Unable to get transition args from contract json")),
                });
            };
            for arg_obj in args {
                scilla_args.push((
                    arg_obj["vname"].as_str().unwrap_or_default(),
                    arg_obj["type"].as_str().unwrap_or_default(),
                ));
            }
            break;
        }
    }
    for scilla_arg in &scilla_args {
        let decoded_arg = substitute_scilla_type_with_sol(scilla_arg.1)?;
        solidity_args.push(decoded_arg);
    }

    let Ok(decoded_values) = decode(&solidity_args, input) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to get all arguments from precompile input")),
        });
    };

    let mut result: Value = json!({});

    result["_tag"] = Value::String(expected_transition.to_string());

    let mut result_arguments = Value::Array(vec![]);
    for (scilla_arg, solidity_value) in scilla_args.iter().zip(decoded_values.iter().skip(2)) {
        let json_arg: Value;
        match solidity_value {
            Token::Uint(solidity_uint) => {
                json_arg = json!({"vname" : scilla_arg.0, "type" : scilla_arg.1, "value": format!("{}", solidity_uint)});
            },
            Token::Address(solidity_addr) => {
                json_arg = json!({"vname" : scilla_arg.0, "type" : scilla_arg.1, "value": format!("0x{}", hex::encode(solidity_addr))});
            }
            _ => {
                json_arg = json!({"vname" : scilla_arg.0, "type" : scilla_arg.1, "value": solidity_value.to_string()});
            }
        }
        result_arguments.as_array_mut().unwrap().push(json_arg);
    }
    result["params"] = result_arguments;
    Ok(result)
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok(input_len * PER_BYTE_COST + BASE_COST)
}

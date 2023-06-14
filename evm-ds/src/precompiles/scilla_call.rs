use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError};
use std::borrow::Cow;

use crate::precompiles::scilla_common::parse_invocation_prefix;
use crate::precompiles::scilla_common::substitute_scilla_type_with_sol;
use ethabi::decode;
use ethabi::ethereum_types::Address;
use ethabi::param_type::ParamType;
use ethabi::token::Token;
use hex::ToHex;
use primitive_types::U256;
use serde_json::{json, Value};

// TODO: revisit these consts
const BASE_COST: u64 = 15;
const PER_BYTE_COST: u64 = 3;

pub(crate) fn scilla_call_2(
    input: &[u8],
    gas_limit: Option<u64>,
    _contex: &Context,
    backend: &dyn Backend,
    _is_static: bool,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
    let gas_needed = check_gas(input, gas_limit)?;
    let (code_address, passed_transition_name, keep_origin) = parse_invocation_prefix(input)?;

    if keep_origin != U256::from(0) && keep_origin != U256::from(1) {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("call mode should be either 0 or 1")),
        });
    };
    let keep_origin = keep_origin == U256::from(1);
    scilla_call_common(
        input,
        backend,
        code_address,
        &passed_transition_name,
        keep_origin,
        gas_needed,
    )
}

fn check_gas(input: &[u8], gas_limit: Option<u64>) -> Result<u64, PrecompileFailure> {
    let gas_needed = match required_gas(input) {
        Ok(i) => i,
        Err(err) => return Err(PrecompileFailure::Error { exit_status: err }),
    };

    if let Some(gas_limit) = gas_limit {
        if gas_limit < gas_needed {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::Other(Cow::Borrowed(
                    "Unable to parse scilla contract code",
                )),
            });
        }
    }
    Ok(gas_needed)
}

// input should be formed of: scilla_contract_addr, transition_name, keep_origin, arg1, arg2, arg3, ..., argn
fn scilla_call_common(
    input: &[u8],
    backend: &dyn Backend,
    code_address: Address,
    passed_transition_name: &str,
    keep_origin: bool,
    gas_needed: u64,
) -> Result<(PrecompileOutput, u64), PrecompileFailure> {
    let code = backend.code_as_json(code_address);
    // If there's no code under given address it doesn't make sense to proceed further - transition call will fail at scilla level later
    if code.is_empty() {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("There's no code under given address")),
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
    let mut output_json = build_result_json(input, passed_transition_name, transitions)?;
    output_json["_address"] = Value::String(code_address.encode_hex());
    output_json["keep_origin"] = Value::Bool(keep_origin);

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
    let mut solidity_args = vec![ParamType::Address, ParamType::String, ParamType::Bool];
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
    for (scilla_arg, solidity_value) in scilla_args.iter().zip(decoded_values.iter().skip(3)) {
        let json_arg: Value = match solidity_value {
            Token::Uint(solidity_uint) => {
                json!({"vname" : scilla_arg.0, "type" : scilla_arg.1, "value": format!("{}", solidity_uint)})
            }
            Token::Address(solidity_addr) => {
                json!({"vname" : scilla_arg.0, "type" : scilla_arg.1, "value": format!("0x{}", hex::encode(solidity_addr))})
            }
            _ => {
                json!({"vname" : scilla_arg.0, "type" : scilla_arg.1, "value": solidity_value.to_string()})
            }
        };
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

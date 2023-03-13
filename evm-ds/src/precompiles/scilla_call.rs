use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;
use std::collections::BTreeMap;
use std::str::FromStr;

use ethabi::decode;
use ethabi::ethereum_types::Address;
use ethabi::param_type::ParamType;
use ethabi::token::Token;
use hex::ToHex;
use primitive_types::H160;
use protobuf::text_format::lexer::ParserLanguage::Json;
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

    let (code_address, passed_transition_name) = get_contract_addr_and_transition(input)?;

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
    let mut output_json = build_result_Json(&input, &passed_transition_name, &transitions)?;
    output_json["_address"] = Value::String(code_address.encode_hex());

    println!("Final json: {:}", output_json);
    //println!("Code is: {}", hex::encode(code));

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Trap,
            output: serde_json::to_vec(&output_json).unwrap(),
        },
        gas_needed,
    ))
}

fn build_result_Json(
    input: &[u8],
    expected_transition: &str,
    transitions: &Vec<serde_json::Value>,
) -> Result<serde_json::Value, PrecompileFailure> {
    let mut solidity_args = vec![ParamType::String, ParamType::String];
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
        let decoded_arg = substitute_scilla_arg(&scilla_arg.0)?;
        solidity_args.push(decoded_arg);
    }

    let Ok(decoded_values) = ethabi::decode(&solidity_args, &input) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to get all arguments from precompile input")),
        });
    };

    let mut result: Value = json!({});

    result["_tag"] = Value::String(expected_transition.to_string());

    let mut result_arguments = Value::Array(vec![]);
    for (scilla_arg, solidity_value) in scilla_args.iter().zip(decoded_values.iter().skip(2)) {
        let json_arg = json!({"vname" : scilla_arg.1, "type" : scilla_arg.0, "value": solidity_value.to_string()});
        result_arguments.as_array_mut().unwrap().push(json_arg);
    }
    result["params"] = result_arguments;
    Ok(result)
}

fn substitute_scilla_arg(arg_name: &str) -> Result<ParamType, PrecompileFailure> {
    let mapping = BTreeMap::from([
        ("Int32", ParamType::Int(32)),
        ("Int64", ParamType::Int(64)),
        ("Int128", ParamType::Int(128)),
        ("Int256", ParamType::Int(256)),
        ("Uint32", ParamType::Uint(32)),
        ("Uint64", ParamType::Uint(64)),
        ("Uint128", ParamType::Uint(128)),
        ("Uint256", ParamType::Uint(256)),
        ("String", ParamType::String),
        ("ByStr", ParamType::String),
        ("ByStr20", ParamType::String),
        ("ByStr32", ParamType::String),
    ]);
    let Some(val) = mapping.get(arg_name) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unknown argument type provided")),
        });
    };
    Ok(val.to_owned())
}

fn get_contract_addr_and_transition(input: &[u8]) -> Result<(Address, String), PrecompileFailure> {
    let partial_types = vec![ParamType::String, ParamType::String];
    println!("Hex input: {}", hex::encode(input));
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

    let (Token::String(code_address), Token::String(transition)) = (&partial_tokens[0], &partial_tokens[1]) else {
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

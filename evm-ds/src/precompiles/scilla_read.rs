use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;

use crate::precompiles::scilla_common::{
    get_contract_addr_and_name, substitute_scilla_type_with_sol,
};
use ethabi::decode;
use ethabi::param_type::ParamType;
use ethabi::token::Token;
use serde_json::Value;

const BASE_COST: u64 = 15;
const PER_BYTE_COST: u64 = 3;

struct ScillaField {
    indices: Vec<String>,
    ret_type: String,
}

pub(crate) fn scilla_read(
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

    let (code_address, passed_field_name) = get_contract_addr_and_name(input)?;

    let code = backend.code_as_json(code_address);
    if code.is_empty() {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("There so code under given address")),
        });
    }
    let Ok(code) = serde_json::from_slice::<Value>(&code) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to parse scilla contract code")),
        });
    };

    let fields = code["contract_info"]["fields"].to_owned();
    let field_type = get_field_type_by_name(&passed_field_name, &fields)?;
    let scilla_field = decode_indices(input, &field_type)?;

    let read_result =
        backend.susbtate_as_json(code_address, &passed_field_name, &scilla_field.indices);

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Exit(ExitSucceed::Returned),
            output: read_result.to_vec(),
        },
        gas_needed,
    ))
}

fn get_field_type_by_name(field_name: &str, fields: &Value) -> Result<String, PrecompileFailure> {
    let Some(fields_array) = fields.as_array() else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Fields in contract json are not declared as array")),
        });
    };

    for json_field in fields_array {
        let Some(vname) = json_field["vname"].as_str() else {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::Other(Cow::Borrowed("Fields in contract json are not declared as array")),
            });
        };
        if vname.eq(field_name) && json_field["type"].is_string() {
            return Ok(json_field["type"].as_str().unwrap().to_string());
        }
    }
    Err(PrecompileFailure::Error {
        exit_status: ExitError::Other(Cow::Borrowed(
            "Unable to find field with given name in fields list",
        )),
    })
}

fn decode_indices(input: &[u8], input_field_type: &str) -> Result<ScillaField, PrecompileFailure> {
    let input_field_type = input_field_type.replace(['(', ')'], "");

    let chunks = input_field_type.split_whitespace().collect::<Vec<_>>();
    if chunks.len() == 1 {
        return Ok(ScillaField {
            indices: vec![],
            ret_type: chunks.last().unwrap().to_string(),
        });
    }
    // We support only maps and maps of maps
    let mut idx = 0;
    let mut field_types = vec![ParamType::Address, ParamType::String];
    while idx < chunks.len() {
        if idx == chunks.len() - 1 {
            break;
        }
        let chunk = chunks[idx];
        if chunk.eq("Map") {
            if idx + 2 >= chunks.len() {
                // Malformatted json
                return Err(PrecompileFailure::Error {
                    exit_status: ExitError::Other(Cow::Borrowed(
                        "Unable to find field with given name in fields list",
                    )),
                });
            }
            field_types.push(substitute_scilla_type_with_sol(chunks[idx + 1])?);
            idx += 2;
        } else {
            field_types.push(substitute_scilla_type_with_sol(chunk)?);
            idx += 1;
        }
    }

    let Ok(decoded_array) = decode(&field_types, input) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to decode input by specified field definition")),
        });
    };

    let mut indices = vec![];
    for value in decoded_array.iter().skip(2) {
        if let Token::Uint(solidity_uint) = value {
            // Scilla doesn't like hex strings
            indices.push(format!("{}", solidity_uint));
        } else {
            indices.push(value.to_string());
        }
    }
    Ok(ScillaField {
        indices,
        ret_type: chunks.last().unwrap().to_string(),
    })
}

fn encode_result_type<T>(_arg_type_name: &str, _value: T) -> Option<bool> {
    None
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok(input_len * PER_BYTE_COST + BASE_COST)
}

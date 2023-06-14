use evm::backend::Backend;
use evm::executor::stack::{PrecompileFailure, PrecompileOutput, PrecompileOutputType};
use evm::{Context, ExitError, ExitSucceed};
use std::borrow::Cow;

use crate::precompiles::scilla_common::{
    get_contract_addr_and_name, substitute_scilla_type_with_sol,
};
use ethabi::param_type::ParamType;
use ethabi::token::Token;
use ethabi::{decode, encode, Address, Bytes, Uint};
use serde_json::Value;

// TODO: revisit these consts
const BASE_COST: u64 = 15;
const PER_BYTE_COST: u64 = 3;

struct ScillaField {
    indices: Vec<String>,
    ret_type: String,
}
// input should be formed of: scilla_contract_addr, field_name, key1, key2, ..., keyn
pub(crate) fn scilla_read(
    input: &[u8],
    gas_limit: Option<u64>,
    _context: &Context,
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
            exit_status: ExitError::Other(Cow::Borrowed("There no code under given address")),
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

    let substate_json =
        backend.substate_as_json(code_address, &passed_field_name, &scilla_field.indices);

    let Ok(substate_json) = serde_json::from_slice::<Value>(&substate_json) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to parse substate json")),
        });
    };

    let Ok(result) = extract_substate_from_json(&substate_json, &passed_field_name, &scilla_field) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unable to extract return value")),
        });
    };

    let encoded = match result {
        Some(token) => encode(&[token]),
        _ => Vec::default(),
    };

    Ok((
        PrecompileOutput {
            output_type: PrecompileOutputType::Exit(ExitSucceed::Returned),
            output: encoded,
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
    let input_field_type = input_field_type
        .replace(['(', ')'], "")
        .replace("Option", "");

    // We expect the input to be of the form: [contract_addr, variable_name, idx1, idx2, ..., idxn]
    // Return value is of type of the last element on the list above
    // Iterating over maps requires advancing by two since we don't care about map values expect the last one

    let chunks = input_field_type.split_whitespace().collect::<Vec<_>>();
    if chunks.len() == 1 {
        return Ok(ScillaField {
            indices: vec![],
            ret_type: chunks[0].to_string(),
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
        match value {
            Token::Uint(solidity_uint) => {
                indices.push(format!("{}", solidity_uint));
            }
            Token::Address(solidity_addr) => {
                indices.push(format!("0x{}", hex::encode(solidity_addr)));
            }
            _ => {
                indices.push(value.to_string());
            }
        }
    }
    Ok(ScillaField {
        indices,
        ret_type: chunks.last().unwrap().to_string(),
    })
}

fn extract_substate_from_json(
    value: &Value,
    vname: &str,
    def: &ScillaField,
) -> Result<Option<Token>, PrecompileFailure> {
    if !value.is_object() || value.get(vname).is_none() {
        return Ok(None);
    }
    if def.indices.is_empty() {
        return encode_result_type(&value[vname], def);
    }
    let mut value = value[vname].as_object().unwrap();

    for index in def.indices.clone().iter().take(def.indices.len() - 1) {
        if !value[index].is_object() {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::Other(Cow::Borrowed(
                    "Scilla field definition doesn't match with returned value",
                )),
            });
        }
        value = value[index].as_object().unwrap();
    }

    encode_result_type(&value[def.indices.last().unwrap()], def)
}

fn encode_result_type(
    value: &Value,
    def: &ScillaField,
) -> Result<Option<Token>, PrecompileFailure> {
    if value.is_null() || !value.is_string() {
        return Ok(None);
    }
    let value = value.as_str().unwrap();

    if def.ret_type.starts_with("Uint") {
        return Ok(Some(Token::Uint(
            Uint::from_dec_str(value).unwrap_or_default(),
        )));
    } else if def.ret_type.starts_with("Int") {
        return Ok(Some(Token::Int(
            Uint::from_dec_str(value).unwrap_or_default(),
        )));
    } else if def.ret_type.starts_with("String") {
        return Ok(Some(Token::String(String::from(value))));
    } else if def.ret_type.eq("ByStr20") {
        return Ok(Some(Token::Address(Address::from_slice(
            value.replace("0x", "").as_bytes(),
        ))));
    } else if def.ret_type.starts_with("By") {
        return Ok(Some(Token::Bytes(Bytes::from(value.as_bytes()))));
    }

    Ok(None)
}

fn required_gas(input: &[u8]) -> Result<u64, ExitError> {
    let input_len = u64::try_from(input.len())
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_USIZE_CONVERSION")))?;
    Ok(input_len * PER_BYTE_COST + BASE_COST)
}

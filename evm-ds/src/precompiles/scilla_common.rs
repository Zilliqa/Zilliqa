use ethabi::ethereum_types::Address;
use ethabi::{decode, ParamType, Token, Uint};
use evm::executor::stack::PrecompileFailure;
use evm::ExitError;
use std::borrow::Cow;
use std::collections::BTreeMap;

pub fn substitute_scilla_type_with_sol(arg_name: &str) -> Result<ParamType, PrecompileFailure> {
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
        ("ByStr20", ParamType::Address),
        ("ByStr32", ParamType::String),
    ]);
    let Some(val) = mapping.get(arg_name) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Unknown argument type provided")),
        });
    };
    Ok(val.to_owned())
}

pub fn get_contract_addr_and_name(input: &[u8]) -> Result<(Address, String), PrecompileFailure> {
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

    let (Token::Address(code_address), Token::String(name)) = (&partial_tokens[0], &partial_tokens[1]) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    };

    Ok((code_address.to_owned(), name.to_owned()))
}

pub fn parse_invocation_prefix(input: &[u8]) -> Result<(Address, String, Uint), PrecompileFailure> {
    let partial_types = vec![ParamType::Address, ParamType::String, ParamType::Uint(8)];
    let partial_tokens = decode(&partial_types, input);
    let Ok(partial_tokens) = partial_tokens  else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    };

    if partial_types.len() < 3 {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    }

    let (Token::Address(code_address), Token::String(name), Token::Uint(preserve_sender)) = (&partial_tokens[0], &partial_tokens[1], &partial_tokens[2]) else {
        return Err(PrecompileFailure::Error {
            exit_status: ExitError::Other(Cow::Borrowed("Incorrect input")),
        });
    };

    Ok((
        code_address.to_owned(),
        name.to_owned(),
        preserve_sender.to_owned(),
    ))
}

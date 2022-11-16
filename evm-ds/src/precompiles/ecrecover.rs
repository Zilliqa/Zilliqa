use evm::executor::stack::{PrecompileFailure, PrecompileOutput};
use evm::{Context, ExitError, ExitSucceed};
use primitive_types::{H160, H256};
use std::borrow::Cow;

const ECRECOVER_BASE: u64 = 3_000;
const INPUT_LEN: usize = 128;

type Address = H160;

pub(crate) fn ecrecover(
    input: &[u8],
    gas_limit: Option<u64>,
    _contex: &Context,
    _is_static: bool,
) -> std::result::Result<PrecompileOutput, PrecompileFailure> {
    let cost = ECRECOVER_BASE;
    if let Some(gas_limit) = gas_limit {
        if cost > gas_limit {
            return Err(PrecompileFailure::Error {
                exit_status: ExitError::OutOfGas,
            });
        }
    }

    let mut input = input.to_vec();
    input.resize(INPUT_LEN, 0);

    let mut hash = [0; 32];
    hash.copy_from_slice(&input[0..32]);

    let mut v = [0; 32];
    v.copy_from_slice(&input[32..64]);

    let mut signature = [0; 65]; // signature is (r, s, v), typed (uint256, uint256, uint8)
    signature[0..32].copy_from_slice(&input[64..96]); // r
    signature[32..64].copy_from_slice(&input[96..128]); // s

    let v_bit = match v[31] {
        27 | 28 if v[..31] == [0; 31] => v[31] - 27,
        _ => {
            return Ok(PrecompileOutput {
                exit_status: ExitSucceed::Returned,
                output: vec![],
            })
        }
    };
    signature[64] = v_bit; // v

    let address_res = ecrecover_impl(H256::from_slice(&hash), &signature);
    let output = match address_res {
        Ok(a) => {
            let mut output = [0u8; 32];
            output[12..32].copy_from_slice(a.as_bytes());
            output.to_vec()
        }
        Err(_) => Vec::new(),
    };

    Ok(PrecompileOutput {
        exit_status: ExitSucceed::Returned,
        output,
    })
}

fn ecrecover_impl(hash: H256, signature: &[u8]) -> Result<Address, ExitError> {
    use sha3::Digest;

    let hash = libsecp256k1::Message::parse_slice(hash.as_bytes()).unwrap();
    let v = signature[64];
    let signature = libsecp256k1::Signature::parse_standard_slice(&signature[0..64])
        .map_err(|_| ExitError::Other(Cow::Borrowed("ERR_ECRECOVER")))?;
    let bit = match v {
        0..=26 => v,
        _ => v - 27,
    };

    if let Ok(recovery_id) = libsecp256k1::RecoveryId::parse(bit) {
        if let Ok(public_key) = libsecp256k1::recover(&hash, &signature, &recovery_id) {
            // recover returns a 65-byte key, but addresses come from the raw 64-byte key
            let r = sha3::Keccak256::digest(&public_key.serialize()[1..]);
            return try_address_from_slice(&r[12..])
                .ok_or(ExitError::Other(Cow::Borrowed("ERR_INCORRECT_ADDRESS")));
        }
    }

    Err(ExitError::Other(Cow::Borrowed("ERR_ECRECOVER")))
}

fn try_address_from_slice(raw_addr: &[u8]) -> Option<Address> {
    if raw_addr.len() != 20 {
        return None;
    }
    Some(H160::from_slice(raw_addr))
}

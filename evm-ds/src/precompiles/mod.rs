pub mod ecrecover;
pub mod identity;
pub mod modexp;
pub mod ripemd160;
pub mod sha2_256;

use std::collections::BTreeMap;
use std::str::FromStr;

use evm::executor::stack::PrecompileFn;

use primitive_types::*;

pub fn get_precompiles() -> BTreeMap<H160, PrecompileFn> {
    BTreeMap::from([
        (
            H160::from_str("0000000000000000000000000000000000000001").unwrap(),
            ecrecover::ecrecover as PrecompileFn,
        ),
        (
            H160::from_str("0000000000000000000000000000000000000002").unwrap(),
            sha2_256::sha2_256 as PrecompileFn,
        ),
        (
            H160::from_str("0000000000000000000000000000000000000003").unwrap(),
            ripemd160::ripemd160 as PrecompileFn,
        ),
        (
            H160::from_str("0000000000000000000000000000000000000004").unwrap(),
            identity::identity as PrecompileFn,
        ),
        (
            H160::from_str("0000000000000000000000000000000000000005").unwrap(),
            modexp::modexp as PrecompileFn,
        ),
    ])
}
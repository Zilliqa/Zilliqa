use primitive_types::H256;
use protobuf::Message;

use crate::protos::{Evm as EvmProto, ScillaMessage};

fn address_to_string(address: &EvmProto::Address) -> String {
    let mut buffer = [0; 20];
    buffer[..4].clone_from_slice(&address.get_x0().to_be_bytes());
    buffer[4..12].clone_from_slice(&address.get_x1().to_be_bytes());
    buffer[12..].clone_from_slice(&address.get_x2().to_be_bytes());
    hex::encode(buffer)
}

fn uint256_to_string(number: &EvmProto::UInt256) -> String {
    let buffer = [number.get_x0().to_be_bytes(), number.get_x1().to_be_bytes(), number.get_x2().to_be_bytes(), number.get_x3().to_be_bytes()].concat();
    let number_string = H256::from_slice(&buffer);
    number_string.to_string()
}

fn decode_storage(storage: &EvmProto::Storage) -> (H256, H256) {
    let query = ScillaMessage::ProtoScillaQuery::parse_from_bytes(&storage.key).unwrap();
    // query.set_indices(vec![bytes::Bytes::from(format!("{:X}", key))]);
    let val = ScillaMessage::ProtoScillaVal::parse_from_bytes(&storage.value).unwrap();
    (H256::default(), H256::from_slice(val.get_bval()))
    //(H256::from_slice(&query.get_indices()[0]), H256::from_slice(val.get_bval()))
}

fn storage_key_to_string(storage: &EvmProto::Storage) -> String {
    "1".to_string()
}

fn storage_value_to_string(storage: &EvmProto::Storage) -> String {
    println!("KHAR {:?}", storage.get_value().to_vec());
    "1".to_string()
}

fn log_apply_modify(modify: &EvmProto::Apply_Modify) {
    print!("  modify {{\n    address: {},\n    balance: {:?},\n    nonce: {:?},\n",
            address_to_string(modify.get_address()), uint256_to_string(modify.get_balance()),
            uint256_to_string(modify.get_nonce()));

    if modify.get_storage().len() > 0 {
        println!("    storage: [");
        modify.get_storage().into_iter().for_each(|s| {
            let (key, value) = decode_storage(s);
            println!("      {{\n        key: {}, \n        value: {}\n      }}", "khar", value.to_string());
        });
        println!("    ]");
    }

    println!("  }}");
}

fn log_apply_delete(delete: &EvmProto::Apply_Delete) {
    println!(
        "  delete {{\n    address: {} \n  }}",
        address_to_string(delete.get_address())
    );
}

/// TODO: REMOVE SAEED
pub fn log_evm_result(result: &EvmProto::EvmResult) {
    println!("saeed: \n\n\n{:#?} exit_reason: {:#?}", result, result.get_exit_reason());
    result.get_apply().into_iter().for_each(|optional_apply| {
        if let Some(apply) = &optional_apply.apply {
            println!("apply {{");
            match apply {
                EvmProto::Apply_oneof_apply::modify(ref modify) => log_apply_modify(modify),
                EvmProto::Apply_oneof_apply::delete(ref delete) => log_apply_delete(delete),
            }
            println!("}}");
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn address_to_string_test() {
        let mut address = EvmProto::Address::default();
        address.set_x0(1383840601);
        address.set_x1(1643823253561742928);
        address.set_x2(596286955265148733);
        let addr_string = address_to_string(&address);
        assert_eq!(addr_string, "527bbb5916d0088a2e13165008466fd398c7473d");
    }

    #[test]
    fn uint256_to_string_test() {
        let mut number = EvmProto::UInt256::default();
        number.set_x3(300000);
        let number_string = uint256_to_string(&number);
        assert_eq!(number_string, "0x0000…93e0");
    }
}
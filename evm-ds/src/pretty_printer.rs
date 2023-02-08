use primitive_types::H256;
use protobuf::Message;
use log::debug;
use std::fmt::Write;

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

fn apply_modify_to_string(modify: &EvmProto::Apply_Modify) -> String {
    let mut modify_string = String::new();
    write!(modify_string, "  modify {{\n    address: {},\n    balance: {:?},\n    nonce: {:?},\n",
            address_to_string(modify.get_address()), uint256_to_string(modify.get_balance()),
            uint256_to_string(modify.get_nonce())).unwrap();

    if modify.get_storage().len() > 0 {
        write!(modify_string, "    storage: [").unwrap();
        modify.get_storage().into_iter().for_each(|s| {
            let query = ScillaMessage::ProtoScillaQuery::parse_from_bytes(&s.key).unwrap();
            let value = ScillaMessage::ProtoScillaVal::parse_from_bytes(&s.value).unwrap();
            write!(modify_string, "      {{\n        key: {:?}, \n        value: {}\n      }},\n",
                query.get_indices(), H256::from_slice(&value.get_bval())).unwrap();
        });
        write!(modify_string, "    ]\n").unwrap();
    }

    modify_string.push_str("  }\n");
    modify_string
}

fn apply_delete_to_string(delete: &EvmProto::Apply_Delete) -> String {
    format!("  delete {{\n    address: {} \n  }}\n",
        address_to_string(delete.get_address())
    )
}

pub fn log_evm_result(result: &EvmProto::EvmResult) {
    let mut result_string = String::new();

    debug!("evm_result: {:#?}", result);
    write!(result_string, "\nexit_reason: {:#?}", result.get_exit_reason()).unwrap();
    result.get_apply().into_iter().for_each(|optional_apply| {
        if let Some(apply) = &optional_apply.apply {
            result_string.push_str("apply {\n");
            match apply {
                EvmProto::Apply_oneof_apply::modify(ref modify) => result_string.push_str(&apply_modify_to_string(modify)),
                EvmProto::Apply_oneof_apply::delete(ref delete) => result_string.push_str(&apply_delete_to_string(delete)),
            }
            result_string.push_str("}");
        }
    });

    debug!("{}", result_string);
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
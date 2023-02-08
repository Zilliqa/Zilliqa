use primitive_types::{H256, H160, U256};
use protobuf::Message;
use log::debug;
use std::fmt::Write;

use crate::protos::{Evm as EvmProto, ScillaMessage};

fn apply_modify_to_string(modify: &EvmProto::Apply_Modify) -> String {
    let mut modify_string = String::new();
    write!(modify_string, "  modify {{\n    address: {},\n    balance: {:?},\n    nonce: {:?},\n",
            H160::from(modify.get_address()), U256::from(modify.get_balance()),
            U256::from(modify.get_nonce())).unwrap();

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
        H160::from(delete.get_address())
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
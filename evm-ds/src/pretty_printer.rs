use primitive_types::{H160, H256, U256};
use protobuf::Message;
use std::fmt::Write;

use crate::protos::{Evm as EvmProto, ScillaMessage};

fn apply_modify_to_string(modify: &EvmProto::Apply_Modify) -> String {
    let mut modify_string = String::new();
    write!(
        modify_string,
        "  modify {{\n    address: {},\n    balance: {:?},\n    nonce: {:?},\n",
        H160::from(modify.get_address()),
        U256::from(modify.get_balance()),
        U256::from(modify.get_nonce())
    )
    .unwrap();

    if !modify.get_storage().is_empty() {
        write!(modify_string, "    storage: [").unwrap();
        modify.get_storage().iter().for_each(|s| {
            let query = ScillaMessage::ProtoScillaQuery::parse_from_bytes(&s.key);
            let value = ScillaMessage::ProtoScillaVal::parse_from_bytes(&s.value);

            if query.is_err() || value.is_err() {
                return;
            }

            let query = query.unwrap();
            let value = value.unwrap();

            write!(
                modify_string,
                "      {{\n        key: {:?}, \n        value: {}\n      }},\n",
                query.get_indices(),
                H256::from_slice(value.get_bval())
            )
            .unwrap();
        });
        writeln!(modify_string, "    ]").unwrap();
    }

    modify_string.push_str("  }\n");
    modify_string
}

fn apply_delete_to_string(delete: &EvmProto::Apply_Delete) -> String {
    format!(
        "  delete {{\n    address: {} \n  }}\n",
        H160::from(delete.get_address())
    )
}

pub fn log_evm_result(result: &EvmProto::EvmResult) -> String {
    let mut result_string = String::new();

    write!(
        result_string,
        "\nexit_reason: {:#?}",
        result.get_exit_reason()
    )
    .unwrap();
    result.get_apply().iter().for_each(|optional_apply| {
        if let Some(apply) = &optional_apply.apply {
            result_string.push_str("apply {\n");
            match apply {
                EvmProto::Apply_oneof_apply::modify(ref modify) => {
                    result_string.push_str(&apply_modify_to_string(modify))
                }
                EvmProto::Apply_oneof_apply::delete(ref delete) => {
                    result_string.push_str(&apply_delete_to_string(delete))
                }
            }
            result_string.push('}');
        }
    });

    result_string
}

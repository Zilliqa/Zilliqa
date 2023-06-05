use crate::utils;
use anyhow::{anyhow, Result};
use hex;
use pdtlib::proto::ProtoTransactionWithReceipt;
use primitive_types::{H160, H256};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone)]
pub struct Transaction {
    id: String,
    block: i64,
    amount: Option<String>,
    code: Option<String>,
    data: Option<String>,
    gas_limit: i64,
    gas_price: Option<String>,
    nonce: Option<i64>,
    receipt: Option<String>,
    sender_public_key: Option<String>,
    signature: Option<String>,
    to_addr: String,
    version: i64,
    cum_gas: Option<i64>,
    shard_id: Option<i64>,
}

impl Transaction {
    pub fn from_proto(
        in_val: &ProtoTransactionWithReceipt,
        blk: i64,
        shard_id: i64,
    ) -> Result<Self> {
        let val = in_val.clone();
        let txn = val
            .transaction
            .ok_or(anyhow!("Transaction object does not contain transaction"))?;
        let core = txn.info.ok_or(anyhow!("Transaction has no core info"))?;
        let gas_limit: i64 = <u64>::try_into(core.gaslimit)?;
        let id = H256::from_slice(&txn.tranid);
        let to_addr = H160::from_slice(&core.toaddr);
        let sender_public_key = core.senderpubkey.and_then(|x| Some(hex::encode(&x.data)));
        let nonce: Option<i64> = if let Some(nonce_val) = core.oneof2 {
            if let pdtlib::proto::proto_transaction_core_info::Oneof2::Nonce(actual) = nonce_val {
                Some(<i64>::try_from(actual)?)
            } else {
                None
            }
        } else {
            None
        };
        println!("X");
        let code: Option<String> = core.oneof8.and_then(|x| {
            if let pdtlib::proto::proto_transaction_core_info::Oneof8::Code(y) = x {
                std::str::from_utf8(&y)
                    .ok()
                    .and_then(|x| Some(x.to_string()))
            } else {
                None
            }
        });
        let data: Option<String> = core.oneof9.and_then(|x| {
            if let pdtlib::proto::proto_transaction_core_info::Oneof9::Data(y) = x {
                std::str::from_utf8(&y)
                    .ok()
                    .and_then(|x| Some(x.to_string()))
            } else {
                None
            }
        });

        let signature = txn
            .signature
            .and_then(|x| utils::str_from_u8(Some(x)).ok())
            .flatten();
        let receipt = val.receipt.as_ref().and_then(|x| {
            std::str::from_utf8(&x.receipt)
                .ok()
                .and_then(|x| Some(x.to_string()))
        });
        println!("A");
        let cum_gas = val.receipt.as_ref().and_then(|x| {
            x.oneof2.as_ref().and_then(|y| {
                if let pdtlib::proto::proto_transaction_receipt::Oneof2::Cumgas(z) = y {
                    <u64>::try_into(*z).ok()
                } else {
                    None
                }
            })
        });
        println!(
            "C {:?} D {:?}",
            core.amount.clone().and_then(|x| Some(hex::encode(&x.data))),
            core.gasprice
                .clone()
                .and_then(|x| Some(hex::encode(&x.data)))
        );
        let amount = core
            .amount
            .clone()
            .and_then(|x| utils::u128_string_from_storage(&x));
        let gas_price = core
            .gasprice
            .clone()
            .and_then(|x| utils::u128_string_from_storage(&x));
        println!("B");
        Ok(Transaction {
            id: hex::encode(id.as_bytes()),
            block: blk,
            amount,
            code,
            data,
            gas_limit,
            gas_price,
            nonce,
            receipt,
            sender_public_key: sender_public_key,
            signature,
            to_addr: hex::encode(to_addr.as_bytes()),
            version: i64::from(core.version),
            cum_gas,
            shard_id: Some(shard_id),
        })
    }
}

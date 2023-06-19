use crate::bq;
use crate::utils;
use anyhow::{anyhow, Result};
use base64::Engine;
use hex;
use pdtlib::proto::ProtoTransactionWithReceipt;
use primitive_types::{H160, H256};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone)]
pub struct Transaction {
    pub id: String,
    pub block: i64,
    pub offset_in_block: i64,
    pub amount: Option<String>,
    // application/x-scilla-contract or application/x-evm-contract
    pub api_type: Option<String>,
    pub code: Option<String>,
    pub data: Option<String>,
    pub gas_limit: i64,
    pub gas_price: Option<String>,
    pub nonce: Option<i64>,
    pub receipt: Option<String>,
    pub sender_public_key: Option<String>,
    pub from_addr: Option<String>,
    pub signature: Option<String>,
    pub to_addr: String,
    pub version: i64,
    pub cum_gas: Option<i64>,
    pub shard_id: Option<i64>,
}

impl bq::BlockInsertable for Transaction {
    fn get_coords(&self) -> (i64, i64) {
        (self.block, self.offset_in_block)
    }

    /// Guess how many bytes this txn will take when encoded
    /// If we wanted to be more accurate, we could serialise and measure,
    /// but that would be quite expensive.
    fn estimate_bytes(&self) -> Result<usize> {
        // Annoyingly, because of Javascript escaping, this is the only way :-(
        Ok(self.to_json()?.len())
    }
}

impl Transaction {
    pub fn from_proto(
        in_val: &ProtoTransactionWithReceipt,
        blk: i64,
        offset_in_block: i64,
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
        let sender_public_key = core
            .senderpubkey
            .as_ref()
            .map_or(None, |x| Some(encode_u8(x.data.as_slice())));
        let from_addr = core
            .senderpubkey
            .as_ref()
            .and_then(|x| utils::maybe_hex_address_from_public_key(&x.data, utils::API::Zilliqa));
        let nonce: Option<i64> = if let Some(nonce_val) = core.oneof2 {
            let pdtlib::proto::proto_transaction_core_info::Oneof2::Nonce(actual) = nonce_val;
            Some(<i64>::try_from(actual)?)
        } else {
            None
        };
        let api_type = Some("unknown".to_string());

        let code = core.oneof8.map_or(None, |x| {
            let pdtlib::proto::proto_transaction_core_info::Oneof8::Code(y) = x;
            Some(encode_u8(y.as_slice()))
        });
        let data = core.oneof9.map_or(None, |x| {
            let pdtlib::proto::proto_transaction_core_info::Oneof9::Data(y) = x;
            Some(encode_u8(y.as_slice()))
        });

        // let signature = txn
        //     .signature
        //     .and_then(|x| utils::str_from_u8(Some(x)).ok())
        //     .flatten();
        let signature = txn
            .signature
            .map_or(None, |x| Some(encode_u8(x.data.as_slice())));

        let receipt = val.receipt.as_ref().and_then(|x| {
            std::str::from_utf8(&x.receipt)
                .ok()
                .and_then(|x| Some(x.to_string()))
        });
        let cum_gas = val.receipt.as_ref().and_then(|x| {
            x.oneof2.as_ref().and_then(|y| {
                let pdtlib::proto::proto_transaction_receipt::Oneof2::Cumgas(z) = y;
                <u64>::try_into(*z).ok()
            })
        });
        // println!(
        //     "C {:?} D {:?}",
        //     core.amount.clone().and_then(|x| Some(hex::encode(&x.data))),
        //     core.gasprice
        //         .clone()
        //         .and_then(|x| Some(hex::encode(&x.data)))
        // );
        let amount = core
            .amount
            .clone()
            .and_then(|x| utils::u128_string_from_storage(&x));
        let gas_price = core
            .gasprice
            .clone()
            .and_then(|x| utils::u128_string_from_storage(&x));
        Ok(Transaction {
            id: hex::encode(id.as_bytes()),
            block: blk,
            offset_in_block,
            amount,
            api_type,
            code,
            data,
            gas_limit,
            gas_price,
            nonce,
            receipt,
            sender_public_key,
            from_addr,
            signature,
            to_addr: hex::encode(to_addr.as_bytes()),
            version: i64::from(core.version),
            cum_gas,
            shard_id: Some(shard_id),
        })
    }

    pub fn to_json(&self) -> Result<String> {
        Ok(serde_json::to_string(self)?)
    }
}

fn encode_u8(y: &[u8]) -> String {
    base64::engine::general_purpose::STANDARD
        .encode(y)
        .to_string()
}

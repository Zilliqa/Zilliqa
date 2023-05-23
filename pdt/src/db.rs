#![allow(dead_code)]

use std::{
    collections::HashMap, convert::Infallible, fs, num::ParseIntError, path::Path,
    string::FromUtf8Error, sync::Arc,
};

use anyhow::{anyhow, Result};
use eth_trie::{EthTrie, Trie, DB};
use leveldb::{
    db::Database,
    iterator::{Iterable, LevelDBIterator},
    options::{Options, ReadOptions, WriteOptions},
};
use primitive_types::{H160, H256};
use prost::Message;
use sha2::Digest;
use sha3::Keccak256;
use thiserror::Error;

use crate::proto::{
    ProtoAccountBase, ProtoAccountStore, ProtoBlockLink, ProtoDsBlock, ProtoMicroBlock,
    ProtoMicroBlockKey, ProtoMinerInfoDsComm, ProtoMinerInfoShards, ProtoStateIndex,
    ProtoTransactionReceipt, ProtoTransactionWithReceipt, ProtoTxBlock, ProtoTxEpoch, ProtoVcBlock,
};

struct ShardedDb {
    instances: HashMap<usize, Database>,
}

impl ShardedDb {
    fn new(dir: &Path, name: &str) -> Result<Self> {
        let options = Options {
            create_if_missing: true,
            ..Options::new()
        };
        let prefix = format!("{name}_");
        let mut instances: HashMap<_, _> = fs::read_dir(dir)?
            .filter_map(|entry| {
                let entry = entry.ok()?;
                let file_name = entry.file_name().to_str()?.to_owned();
                if let Some(suffix) = file_name.strip_prefix(&prefix) {
                    let index: usize = suffix.parse().ok()?;
                    Some((index, Database::open(&entry.path(), &options).ok()?))
                } else {
                    None
                }
            })
            .collect();

        // The first database has no suffix.
        instances.insert(0, Database::open(&dir.join(name), &options)?);

        Ok(Self { instances })
    }

    fn db_at(&self, epoch: usize) -> &Database {
        self.instances.get(&epoch).unwrap()
    }

    fn iter(&self) -> impl Iterator<Item = (Vec<u8>, Vec<u8>)> + '_ {
        self.instances
            .values()
            .flat_map(|db| db.iter(&ReadOptions::new()))
    }
}

#[derive(Debug)]
pub struct StateDatabase {
    db: Database,
    delete: bool,
}

impl StateDatabase {
    pub fn new(db: Database, delete: bool) -> Result<StateDatabase> {
        let db = StateDatabase { db, delete };

        let value = rlp::NULL_RLP;
        let key = Keccak256::digest(value);
        db.insert(&key, value.to_vec())?;

        Ok(db)
    }
}

impl DB for StateDatabase {
    type Error = leveldb::error::Error;

    fn get(&self, key: &[u8]) -> Result<Option<Vec<u8>>, Self::Error> {
        self.db
            .get_u8(&ReadOptions::new(), hex::encode(key).as_bytes())
    }

    fn insert(&self, key: &[u8], value: Vec<u8>) -> Result<(), Self::Error> {
        self.db
            .put_u8(&WriteOptions::new(), hex::encode(key).as_bytes(), &value)?;

        Ok(())
    }

    fn remove(&self, key: &[u8]) -> Result<(), Self::Error> {
        if self.delete {
            self.db
                .delete_u8(&WriteOptions::new(), hex::encode(key).as_bytes())?;
        }

        Ok(())
    }

    fn flush(&self) -> Result<(), Self::Error> {
        Ok(())
    }
}

pub struct Db {
    block_links: Database,
    contract_code: Database,
    contract_init_state_2: Database,
    contract_state_data_2: Database,
    contract_state_index: Database,
    contract_trie: EthTrie<StateDatabase>,
    ds_blocks: Database,
    ds_committee: Database,
    ext_seed_pub_keys: Database,
    metadata: Database,
    micro_block_keys: Database,
    micro_blocks: ShardedDb,
    miner_info_ds_comm: Database,
    miner_info_shards: Database,
    processed_txn_tmp: Database,
    shard_structure: Database,
    state: EthTrie<StateDatabase>,
    state_purge: Database,
    state_delta: Database,
    state_root: Database,
    temp_state: Database,
    tx_block_hash_to_num: Database,
    tx_blocks: Database,
    tx_blocks_aux: Database,
    tx_bodies: ShardedDb,
    tx_epochs: Database,
    vc_blocks: Database,
}

impl Db {
    pub fn new(dir: impl AsRef<Path>) -> Result<Self> {
        let options = Options {
            create_if_missing: true,
            ..Options::new()
        };
        let dir = dir.as_ref();
        let state_root = Database::open(&dir.join("stateRoot"), &options)?;
        let trie_root = state_root
            .get_u8(&ReadOptions::new(), 0.to_string().as_bytes())?
            .map(|r| H256::from_slice(&r));

        let state = EthTrie::new(Arc::new(StateDatabase::new(
            Database::open(&dir.join("state"), &options)?,
            false,
        )?));
        let state = if let Some(trie_root) = trie_root {
            state.at_root(trie_root)
        } else {
            state
        };

        Ok(Db {
            block_links: Database::open(&dir.join("blockLinks"), &options)?,
            contract_code: Database::open(&dir.join("contractCode"), &options)?,
            contract_init_state_2: Database::open(&dir.join("contractInitState2"), &options)?,
            contract_state_data_2: Database::open(&dir.join("contractStateData2"), &options)?,
            contract_state_index: Database::open(&dir.join("contractStateIndex"), &options)?,
            contract_trie: EthTrie::new(Arc::new(StateDatabase::new(
                Database::open(&dir.join("contractTrie"), &options)?,
                false,
            )?)),
            ds_blocks: Database::open(&dir.join("dsBlocks"), &options)?,
            ds_committee: Database::open(&dir.join("dsCommittee"), &options)?,
            ext_seed_pub_keys: Database::open(&dir.join("extSeedPubKeys"), &options)?,
            metadata: Database::open(&dir.join("metadata"), &options)?,
            micro_block_keys: Database::open(&dir.join("microBlockKeys"), &options)?,
            micro_blocks: ShardedDb::new(dir, "microBlocks")?,
            miner_info_ds_comm: Database::open(&dir.join("minerInfoDSComm"), &options)?,
            miner_info_shards: Database::open(&dir.join("minerInfoShards"), &options)?,
            processed_txn_tmp: Database::open(&dir.join("processedTxnTmp"), &options)?,
            shard_structure: Database::open(&dir.join("shardStructure"), &options)?,
            state,
            state_purge: Database::open(&dir.join("state_purge"), &options)?,
            state_delta: Database::open(&dir.join("stateDelta"), &options)?,
            state_root,
            temp_state: Database::open(&dir.join("tempState"), &options)?,
            tx_block_hash_to_num: Database::open(&dir.join("txBlockHashToNum"), &options)?,
            tx_blocks: Database::open(&dir.join("txBlocks"), &options)?,
            tx_blocks_aux: Database::open(&dir.join("txBlocksAux"), &options)?,
            tx_bodies: ShardedDb::new(dir, "txBodies")?,
            tx_epochs: Database::open(&dir.join("txEpochs"), &options)?,
            vc_blocks: Database::open(&dir.join("VCBlocks"), &options)?,
        })
    }

    pub fn block_links(&self) -> Result<impl Iterator<Item = Result<(u64, ProtoBlockLink)>> + '_> {
        kv_iter(&self.block_links, key_to_id, |v| {
            ProtoBlockLink::decode(v.as_slice())
        })
    }

    pub fn get_contract_code(&self, address: H160) -> Result<Option<Vec<u8>>> {
        Ok(self
            .contract_code
            .get_u8(&ReadOptions::new(), format!("{:02x}", address).as_bytes())?)
    }

    pub fn put_contract_code(&self, address: H160, code: &[u8]) -> Result<()> {
        Ok(self.contract_code.put_u8(
            &WriteOptions::new(),
            format!("{:02x}", address).as_bytes(),
            code,
        )?)
    }

    pub fn contract_code(&self) -> Result<impl Iterator<Item = Result<(H160, Vec<u8>)>> + '_> {
        kv_iter(&self.contract_code, h160_from_hex, Ok::<_, Infallible>)
    }

    pub fn get_contract_init_state_2(&self, account: H160) -> Result<Option<Vec<u8>>> {
        Ok(self
            .contract_init_state_2
            .get_u8(&ReadOptions::new(), format!("{:02x}", account).as_bytes())?)
    }

    pub(crate) fn put_init_state_2(&self, address: H160, init_data: &[u8]) -> Result<()> {
        Ok(self.contract_init_state_2.put_u8(
            &WriteOptions::new(),
            format!("{:02x}", address).as_bytes(),
            init_data,
        )?)
    }

    pub fn contract_init_state_2(
        &self,
    ) -> Result<impl Iterator<Item = Result<(H160, Vec<u8>)>> + '_> {
        kv_iter(
            &self.contract_init_state_2,
            h160_from_hex,
            Ok::<_, Infallible>,
        )
    }

    pub fn get_contract_state_data(&self, k: &str) -> Result<Option<Vec<u8>>> {
        Ok(self
            .contract_state_data_2
            .get_u8(&ReadOptions::new(), k.as_bytes())?)
    }

    pub fn get_contract_state_data_with_prefix(
        &self,
        prefix: &str,
    ) -> impl Iterator<Item = Result<(String, Vec<u8>)>> + '_ {
        let prefix = prefix.to_owned();
        let iter = self.contract_state_data_2.iter(&ReadOptions::new());
        iter.seek(prefix.as_bytes());
        iter.take_while(move |(k, _)| k.starts_with(prefix.as_bytes()))
            .map(|(k, v)| Ok((String::from_utf8(k)?, v)))
    }

    pub fn put_contract_state_data(&self, k: &str, v: &[u8]) -> Result<()> {
        Ok(self
            .contract_state_data_2
            .put_u8(&WriteOptions::new(), k.as_bytes(), v)?)
    }

    pub fn delete_contract_state_data(&self, k: &str) -> Result<()> {
        Ok(self
            .contract_state_data_2
            .delete_u8(&WriteOptions::new(), k.as_bytes())?)
    }

    pub fn contract_state(&self) -> impl Iterator<Item = Result<(ContractStateKey, Vec<u8>)>> + '_ {
        self.contract_state_data_2
            .iter(&ReadOptions::new())
            .map(|(k, v)| {
                let k = ContractStateKey::new(String::from_utf8(k)?)?;
                //let v = String::from_utf8(v)?;

                Ok((k, v))
            })
    }

    pub fn contract_state_index(
        &self,
    ) -> Result<impl Iterator<Item = Result<(H160, ProtoStateIndex)>> + '_> {
        kv_iter(&self.contract_state_index, h160_from_hex, |v| {
            ProtoStateIndex::decode(v.as_slice())
        })
    }

    pub fn new_contract_trie(&self) -> EthTrie<StateDatabase> {
        self.contract_trie
            .at_root(H256::from_slice(&Keccak256::digest(rlp::NULL_RLP)))
    }

    pub fn contract_trie_at(&self, root: H256) -> EthTrie<StateDatabase> {
        self.contract_trie.at_root(root)
    }

    pub fn get_ds_block(&self, block: u64) -> Result<Option<ProtoDsBlock>> {
        Ok(self
            .ds_blocks
            .get_u8(&ReadOptions::new(), block.to_string().as_bytes())?
            .map(|d| ProtoDsBlock::decode(d.as_slice()).unwrap()))
    }

    pub fn put_ds_block(&self, block_num: u64, block: ProtoDsBlock) -> Result<()> {
        let key = block_num.to_string();
        Ok(self
            .ds_blocks
            .put_u8(&WriteOptions::new(), key.as_bytes(), &block.encode_to_vec())?)
    }

    pub fn ds_blocks(&self) -> Result<impl Iterator<Item = Result<(u64, ProtoDsBlock)>> + '_> {
        kv_iter(&self.ds_blocks, key_to_id, |v| {
            ProtoDsBlock::decode(v.as_slice())
        })
    }

    pub fn ds_committee(&self) -> Result<impl Iterator<Item = Result<(u64, Vec<u8>)>> + '_> {
        kv_iter(&self.ds_committee, key_to_id, Ok::<_, Infallible>)
    }

    pub fn ext_seed_pub_keys(&self) -> Result<impl Iterator<Item = Result<(u64, Vec<u8>)>> + '_> {
        kv_iter(&self.ext_seed_pub_keys, key_to_id, Ok::<_, Infallible>)
    }

    pub fn metadata(&self) -> Result<impl Iterator<Item = Result<(u64, Vec<u8>)>> + '_> {
        kv_iter(&self.metadata, key_to_id, Ok::<_, Infallible>)
    }

    pub fn get_micro_block_key(&self, hash: H256) -> Result<Option<ProtoMicroBlockKey>> {
        let key = format!("{hash:x}");
        Ok(self
            .micro_block_keys
            .get_u8(&ReadOptions::new(), key.as_bytes())?
            .map(|v| ProtoMicroBlockKey::decode(v.as_slice()))
            .transpose()?)
    }

    pub fn micro_block_keys(
        &self,
    ) -> Result<impl Iterator<Item = Result<(H256, ProtoMicroBlockKey)>> + '_> {
        kv_iter(&self.micro_block_keys, h256_from_hex, |v| {
            ProtoMicroBlockKey::decode(v.as_slice())
        })
    }

    pub fn get_micro_block(&self, key: &ProtoMicroBlockKey) -> Result<Option<ProtoMicroBlock>> {
        let db_index = key.epochnum / 250000;
        let Some(mb) = self.micro_blocks.db_at(db_index as usize).get_u8(&ReadOptions::new(), &key.encode_to_vec())? else { return Ok(None); };

        Ok(Some(ProtoMicroBlock::decode(mb.as_slice())?))
    }

    pub fn micro_blocks(
        &self,
    ) -> Result<impl Iterator<Item = Result<(ProtoMicroBlockKey, ProtoMicroBlock)>> + '_ + '_> {
        Ok(self.micro_blocks.iter().map(|(k, v)| {
            ProtoMicroBlockKey::decode(k.as_slice())
                .and_then(|k| ProtoMicroBlock::decode(v.as_slice()).map(|v| (k, v)))
                .map_err(Into::into)
        }))
    }

    pub fn get_miner_info_ds_comm(&self, block: u64) -> Result<Option<ProtoMinerInfoDsComm>> {
        let Some(info) = self.miner_info_ds_comm.get_u8(&ReadOptions::new(), block.to_string().as_bytes())? else { return Ok(None); };

        Ok(Some(ProtoMinerInfoDsComm::decode(info.as_slice())?))
    }

    pub fn miner_info_ds_comm(
        &self,
    ) -> Result<impl Iterator<Item = Result<(u64, ProtoMinerInfoDsComm)>> + '_> {
        kv_iter(&self.miner_info_ds_comm, key_to_id, |v| {
            ProtoMinerInfoDsComm::decode(v.as_slice())
        })
    }

    pub fn get_miner_info_shard(&self, block: u64) -> Result<Option<ProtoMinerInfoShards>> {
        let Some(info) = self.miner_info_shards.get_u8(&ReadOptions::new(), block.to_string().as_bytes())? else { return Ok(None); };

        Ok(Some(ProtoMinerInfoShards::decode(info.as_slice())?))
    }

    pub fn miner_info_shards(
        &self,
    ) -> Result<impl Iterator<Item = Result<(u64, ProtoMinerInfoShards)>> + '_> {
        kv_iter(&self.miner_info_shards, key_to_id, |v| {
            ProtoMinerInfoShards::decode(v.as_slice())
        })
    }

    pub fn processed_txn_tmp(
        &self,
    ) -> Result<impl Iterator<Item = Result<(H256, ProtoTransactionReceipt)>> + '_> {
        kv_iter(&self.processed_txn_tmp, h256_from_hex, |v| {
            ProtoTransactionReceipt::decode(v.as_slice())
        })
    }

    pub fn save_account(&mut self, address: H160, account: ProtoAccountBase) -> Result<()> {
        self.state.insert(
            format!("{:02x}", address).as_bytes(),
            &account.encode_to_vec(),
        )?;

        Ok(())
    }

    pub fn state_root_hash(&mut self) -> Result<H256> {
        Ok(self.state.root_hash()?)
    }

    pub fn accounts(&self) -> impl Iterator<Item = (H160, ProtoAccountBase)> + '_ {
        self.state.iter().map(|(k, v)| {
            (
                H160::from_slice(&hex::decode(String::from_utf8(k).unwrap()).unwrap()),
                ProtoAccountBase::decode(v.as_slice()).unwrap(),
            )
        })
    }

    pub fn accounts_at(&self, root_hash: H256) -> EthTrie<StateDatabase> {
        self.state.at_root(root_hash)
    }

    pub fn get_account(&self, address: H160) -> Result<Option<ProtoAccountBase>> {
        let Some(account) = self.state.get(format!("{:02x}", address).as_bytes())? else { return Ok(None); };

        Ok(Some(ProtoAccountBase::decode(account.as_slice())?))
    }

    pub fn state_root(&self) -> Result<H256> {
        let trie_root = self
            .state_root
            .get_u8(&ReadOptions::new(), 0.to_string().as_bytes())?
            .ok_or_else(|| anyhow!("no state root"))?;
        Ok(H256::from_slice(&trie_root))
    }

    pub fn put_state_root(&self, state_root: H256) -> Result<()> {
        Ok(self.state_root.put_u8(
            &WriteOptions::new(),
            0.to_string().as_bytes(),
            state_root.as_bytes(),
        )?)
    }

    pub fn state_purge(&self) -> Result<impl Iterator<Item = Result<(u64, Vec<u8>)>> + '_> {
        kv_iter(&self.state_purge, key_to_id, Ok::<_, Infallible>)
    }

    pub fn get_shard_structure(&self, block: u64) -> Result<Option<Vec<u8>>> {
        let Some(ss) = self.shard_structure.get_u8(&ReadOptions::new(), block.to_string().as_bytes())? else { return Ok(None); };

        Ok(Some(ss))
    }

    pub fn shard_structure(&self) -> Result<impl Iterator<Item = Result<(u64, Vec<u8>)>> + '_> {
        kv_iter(&self.shard_structure, key_to_id, Ok::<_, Infallible>)
    }

    pub fn get_state_delta(&self, key: u64) -> Result<Option<ProtoAccountStore>> {
        let key = key.to_string();
        Ok(self
            .state_delta
            .get_u8(&ReadOptions::new(), key.as_bytes())?
            .map(|v| ProtoAccountStore::decode(v.as_slice()))
            .transpose()?)
    }

    pub fn put_state_delta(&self, key: u64, value: ProtoAccountStore) -> Result<()> {
        let key = key.to_string();
        Ok(self
            .state_delta
            .put_u8(&WriteOptions::new(), key.as_bytes(), &value.encode_to_vec())?)
    }

    pub fn state_delta(
        &self,
    ) -> Result<impl Iterator<Item = Result<(u64, ProtoAccountStore)>> + '_> {
        kv_iter(&self.state_delta, key_to_id, |v| {
            ProtoAccountStore::decode(v.as_slice())
        })
    }

    pub fn temp_state(
        &self,
    ) -> Result<impl Iterator<Item = Result<(H160, ProtoAccountBase)>> + '_> {
        kv_iter(&self.temp_state, h160_from_hex, |v| {
            ProtoAccountBase::decode(v.as_slice())
        })
    }

    pub fn tx_block_hash_to_num(&self) -> Result<impl Iterator<Item = Result<(H256, u64)>> + '_> {
        kv_iter(&self.tx_block_hash_to_num, h256_from_be, key_to_id)
    }

    pub fn tx_blocks(&self) -> Result<impl Iterator<Item = Result<(u64, ProtoTxBlock)>> + '_> {
        kv_iter(&self.tx_blocks, key_to_id, |v| {
            ProtoTxBlock::decode(v.as_slice())
        })
    }

    pub fn get_tx_block(&self, key: u64) -> Result<Option<ProtoTxBlock>> {
        let key = key.to_string();
        Ok(self
            .tx_blocks
            .get_u8(&ReadOptions::new(), key.as_bytes())?
            .map(|v| ProtoTxBlock::decode(v.as_slice()))
            .transpose()?)
    }

    pub fn put_tx_block(&self, key: u64, block: ProtoTxBlock) -> Result<()> {
        let key = key.to_string();
        Ok(self
            .tx_blocks
            .put_u8(&WriteOptions::new(), key.as_bytes(), &block.encode_to_vec())?)
    }

    pub fn tx_blocks_aux(&self) -> Result<impl Iterator<Item = Result<(String, u64)>> + '_> {
        kv_iter(&self.tx_blocks_aux, String::from_utf8, key_to_id)
    }

    pub fn get_tx_body(
        &self,
        epoch: u64,
        hash: H256,
    ) -> Result<Option<ProtoTransactionWithReceipt>> {
        let db_index = epoch / 250000;

        Ok(self
            .tx_bodies
            .db_at(db_index as usize)
            .get_u8(&ReadOptions::new(), hash.as_bytes())?
            .map(|tx| ProtoTransactionWithReceipt::decode(tx.as_slice()))
            .transpose()?)
    }

    pub fn put_tx_body(
        &self,
        epoch: u64,
        hash: H256,
        tx_body: ProtoTransactionWithReceipt,
    ) -> Result<()> {
        let db_index = epoch / 250000;

        Ok(self.tx_bodies.db_at(db_index as usize).put_u8(
            &WriteOptions::new(),
            hash.as_bytes(),
            &tx_body.encode_to_vec(),
        )?)
    }

    pub fn tx_bodies(
        &self,
    ) -> Result<impl Iterator<Item = Result<(H256, ProtoTransactionWithReceipt)>> + '_ + '_> {
        Ok(self.tx_bodies.iter().map(|(k, v)| {
            h256_from_be(k).map_err(Into::into).and_then(|k| {
                ProtoTransactionWithReceipt::decode(v.as_slice())
                    .map(|v| (k, v))
                    .map_err(Into::into)
            })
        }))
    }

    pub fn tx_epochs(&self) -> Result<impl Iterator<Item = Result<(H256, ProtoTxEpoch)>> + '_> {
        kv_iter(&self.tx_epochs, h256_from_be, |v| {
            ProtoTxEpoch::decode(v.as_slice())
        })
    }

    pub fn put_tx_epoch(&self, tx_hash: H256, epoch: ProtoTxEpoch) -> Result<()> {
        Ok(self.tx_epochs.put_u8(
            &WriteOptions::new(),
            tx_hash.as_bytes(),
            &epoch.encode_to_vec(),
        )?)
    }

    pub fn vc_blocks(&self) -> Result<impl Iterator<Item = Result<(H256, ProtoVcBlock)>> + '_> {
        kv_iter(&self.vc_blocks, h256_from_hex, |v| {
            ProtoVcBlock::decode(v.as_slice())
        })
    }
}

#[derive(Error, Debug)]
enum KeyToIdError {
    #[error("invalid utf-8")]
    Utf8(#[from] FromUtf8Error),
    #[error("invalid integer")]
    Int(#[from] ParseIntError),
}

fn key_to_id(key: Vec<u8>) -> Result<u64, KeyToIdError> {
    Ok(String::from_utf8(key)?.parse()?)
}

#[derive(Error, Debug)]
enum FromHexError {
    #[error("invalid utf-8")]
    Utf8(#[from] FromUtf8Error),
    #[error("invalid hex encoding")]
    Hex(#[from] rustc_hex::FromHexError),
}

fn h160_from_hex(hex: Vec<u8>) -> Result<H160, FromHexError> {
    Ok(String::from_utf8(hex)?.parse()?)
}

fn h256_from_hex(hex: Vec<u8>) -> Result<H256, FromHexError> {
    Ok(String::from_utf8(hex)?.parse()?)
}

fn h256_from_be(bytes: Vec<u8>) -> Result<H256, Infallible> {
    Ok(H256::from_slice(&bytes))
}

fn kv_iter<K, KM, KE, V, VM, VE>(
    db: &Database,
    mut key_mapper: KM,
    mut value_mapper: VM,
) -> Result<impl Iterator<Item = Result<(K, V)>> + '_>
where
    KM: FnMut(Vec<u8>) -> Result<K, KE> + 'static,
    VM: FnMut(Vec<u8>) -> Result<V, VE> + 'static,
    KE: std::error::Error + Send + Sync + 'static,
    VE: std::error::Error + Send + Sync + 'static,
{
    Ok(db.iter(&ReadOptions::new()).map(move |(k, v)| {
        key_mapper(k)
            .map_err(Into::into)
            .and_then(|k| value_mapper(v).map(|v| (k, v)).map_err(Into::into))
    }))
}

#[derive(Debug)]
pub struct ContractStateKey {
    pub addr: H160,
    pub parts: Vec<String>,
}

impl ContractStateKey {
    fn new(key: String) -> Result<Self> {
        let mut parts = key.split_terminator('\u{16}');
        // The first element will always be the contract address.
        let addr = parts
            .next()
            .ok_or_else(|| anyhow!("invalid contract state key"))?
            .parse()?;
        Ok(ContractStateKey {
            addr,
            parts: parts.map(|s| s.to_owned()).collect::<Vec<_>>(),
        })
    }
}

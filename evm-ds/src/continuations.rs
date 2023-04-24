use std::{
    collections::{BTreeMap, BTreeSet, HashMap},
    ops::Range,
};

use evm::{
    backend::Log,
    executor::stack::{MemoryStackAccount, MemoryStackSubstate},
    ExitReason, Memory, Stack, Valids,
};
use primitive_types::{H160, H256, U256};

pub struct Continuation {
    pub data: Vec<u8>,
    pub code: Vec<u8>,
    pub position: Result<usize, ExitReason>,
    pub return_range: Range<U256>,
    pub valids: Valids,
    pub memory: Memory,
    pub stack: Stack,
    pub logs: Vec<Log>,
    pub accounts: BTreeMap<H160, MemoryStackAccount>,
    pub storages: BTreeMap<(H160, H256), H256>,
    pub deletes: BTreeSet<H160>,
}

pub struct Continuations {
    storage: HashMap<u64, Continuation>,
    next_continuation_id: u64,
}

impl Continuations {
    pub fn new() -> Self {
        Self {
            storage: HashMap::new(),
            next_continuation_id: 0,
        }
    }

    pub fn create_continuation(
        &mut self,
        machine: &mut evm::Machine,
        substate: &MemoryStackSubstate,
    ) -> u64 {
        self.next_continuation_id += 1;
        let continuation = Continuation {
            data: machine.data(),
            code: machine.code(),
            position: machine.position().to_owned(),
            return_range: machine.return_range().clone(),
            valids: machine.valids().clone(),
            memory: machine.memory().clone(),
            stack: machine.stack().clone(),
            accounts: substate.accounts().clone(),
            logs: Vec::from(substate.logs()),
            storages: substate.storages().clone(),
            deletes: substate.deletes().clone(),
        };
        self.storage.insert(self.next_continuation_id, continuation);
        self.next_continuation_id
    }

    pub fn get_contination(&mut self, id: u64) -> Option<Continuation> {
        self.storage.remove(&id)
    }

    // Sometimes a contract will change the state of another contract
    // in this case, we need to find cached state of continuations that
    // has now been invalidated by this and update it
    pub fn update_states(&mut self, addr: H160, key: H256, value: H256, skip: bool)  {

        if skip {
            return;
        }

        // print states we are updating
        eprintln!("Updating states for {:?} {:?} {:?}", addr, key, value);

        // Loop over continuations updating the address if it exists
        for (_, continuation) in self.storage.iter_mut() {

            if let Some(value_current) = continuation.storages.get_mut(&(addr, key)) {
                *value_current = value;
            }
        }
    }
}

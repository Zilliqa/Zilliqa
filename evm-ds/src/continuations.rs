use std::{
    collections::HashMap,
    ops::Range,
    sync::{atomic::AtomicU64, Arc, Mutex},
};

use evm::{ExitReason, Memory, Stack, Valids};
use primitive_types::U256;

pub struct Continuation {
    pub data: Vec<u8>,
    pub code: Vec<u8>,
    pub position: Result<usize, ExitReason>,
    pub return_range: Range<U256>,
    pub valids: Valids,
    pub memory: Memory,
    pub stack: Stack,
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

    pub fn create_continuation(&mut self, machine: &mut evm::Machine) -> u64 {
        self.next_continuation_id += 1;
        let continuation = Continuation {
            data: machine.data(),
            code: machine.code(),
            position: machine.position().to_owned(),
            return_range: machine.return_range().clone(),
            valids: machine.valids().clone(),
            memory: machine.memory().clone(),
            stack: machine.stack().clone(),
        };
        self.storage.insert(self.next_continuation_id, continuation);
        self.next_continuation_id
    }

    pub fn get_contination(&mut self, id: u64) -> Option<Continuation> {
        let ret_val = self.storage.remove(&id);
        ret_val
    }
}

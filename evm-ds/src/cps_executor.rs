use std::collections::BTreeMap;

use crate::protos::Evm as EvmProto;
use core::cmp::min;
use evm::executor::stack::{
    MemoryStackState, PrecompileFailure, PrecompileOutput, PrecompileSet, StackExecutor,
    StackExecutorHandle, StackState,
};

use evm::{
    Capture, Config, Context, CreateScheme, ExitError, ExitReason, Handler, Opcode, Resolve,
    Runtime, Stack, Transfer,
};
use primitive_types::{H160, H256, U256};

use crate::scillabackend::ScillaBackend;
type PrecompileMap = BTreeMap<
    H160,
    fn(&[u8], Option<u64>, &Context, bool) -> Result<(PrecompileOutput, u64), PrecompileFailure>,
>;

pub struct CpsExecutor<'a> {
    stack_executor: StackExecutor<'a, 'a, MemoryStackState<'a, 'a, ScillaBackend>, PrecompileMap>,
    enable_cps: bool,
}

pub struct CpsCreateInterrupt {
    pub caller: H160,
    pub scheme: CreateScheme,
    pub create2_address: H160,
    pub value: U256,
    pub init_code: Vec<u8>,
    pub target_gas: Option<u64>,
}

pub struct CpsCreateFeedback {
    pub address: H160,
}

pub struct CpsCallInterrupt {
    pub code_address: H160,
    pub transfer: Option<Transfer>,
    pub input: Vec<u8>,
    pub target_gas: Option<u64>,
    pub is_static: bool,
    pub context: Context,
    pub memory_offset: U256,
    pub offset_len: U256,
}

pub struct CpsCallFeedback {}

pub enum CpsReason {
    NormalExit(evm::ExitReason),
    CallInterrupt(CpsCallInterrupt),
    CreateInterrupt(CpsCreateInterrupt),
}

impl<'a> CpsExecutor<'a> {
    /// Create a new stack-based executor with given precompiles.
    pub fn new_with_precompiles(
        state: MemoryStackState<'a, 'a, ScillaBackend>,
        config: &'a Config,
        precompile_set: &'a PrecompileMap,
        enable_cps: bool,
    ) -> Self {
        Self {
            stack_executor: StackExecutor::new_with_precompiles(state, config, precompile_set),
            enable_cps,
        }
    }

    /// Execute the runtime until it returns.
    pub fn execute(
        &mut self,
        runtime: &mut Runtime,
        feedback: Option<EvmProto::Continuation>,
    ) -> CpsReason {
        if self.enable_cps {
            if let Err(r) = self.apply_feedback(runtime, feedback) {
                return CpsReason::NormalExit(ExitReason::Fatal(evm::ExitFatal::CallErrorAsFatal(
                    r,
                )));
            }
        }
        match runtime.run(self) {
            Capture::Exit(s) => CpsReason::NormalExit(s),
            Capture::Trap(t) => match t {
                Resolve::Call(i, _) => CpsReason::CallInterrupt(i),
                Resolve::Create(i, _) => CpsReason::CreateInterrupt(i),
            },
        }
    }

    fn apply_feedback(
        &mut self,
        runtime: &mut Runtime,
        continuation_feedback: Option<EvmProto::Continuation>,
    ) -> Result<(), evm::ExitError> {
        if let Some(feedback) = feedback {
            // Sputnik places empty H256 value on a stack before returning with a trap
            // Hence, pop it before applying the feedback
            runtime.machine_mut().stack_mut().pop()?;
            if feedback.get_feedback_type() == EvmProto::Continuation_Type::CREATE {
                let eth_address = H160::from(feedback.get_address());
                runtime.machine_mut().stack_mut().push(eth_address.into())?;
            } else {
                *runtime.return_data_buffer() = Vec::from(feedback.get_calldata().get_data());
                let offset_len: U256 = U256::from(feedback.get_calldata().get_offset_len());
                let target_len = min(offset_len, U256::from(runtime.return_data_buffer().len()));

                match runtime.machine_mut().memory_mut().copy_large(
                    U256::from(feedback.get_calldata().get_memory_offset()),
                    U256::zero(),
                    target_len,
                    feedback.get_calldata().get_data(),
                ) {
                    Ok(()) => {
                        let mut value = H256::default();
                        let one = U256::one();
                        one.to_big_endian(&mut value[..]);
                        runtime.machine_mut().stack_mut().push(value)?;
                    }
                    Err(_) => {
                        let mut value = H256::default();
                        let zero = U256::zero();
                        zero.to_big_endian(&mut value[..]);
                        runtime.machine_mut().stack_mut().push(value)?;
                    }
                }
            }
        }
        Result::Ok(())
    }

    /// Get remaining gas.
    pub fn gas(&self) -> u64 {
        self.stack_executor.gas()
    }

    pub fn into_state(self) -> MemoryStackState<'a, 'a, ScillaBackend> {
        self.stack_executor.into_state()
    }
}

impl<'a> Handler for CpsExecutor<'a> {
    type CreateInterrupt = CpsCreateInterrupt;
    type CreateFeedback = CpsCreateFeedback;
    type CallInterrupt = CpsCallInterrupt;
    type CallFeedback = CpsCallFeedback;

    /// Get balance of address.
    fn balance(&self, address: H160) -> U256 {
        self.stack_executor.balance(address)
    }

    /// Get code size of address.
    fn code_size(&self, address: H160) -> U256 {
        self.stack_executor.code_size(address)
    }

    /// Get code hash of address.
    fn code_hash(&self, address: H160) -> H256 {
        self.stack_executor.code_hash(address)
    }

    /// Get code of address.
    fn code(&self, address: H160) -> Vec<u8> {
        self.stack_executor.code(address)
    }

    /// Get storage value of address at index.
    fn storage(&self, address: H160, index: H256) -> H256 {
        self.stack_executor.storage(address, index)
    }

    /// Get original storage value of address at index.
    fn original_storage(&self, address: H160, index: H256) -> H256 {
        self.stack_executor.original_storage(address, index)
    }

    /// Get the gas left value.
    fn gas_left(&self) -> U256 {
        self.stack_executor.gas_left()
    }

    /// Get the gas price value.
    fn gas_price(&self) -> U256 {
        self.stack_executor.gas_price()
    }

    /// Get execution origin.
    fn origin(&self) -> H160 {
        self.stack_executor.origin()
    }

    /// Get environmental block hash.
    fn block_hash(&self, number: U256) -> H256 {
        self.stack_executor.block_hash(number)
    }

    /// Get environmental block number.
    fn block_number(&self) -> U256 {
        self.stack_executor.block_number()
    }

    /// Get environmental coinbase.
    fn block_coinbase(&self) -> H160 {
        self.stack_executor.block_coinbase()
    }

    /// Get environmental block timestamp.
    fn block_timestamp(&self) -> U256 {
        self.stack_executor.block_timestamp()
    }

    /// Get environmental block difficulty.
    fn block_difficulty(&self) -> U256 {
        self.stack_executor.block_difficulty()
    }

    /// Get environmental gas limit.
    fn block_gas_limit(&self) -> U256 {
        self.stack_executor.block_gas_limit()
    }

    /// Environmental block base fee.
    fn block_base_fee_per_gas(&self) -> U256 {
        self.stack_executor.block_base_fee_per_gas()
    }

    /// Get environmental chain ID.
    fn chain_id(&self) -> U256 {
        self.stack_executor.chain_id()
    }

    /// Check whether an address exists.
    fn exists(&self, address: H160) -> bool {
        self.stack_executor.exists(address)
    }

    /// Check whether an address has already been deleted.
    fn deleted(&self, address: H160) -> bool {
        self.stack_executor.deleted(address)
    }

    /// Checks if the address or (address, index) pair has been previously accessed
    /// (or set in `accessed_addresses` / `accessed_storage_keys` via an access list
    /// transaction).
    /// References:
    /// * https://eips.ethereum.org/EIPS/eip-2929
    /// * https://eips.ethereum.org/EIPS/eip-2930
    fn is_cold(&self, address: H160, index: Option<H256>) -> bool {
        self.stack_executor.is_cold(address, index)
    }

    /// Set storage value of address at index.
    fn set_storage(&mut self, address: H160, index: H256, value: H256) -> Result<(), ExitError> {
        self.stack_executor.set_storage(address, index, value)
    }

    /// Create a log owned by address with given topics and data.
    fn log(&mut self, address: H160, topics: Vec<H256>, data: Vec<u8>) -> Result<(), ExitError> {
        self.stack_executor.log(address, topics, data)
    }

    /// Mark an address to be deleted, with funds transferred to target.
    fn mark_delete(&mut self, address: H160, target: H160) -> Result<(), ExitError> {
        self.stack_executor.mark_delete(address, target)
    }

    /// Invoke a create operation.
    fn create(
        &mut self,
        caller: H160,
        scheme: CreateScheme,
        value: U256,
        init_code: Vec<u8>,
        target_gas: Option<u64>,
    ) -> Capture<(ExitReason, Option<H160>, Vec<u8>), Self::CreateInterrupt> {
        if self.enable_cps {
            return Capture::Trap(Self::CreateInterrupt {
                caller,
                scheme,
                create2_address: self.stack_executor.create_address(scheme),
                value,
                init_code,
                target_gas,
            });
        }

        let result =
            self.stack_executor
                .create(caller, scheme, value, init_code.clone(), target_gas);

        // Repack StackExecutor::CreateInterrupt to CpsExecutor::CreateInterrupt
        match result {
            Capture::Exit(s) => Capture::Exit(s),
            Capture::Trap(_) => Capture::Trap(Self::CreateInterrupt {
                caller,
                scheme,
                create2_address: self.stack_executor.create_address(scheme),
                value,
                init_code,
                target_gas,
            }),
        }
    }

    /// Invoke a call operation.
    fn call(
        &mut self,
        code_address: H160,
        transfer: Option<Transfer>,
        input: Vec<u8>,
        target_gas: Option<u64>,
        is_static: bool,
        context: Context,
        memory_offset: U256,
        offset_len: U256,
    ) -> Capture<(ExitReason, Vec<u8>), Self::CallInterrupt> {
        if self.enable_cps {
            // See StackExecutor precompiles handling
            // TODO: allow precompile to return with a Trap
            if let Some(result) =
                self.stack_executor
                    .precompiles()
                    .execute(&mut StackExecutorHandle {
                        executor: &mut self.stack_executor,
                        code_address,
                        input: &input,
                        gas_limit: target_gas,
                        context: &context,
                        is_static,
                    })
            {
                return match result {
                    Ok(PrecompileOutput {
                        exit_status,
                        output,
                    }) => Capture::Exit((ExitReason::Succeed(exit_status), output)),
                    Err(PrecompileFailure::Error { exit_status }) => {
                        Capture::Exit((ExitReason::Error(exit_status), Vec::new()))
                    }
                    Err(PrecompileFailure::Revert {
                        exit_status,
                        output,
                    }) => Capture::Exit((ExitReason::Revert(exit_status), output)),
                    Err(PrecompileFailure::Fatal { exit_status }) => {
                        self.stack_executor
                            .state_mut()
                            .metadata_mut()
                            .gasometer_mut()
                            .fail();
                        Capture::Exit((ExitReason::Fatal(exit_status), Vec::new()))
                    }
                };
            }

            return Capture::Trap(Self::CallInterrupt {
                code_address,
                transfer,
                input,
                target_gas,
                is_static,
                context,
                memory_offset,
                offset_len,
            });
        }

        let result = self.stack_executor.call(
            code_address,
            transfer.clone(),
            input.clone(),
            target_gas,
            is_static,
            context.clone(),
            memory_offset,
            offset_len,
        );
        // Repack StackExecutor::CallInterrupt to CpsExecutor::CallInterrupt
        match result {
            Capture::Exit(s) => Capture::Exit(s),
            Capture::Trap(_) => Capture::Trap(Self::CallInterrupt {
                code_address,
                transfer,
                input,
                target_gas,
                is_static,
                context,
                memory_offset,
                offset_len,
            }),
        }
    }

    /// Pre-validation step for the runtime.
    #[inline]
    fn pre_validate(
        &mut self,
        context: &Context,
        opcode: Opcode,
        stack: &Stack,
    ) -> Result<(), ExitError> {
        self.stack_executor.pre_validate(context, opcode, stack)
    }
}

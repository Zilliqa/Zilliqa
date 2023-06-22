use std::ops::Deref;
use std::panic::{self, AssertUnwindSafe};
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use evm::backend::Backend;
use evm::executor::stack::MemoryStackSubstate;
use evm::{
    backend::Apply,
    executor::stack::{MemoryStackState, StackSubstateMetadata},
    CreateScheme, Handler,
};
use evm::{Machine, Runtime};
use serde::{Deserialize, Serialize};

use log::{debug, error, info};

use jsonrpc_core::Result;
use primitive_types::*;
use scillabackend::{encode_storage, scale_eth_to_zil, ScillaBackend};

use crate::continuations::Continuations;
use crate::cps_executor::{CpsCallInterrupt, CpsCreateInterrupt, CpsExecutor, CpsReason};
use crate::precompiles::get_precompiles;
use crate::pretty_printer::log_evm_result;
use crate::protos::Evm as EvmProto;
use crate::protos::Evm::EvmResult;
use crate::scillabackend;
use protobuf::Message;

#[allow(clippy::too_many_arguments)]
pub async fn run_evm_impl(
    address: H160,
    code: Vec<u8>,
    data: Vec<u8>,
    apparent_value: U256,
    gas_limit: u64,
    caller: H160,
    backend: ScillaBackend,
    gas_scaling_factor: u64,
    estimate: bool,
    is_static: bool,
    evm_context: String,
    node_continuation: Option<EvmProto::Continuation>,
    continuations: Arc<Mutex<Continuations>>,
    enable_cps: bool,
    tx_trace_enabled: bool,
    tx_trace: String,
) -> Result<String> {
    // We must spawn a separate blocking task (on a blocking thread), because by default a JSONRPC
    // method runs as a non-blocking thread under a tokio runtime, and creating a new runtime
    // cannot be done. And we'll need a new runtime that we can safely drop on a handled
    // panic. (Using the parent runtime and dropping on stack unwind will mess up the parent runtime).
    tokio::task::spawn_blocking(move || {
        let result = run_evm_impl_direct(
            address,
            code,
            data,
            apparent_value,
            gas_limit,
            caller,
            gas_scaling_factor,
            backend.config.zil_scaling_factor,
            backend,
            estimate,
            is_static,
            evm_context,
            node_continuation,
            continuations,
            enable_cps,
            tx_trace_enabled,
            tx_trace,
            true,
        );

        Ok(base64::encode(result.write_to_bytes().unwrap()))
    })
    .await
    .unwrap()
}

#[allow(clippy::too_many_arguments)]
fn build_exit_result<B: Backend>(
    executor: CpsExecutor<B>,
    runtime: &Runtime,
    _backend: &impl Backend,
    trace: &LoggingEventListener,
    exit_reason: &evm::ExitReason,
    remaining_gas: u64,
    is_static: bool,
    continuations: Arc<Mutex<Continuations>>,
    perform_scaling: bool,
    scaling_factor: u64,
) -> EvmProto::EvmResult {
    let mut result = EvmProto::EvmResult::new();
    result.set_exit_reason(exit_reason.clone().into());
    result.set_return_value(runtime.machine().return_value().into());
    let (state_apply, logs) = executor.into_state().deconstruct();

    result.set_apply(
        state_apply
            .into_iter()
            .map(|apply| {
                let mut result = EvmProto::Apply::new();
                match apply {
                    Apply::Delete { address } => {
                        let mut delete = EvmProto::Apply_Delete::new();
                        delete.set_address(address.into());
                        result.set_delete(delete);
                    }
                    Apply::Modify {
                        address,
                        basic,
                        code,
                        storage,
                        reset_storage,
                    } => {
                        let mut modify = EvmProto::Apply_Modify::new();
                        modify.set_address(address.into());
                        if perform_scaling {
                            modify.set_balance(
                                scale_eth_to_zil(basic.balance, scaling_factor).into(),
                            ); // todo
                        }
                        modify.set_nonce(basic.nonce.into());
                        if let Some(code) = code {
                            modify.set_code(code.into());
                        }
                        modify.set_reset_storage(reset_storage);

                        // Is this call static? if so, we don't want to modify other continuations' state
                        let storage_proto = storage
                            .into_iter()
                            .map(|(k, v)| {
                                continuations
                                    .lock()
                                    .unwrap()
                                    .update_states(address, k, v, is_static);
                                encode_storage(k, v, perform_scaling).into()
                            })
                            .collect();

                        modify.set_storage(storage_proto);
                        result.set_modify(modify);
                    }
                };
                result
            })
            .collect(),
    );
    result.set_tx_trace(trace.as_string().into());
    result.set_logs(logs.into_iter().map(Into::into).collect());
    result.set_remaining_gas(remaining_gas);
    result
}

#[allow(clippy::too_many_arguments)]
fn build_call_result<B: Backend>(
    executor: CpsExecutor<B>,
    runtime: &Runtime,
    _backend: &impl Backend,
    interrupt: CpsCallInterrupt,
    trace: &LoggingEventListener,
    remaining_gas: u64,
    is_static: bool,
    cont_id: u64,
    perform_scaling: bool,
    scaling_factor: u64,
) -> EvmProto::EvmResult {
    let mut result = EvmProto::EvmResult::new();
    result.set_return_value(runtime.machine().return_value().into());
    let mut trap_reason = EvmProto::ExitReason_Trap::new();
    trap_reason.set_kind(EvmProto::ExitReason_Trap_Kind::CALL);
    let mut exit_reason = EvmProto::ExitReason::new();

    let (state_apply, _) = executor.into_state().deconstruct();

    // We need to apply the changes made to the state so subsequent calls can
    // see the changes.
    result.set_apply(
        state_apply
            .into_iter()
            .map(|apply| {
                let mut result = EvmProto::Apply::new();
                match apply {
                    Apply::Delete { address } => {
                        let mut delete = EvmProto::Apply_Delete::new();
                        delete.set_address(address.into());
                        result.set_delete(delete);
                    }
                    Apply::Modify {
                        address,
                        basic,
                        code,
                        storage,
                        reset_storage,
                    } => {
                        debug!("Modify: {:?} {:?}", address, basic);
                        let mut modify = EvmProto::Apply_Modify::new();
                        modify.set_address(address.into());
                        if perform_scaling {
                            modify.set_balance(
                                scale_eth_to_zil(basic.balance, scaling_factor).into(),
                            );
                        }
                        modify.set_nonce(basic.nonce.into());
                        if let Some(code) = code {
                            modify.set_code(code.into());
                        }
                        modify.set_reset_storage(reset_storage);
                        let storage_proto = storage
                            .into_iter()
                            .map(|(k, v)| encode_storage(k, v, perform_scaling).into())
                            .collect();
                        modify.set_storage(storage_proto);
                        result.set_modify(modify);
                    }
                };
                result
            })
            .collect(),
    );

    exit_reason.set_trap(trap_reason);
    result.set_exit_reason(exit_reason);
    result.set_tx_trace(trace.as_string().into());
    result.set_remaining_gas(remaining_gas);

    let mut trap_data_call = EvmProto::TrapData_Call::new();

    let mut context = EvmProto::TrapData_Context::new();
    context.set_apparent_value(interrupt.context.apparent_value.into());
    context.set_caller(interrupt.context.caller.into());
    context.set_destination(interrupt.context.address.into());

    trap_data_call.set_context(context);

    if let Some(tran) = interrupt.transfer {
        let mut transfer = EvmProto::TrapData_Transfer::new();
        transfer.set_destination(tran.target.into());
        transfer.set_source(tran.source.into());
        transfer.set_value(tran.value.into());
        trap_data_call.set_transfer(transfer);
    }

    trap_data_call.set_callee_address(interrupt.code_address.into());
    trap_data_call.set_call_data(interrupt.input.into());
    trap_data_call.set_is_static(interrupt.is_static || is_static);
    trap_data_call.set_is_precompile(interrupt.is_precompile);
    trap_data_call.set_target_gas(interrupt.target_gas.unwrap_or(u64::MAX));
    trap_data_call.set_memory_offset(interrupt.memory_offset.into());
    trap_data_call.set_offset_len(interrupt.offset_len.into());

    let mut trap_data = EvmProto::TrapData::new();
    trap_data.set_call(trap_data_call);
    result.set_trap_data(trap_data);
    result.set_continuation_id(cont_id);
    result
}

fn build_create_result(
    runtime: &Runtime,
    interrupt: CpsCreateInterrupt,
    trace: &LoggingEventListener,
    remaining_gas: u64,
    cont_id: u64,
) -> EvmProto::EvmResult {
    let mut result = EvmProto::EvmResult::new();

    result.set_return_value(runtime.machine().return_value().into());
    let mut trap_reason = EvmProto::ExitReason_Trap::new();
    trap_reason.set_kind(EvmProto::ExitReason_Trap_Kind::CREATE);
    let mut exit_reason = EvmProto::ExitReason::new();
    exit_reason.set_trap(trap_reason);
    result.set_exit_reason(exit_reason);
    result.set_tx_trace(trace.as_string().into());
    result.set_remaining_gas(remaining_gas);

    let mut scheme = EvmProto::TrapData_Scheme::new();

    match interrupt.scheme {
        evm::CreateScheme::Legacy { caller } => {
            let mut scheme_legacy = EvmProto::TrapData_Scheme_Legacy::new();
            scheme_legacy.set_caller(caller.into());
            scheme.set_legacy(scheme_legacy);
        }
        evm::CreateScheme::Create2 {
            caller,
            code_hash,
            salt,
        } => {
            let mut scheme_create2 = EvmProto::TrapData_Scheme_Create2::new();
            scheme_create2.set_caller(caller.into());
            scheme_create2.set_code_hash(code_hash.into());
            scheme_create2.set_salt(salt.into());
            scheme_create2.set_create2_address(interrupt.create2_address.into());
            scheme.set_create2(scheme_create2);
        }
        evm::CreateScheme::Fixed(address) => {
            let mut scheme_fixed = EvmProto::TrapData_Scheme_Fixed::new();
            scheme_fixed.set_addres(address.into());
            scheme.set_fixed(scheme_fixed);
        }
    }
    let mut trap_data_create = EvmProto::TrapData_Create::new();
    trap_data_create.set_scheme(scheme);
    trap_data_create.set_caller(interrupt.caller.into());
    trap_data_create.set_call_data(interrupt.init_code.into());
    trap_data_create.set_target_gas(interrupt.target_gas.unwrap_or(u64::MAX));
    trap_data_create.set_value(interrupt.value.into());
    let mut trap_data = EvmProto::TrapData::new();
    trap_data.set_create(trap_data_create);
    result.set_trap_data(trap_data);
    result.set_continuation_id(cont_id);
    result
}

fn handle_panic(trace: String, remaining_gas: u64, reason: &str) -> EvmProto::EvmResult {
    let mut result = EvmProto::EvmResult::new();
    let mut fatal = EvmProto::ExitReason_Fatal::new();
    fatal.set_error_string(reason.into());
    let mut exit_reason = EvmProto::ExitReason::new();
    exit_reason.set_fatal(fatal);
    result.set_exit_reason(exit_reason);
    result.set_tx_trace(trace.into());
    result.set_remaining_gas(remaining_gas);
    result
}

// Convenience fn to hide the evm internals and just
// let you calculate contract address as easily as possible
#[allow(dead_code)]
pub fn calculate_contract_address(address: H160, backend: impl Backend) -> H160 {
    let config = evm::Config {
        estimate: false,
        call_l64_after_gas: false,
        ..evm::Config::london()
    };

    let metadata = StackSubstateMetadata::new(1, &config);
    let state = MemoryStackState::new(metadata, &backend);
    let precompiles = get_precompiles();

    let mut executor = CpsExecutor::new_with_precompiles(state, &config, &precompiles, true);
    executor.get_create_address(CreateScheme::Legacy { caller: address })
}

#[allow(clippy::too_many_arguments, dead_code)]
pub fn run_evm_impl_direct(
    address: H160,
    code: Vec<u8>,
    data: Vec<u8>,
    apparent_value: U256,
    gas_limit: u64,
    caller: H160,
    gas_scaling_factor: u64,
    scaling_factor: u64,
    backend: impl Backend,
    estimate: bool,
    is_static: bool,
    evm_context: String,
    node_continuation: Option<EvmProto::Continuation>,
    continuations: Arc<Mutex<Continuations>>,
    enable_cps: bool,
    tx_trace_enabled: bool,
    tx_trace: String,
    perform_scaling: bool,
) -> EvmResult {
    info!(
        "Running EVM: origin: {:?} address: {:?} gas: {:?} value: {:?}  estimate: {:?} is_continuation: {:?}, cps: {:?}, \ntx_trace: {:?}, \ndata: {:02X?}, \ncode: {:02X?}",
        backend.origin(), address, gas_limit, apparent_value,
        estimate, node_continuation.is_none(), enable_cps, tx_trace, data, code);
    let code = Rc::new(code);
    let data = Rc::new(data);
    // TODO: handle call_l64_after_gas problem: https://zilliqa-jira.atlassian.net/browse/ZIL-5012
    // todo: this needs to be shanghai...
    let config = evm::Config {
        estimate,
        call_l64_after_gas: false,
        ..evm::Config::london()
    };
    let context = evm::Context {
        address,
        caller,
        apparent_value,
    };
    let gas_limit = gas_limit * gas_scaling_factor;
    let metadata = StackSubstateMetadata::new(gas_limit, &config);
    // Check if evm should resume from the point it stopped
    let (feedback_continuation, mut runtime, state) = if let Some(continuation) = node_continuation
    {
        let recorded_cont = continuations
            .lock()
            .unwrap()
            .get_contination(continuation.get_id());
        if recorded_cont.is_none() {
            let result = handle_panic(tx_trace, gas_limit, "Continuation not found!");
            return result;
        }

        let recorded_cont = recorded_cont.unwrap();

        let machine = Machine::create_from_state(
            Rc::new(recorded_cont.code),
            Rc::new(recorded_cont.data),
            recorded_cont.position,
            recorded_cont.return_range,
            recorded_cont.valids,
            recorded_cont.memory,
            recorded_cont.stack,
        );
        let runtime = Runtime::new_from_state(machine, context, &config);
        let memory_substate = MemoryStackSubstate::from_state(
            metadata,
            recorded_cont.logs,
            recorded_cont.accounts,
            recorded_cont.storages,
            recorded_cont.deletes,
        );
        let state = MemoryStackState::new_with_substate(memory_substate, &backend);
        (Some(continuation), runtime, state)
    } else {
        let runtime = evm::Runtime::new(code, data.clone(), context, &config);
        let state = MemoryStackState::new(metadata, &backend);
        (None, runtime, state)
    };
    // Scale the gas limit.

    let precompiles = get_precompiles();

    let mut executor = CpsExecutor::new_with_precompiles(state, &config, &precompiles, enable_cps);

    let mut listener;

    if tx_trace.is_empty() {
        listener = LoggingEventListener::new(tx_trace_enabled);
    } else {
        listener = serde_json::from_str(&tx_trace).unwrap()
    }

    // If there is no continuation, we need to push our call context on,
    // Otherwise, our call context is loaded and is last element in stack
    if feedback_continuation.is_none() {
        let mut call = CallContext::new();
        call.call_type = "CALL".to_string();
        call.value = format!("0x{apparent_value}");
        call.gas = format!("0x{gas_limit:x}"); // Gas provided for call
        call.input = format!("0x{}", hex::encode(data.deref()));

        if listener.call_tracer.is_empty() {
            call.from = format!("{:?}", backend.origin());
        } else {
            call.from = listener.call_tracer.last().unwrap().to.clone();
        }

        call.to = format!("{address:?}");
        listener.push_call(call);
    }

    // We have to catch panics, as error handling in the Backend interface of
    // do not have Result, assuming all operations are successful.
    //
    // We are asserting it is safe to unwind, as objects will be dropped after
    // the unwind.
    let executor_result = panic::catch_unwind(AssertUnwindSafe(|| {
        evm::runtime::tracing::using(&mut listener, || {
            executor.execute(&mut runtime, feedback_continuation)
        })
    }));

    // Scale back remaining gas to Scilla units (no rounding!).
    let remaining_gas = executor.gas() / gas_scaling_factor;

    // Update the traces
    listener.raw_tracer.return_value = hex::encode(runtime.machine().return_value());
    listener.raw_tracer.gas = gas_limit - remaining_gas;
    if !listener.call_tracer.is_empty() {
        listener.call_tracer.last_mut().unwrap().gas_used =
            format!("0x{:x}", gas_limit - remaining_gas);
        listener.call_tracer.last_mut().unwrap().output =
            format!("0x{}", hex::encode(runtime.machine().return_value()));
    }

    if let Err(panic) = executor_result {
        let panic_message = panic
            .downcast::<String>()
            .unwrap_or_else(|_| Box::new("unknown panic".to_string()));
        error!("EVM panicked: '{:?}'", panic_message);
        let result = handle_panic(listener.as_string(), remaining_gas, &panic_message);
        return result;
    }

    let cps_result = executor_result.unwrap();

    let result = match cps_result {
        CpsReason::NormalExit(exit_reason) => {
            // Normal exit, we finished call.
            listener.finished_call();

            match exit_reason {
                evm::ExitReason::Revert(_) => {
                    listener.otter_transaction_error =
                        format!("0x{}", hex::encode(runtime.machine().return_value()));
                }
                _ => {
                    debug!(
                        "Machine: position: {:?}, memory: {:?}, stack: {:?}",
                        runtime.machine().position(),
                        &runtime
                            .machine()
                            .memory()
                            .data()
                            .iter()
                            .take(128)
                            .collect::<Vec<_>>(),
                        &runtime
                            .machine()
                            .stack()
                            .data()
                            .iter()
                            .take(128)
                            .collect::<Vec<_>>()
                    );
                }
            }

            build_exit_result(
                executor,
                &runtime,
                &backend,
                &listener,
                &exit_reason,
                remaining_gas,
                is_static,
                continuations,
                perform_scaling,
                scaling_factor,
            )
        }
        CpsReason::CallInterrupt(i) => {
            let cont_id = continuations
                .lock()
                .unwrap()
                .create_continuation(runtime.machine_mut(), executor.state().substate());

            build_call_result(
                executor,
                &runtime,
                &backend,
                i,
                &listener,
                remaining_gas,
                is_static,
                cont_id,
                perform_scaling,
                scaling_factor,
            )
        }
        CpsReason::CreateInterrupt(i) => {
            let cont_id = continuations
                .lock()
                .unwrap()
                .create_continuation(runtime.machine_mut(), executor.into_state().substate());

            build_create_result(&runtime, i, &listener, remaining_gas, cont_id)
        }
    };

    info!(
        "EVM execution summary: context: {:?}, origin: {:?} address: {:?} gas: {:?} value: {:?}, data: {:?}, estimate: {:?}, cps: {:?}, result: {}, returnVal: {}",
        evm_context, backend.origin(), address, gas_limit, apparent_value,
        hex::encode(data.deref()),
        estimate, enable_cps, log_evm_result(&result), hex::encode(runtime.machine().return_value()));

    result
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct CallContext {
    #[serde(rename = "type")]
    pub call_type: String, // only 'call' (not create, delegate, static)
    pub from: String,
    pub to: String,
    pub value: String,
    pub gas: String,
    #[serde(rename = "gasUsed")]
    pub gas_used: String,
    pub input: String,
    pub output: String,

    pub calls: Vec<CallContext>,
}

impl CallContext {
    #[allow(dead_code)]
    pub fn new() -> Self {
        CallContext {
            call_type: Default::default(),
            from: Default::default(),
            to: Default::default(),
            value: Default::default(),
            gas: "0x0".to_string(),
            gas_used: "0x0".to_string(),
            input: Default::default(),
            output: Default::default(),
            calls: Default::default(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct OtterscanCallContext {
    #[serde(rename = "type")]
    pub call_type: String,
    pub depth: usize,
    pub from: String,
    pub to: String,
    pub value: String,
    pub input: String,
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct StructLog {
    pub depth: usize,
    pub error: String,
    pub gas: u64, // not populated
    #[serde(rename = "gasCost")]
    pub gas_cost: u64, // not populated
    pub op: String,
    pub pc: usize,
    pub stack: Vec<String>,
    pub storage: Vec<String>, // not populated
}

// This implementation has a stack of call contexts each with reference to their calls - so a tree is
// Created in this way.
// Each new call gets added to the end of the stack and becomes the current context.
// On returning from a call, the end of the stack gets put into the item above's calls
#[derive(Debug, Serialize, Deserialize)]
pub struct LoggingEventListener {
    pub call_tracer: Vec<CallContext>,
    pub raw_tracer: StructLogTopLevel,
    pub otter_internal_tracer: Vec<InternalOperationOtter>,
    pub otter_call_tracer: Vec<OtterscanCallContext>,
    pub otter_transaction_error: String,
    pub otter_addresses_called: Vec<String>,
    pub enabled: bool,
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct StructLogTopLevel {
    pub gas: u64,
    #[serde(rename = "returnValue")]
    pub return_value: String,
    #[serde(rename = "structLogs")]
    pub struct_logs: Vec<StructLog>,
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct InternalOperationOtter {
    #[serde(rename = "type")]
    pub call_type: usize,
    pub from: String,
    pub to: String,
    pub value: String,
}

impl LoggingEventListener {
    pub fn new(enabled: bool) -> Self {
        LoggingEventListener {
            call_tracer: Default::default(),
            raw_tracer: Default::default(),
            otter_internal_tracer: Default::default(),
            otter_call_tracer: Default::default(),
            otter_transaction_error: "0x".to_string(),
            otter_addresses_called: Default::default(),
            enabled,
        }
    }
}

impl evm::runtime::tracing::EventListener for LoggingEventListener {
    fn event(&mut self, event: evm::runtime::tracing::Event) {
        if !self.enabled {
            return;
        }

        let call_depth = self.call_tracer.len() - 1;

        let mut struct_log = StructLog {
            depth: call_depth,
            ..Default::default()
        };

        let mut intern_trace = None;

        match event {
            evm::runtime::tracing::Event::Step {
                context: _,
                opcode,
                position,
                stack,
                memory: _,
            } => {
                struct_log.op = format!("{opcode}");
                struct_log.pc = position.clone().unwrap_or(0);

                for sta in stack.data() {
                    struct_log.stack.push(format!("{sta:?}"));
                }
            }
            evm::runtime::tracing::Event::StepResult {
                result,
                return_value: _,
            } => {
                struct_log.op = "StepResult".to_string();
                struct_log.error = format!("{:?}", result.clone());
            }
            evm::runtime::tracing::Event::SLoad {
                address: _,
                index: _,
                value: _,
            } => {
                struct_log.op = "Sload".to_string();
            }
            evm::runtime::tracing::Event::SStore {
                address: _,
                index: _,
                value: _,
            } => {
                struct_log.op = "SStore".to_string();
            }
            evm::runtime::tracing::Event::TransactTransfer {
                call_type,
                address,
                target,
                balance,
                input,
            } => {
                intern_trace = Some(InternalOperationOtter {
                    call_type: call_depth,
                    from: format!("{:?}", address),
                    to: format!("{:?}", target),
                    value: format!("{:0X?}", balance),
                });

                self.otter_call_tracer.push(OtterscanCallContext {
                    call_type: call_type.to_string(),
                    depth: call_depth,
                    from: format!("{:?}", address),
                    to: format!("{:?}", target),
                    value: format!("{:0X?}", balance),
                    input: input.to_string(),
                });

                let to_add = format!("{:?}", target);

                // only push if doesn't exist in otter_addresses_called
                if !self.otter_addresses_called.contains(&to_add) {
                    self.otter_addresses_called.push(to_add);
                }
            }
            evm::runtime::tracing::Event::TransactSuicide {
                address,
                target,
                balance,
            } => {
                intern_trace = Some(InternalOperationOtter {
                    call_type: 1,
                    from: format!("{:?}", address),
                    to: format!("{:?}", target),
                    value: format!("{:0X?}", balance),
                });

                self.otter_call_tracer.push(OtterscanCallContext {
                    call_type: "SELFDESTRUCT".to_string(),
                    depth: call_depth,
                    from: format!("{:?}", address),
                    to: format!("{:?}", target),
                    value: format!("{:0X?}", balance),
                    input: "".to_string(),
                });
            }
            evm::runtime::tracing::Event::TransactCreate {
                call_type,
                address,
                target,
                balance,
                is_create2,
                input,
            } => {
                intern_trace = Some(InternalOperationOtter {
                    call_type: if is_create2 { 3 } else { 2 },
                    from: format!("{:?}", address),
                    to: format!("{:?}", target),
                    value: format!("{:0X?}", balance),
                });

                self.otter_call_tracer.push(OtterscanCallContext {
                    call_type: call_type.to_string(),
                    depth: call_depth,
                    from: format!("{:?}", address),
                    to: format!("{:?}", target),
                    value: format!("{:0X?}", balance),
                    input: input.to_string(),
                });

                let to_add = format!("{:?}", target);

                // only push if doesn't exist in otter_addresses_called
                if !self.otter_addresses_called.contains(&to_add) {
                    self.otter_addresses_called.push(to_add);
                }
            }
        }

        if self.raw_tracer.struct_logs.len() < 5 {
            self.raw_tracer.struct_logs.push(struct_log);
        }

        if let Some(intern_trace) = intern_trace {
            self.otter_internal_tracer.push(intern_trace);
        }
    }
}

impl LoggingEventListener {
    #[allow(dead_code)]
    pub fn as_string(&self) -> String {
        serde_json::to_string(self).unwrap()
    }

    #[allow(dead_code)]
    pub fn as_string_pretty(&self) -> String {
        serde_json::to_string_pretty(self).unwrap()
    }

    pub fn finished_call(&mut self) {
        // The call has now completed - adjust the stack if neccessary
        if self.call_tracer.len() > 1 {
            let end = self.call_tracer.pop().unwrap();
            let new_end = self.call_tracer.last_mut().unwrap();
            new_end.calls.push(end);
        }
    }

    pub fn push_call(&mut self, context: CallContext) {
        // Now we have constructed our new call context, it gets added to the end of
        // the stack (if we want to do tracing)
        if self.enabled {
            self.call_tracer.push(context);
        }
    }
}

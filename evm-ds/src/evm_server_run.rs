use std::ops::Deref;
use std::panic::{self, AssertUnwindSafe};
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use evm::executor::stack::MemoryStackSubstate;
use evm::{
    backend::Apply,
    executor::stack::{MemoryStackState, StackSubstateMetadata},
};
use evm::{Machine, Runtime};
use crate::CallContext;

use log::{debug, error, info};

use jsonrpc_core::Result;
use primitive_types::*;
use scillabackend::ScillaBackend;

use crate::continuations::Continuations;
use crate::cps_executor::{CpsCallInterrupt, CpsCreateInterrupt, CpsExecutor, CpsReason};
use crate::precompiles::get_precompiles;
use crate::pretty_printer::log_evm_result;
use crate::protos::Evm as EvmProto;
use crate::{scillabackend, LoggingEventListener};
use protobuf::Message;

#[allow(clippy::too_many_arguments)]
pub async fn run_evm_impl(
    address: H160,
    code: Vec<u8>,
    data: Vec<u8>,
    apparent_value: U256,
    gas_limit: u64,
    backend: ScillaBackend,
    gas_scaling_factor: u64,
    estimate: bool,
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
        info!(
            "Running EVM: origin: {:?} address: {:?} gas: {:?} value: {:?}  extras: {:?}, estimate: {:?}, cps: {:?}, tx_trace: {:?}, data: {:?}",
            backend.origin, address, gas_limit, apparent_value,
            backend.extras, estimate, enable_cps, tx_trace, data);
        let code = Rc::new(code);
        let data = Rc::new(data);
        // TODO: handle call_l64_after_gas problem: https://zilliqa-jira.atlassian.net/browse/ZIL-5012
        let config = evm::Config { estimate, call_l64_after_gas: false, ..evm::Config::london()};
        let context = evm::Context {
            address,
            caller: backend.origin,
            apparent_value,
        };
        let gas_limit = gas_limit * gas_scaling_factor;
        let metadata = StackSubstateMetadata::new(gas_limit, &config);
        // Check if evm should resume from the point it stopped
        let (feedback_continuation, mut runtime, state) =
        if let Some(continuation) = node_continuation {
            let recorded_cont = continuations.lock().unwrap().get_contination(continuation.get_id());
            if recorded_cont.is_none() {
                let result = handle_panic(tx_trace, gas_limit, "Continuation not found!");
                return Ok(base64::encode(result.write_to_bytes().unwrap()));
            }
            let recorded_cont = recorded_cont.unwrap();
            let machine = Machine::create_from_state(Rc::new(recorded_cont.code), Rc::new(recorded_cont.data),
                                                              recorded_cont.position, recorded_cont.return_range, recorded_cont.valids,
                                                              recorded_cont.memory, recorded_cont.stack);
            let runtime = Runtime::new_from_state(machine, context, &config);
            let memory_substate = MemoryStackSubstate::from_state(metadata, recorded_cont.logs, recorded_cont.accounts,
                recorded_cont.storages, recorded_cont.deletes);
            let state = MemoryStackState::new_with_substate(memory_substate, &backend);
            (Some(continuation), runtime, state)
        }
        else {
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
                call.from = format!("{:?}", backend.origin);
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
        evm::runtime::tracing::using(&mut listener, || executor.execute(&mut runtime, feedback_continuation))
        }));

        // Scale back remaining gas to Scilla units (no rounding!).
        let remaining_gas = executor.gas() / gas_scaling_factor;

        // Update the traces
        listener.raw_tracer.return_value = hex::encode(runtime.machine().return_value());
        listener.raw_tracer.gas = gas_limit - remaining_gas;
        listener.call_tracer.last_mut().unwrap().gas_used = format!("0x{:x}", gas_limit - remaining_gas);
        listener.call_tracer.last_mut().unwrap().output = format!("0x{}", hex::encode(runtime.machine().return_value()));

        if let Err(panic) = executor_result {
            let panic_message = panic
                    .downcast::<String>()
                    .unwrap_or_else(|_| Box::new("unknown panic".to_string()));
                error!("EVM panicked: '{:?}'", panic_message);
            let result = handle_panic(listener.as_string(), remaining_gas, &panic_message);
            return Ok(base64::encode(result.write_to_bytes().unwrap()));
        }

        let cps_result = executor_result.unwrap();

        let result = match cps_result {
            CpsReason::NormalExit(exit_reason) => {
                // Normal exit, we finished call.
                listener.finished_call();

                match exit_reason {
                    evm::ExitReason::Succeed(_) => {}
                    _ => {
                        debug!("Machine: position: {:?}, memory: {:?}, stack: {:?}",
                               runtime.machine().position(),
                               &runtime.machine().memory().data().iter().take(128).collect::<Vec<_>>(),
                               &runtime.machine().stack().data().iter().take(128).collect::<Vec<_>>());
                    }
                }
                build_exit_result(executor, &runtime, &backend, &listener, &exit_reason, remaining_gas)
            },
            CpsReason::CallInterrupt(i) => {
                let cont_id = continuations.lock().unwrap().create_continuation(runtime.machine_mut(), executor.into_state().substate());
                
                build_call_result(&runtime, i, &listener, remaining_gas, cont_id)
            },
            CpsReason::CreateInterrupt(i) => {
                let cont_id = continuations.lock().unwrap().create_continuation(runtime.machine_mut(), executor.into_state().substate());
                
                build_create_result(&runtime, i, &listener, remaining_gas, cont_id)
            }
        };
        info!(
            "EVM execution summary: context: {:?}, origin: {:?} address: {:?} gas: {:?} value: {:?}, data: {:?}, extras: {:?}, estimate: {:?}, cps: {:?}, result: {}, returnVal: {}",
            evm_context, backend.origin, address, gas_limit, apparent_value,
            hex::encode(data.deref()),
            backend.extras, estimate, enable_cps, log_evm_result(&result), hex::encode(runtime.machine().return_value()));
        Ok(base64::encode(result.write_to_bytes().unwrap()))
    })
    .await
    .unwrap()
}

fn build_exit_result(
    executor: CpsExecutor,
    runtime: &Runtime,
    backend: &ScillaBackend,
    trace: &LoggingEventListener,
    exit_reason: &evm::ExitReason,
    remaining_gas: u64,
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
                        debug!("Modify: {:?} {:?}", address, basic);
                        let mut modify = EvmProto::Apply_Modify::new();
                        modify.set_address(address.into());
                        modify.set_balance(backend.scale_eth_to_zil(basic.balance).into());
                        modify.set_nonce(basic.nonce.into());
                        if let Some(code) = code {
                            modify.set_code(code.into());
                        }
                        modify.set_reset_storage(reset_storage);
                        let storage_proto = storage
                            .into_iter()
                            .map(|(k, v)| backend.encode_storage(k, v).into())
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

fn build_call_result(
    runtime: &Runtime,
    interrupt: CpsCallInterrupt,
    trace: &LoggingEventListener,
    remaining_gas: u64,
    cont_id: u64,
) -> EvmProto::EvmResult {
    let mut result = EvmProto::EvmResult::new();
    result.set_return_value(runtime.machine().return_value().into());
    let mut trap_reason = EvmProto::ExitReason_Trap::new();
    trap_reason.set_kind(EvmProto::ExitReason_Trap_Kind::CALL);
    let mut exit_reason = EvmProto::ExitReason::new();
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
    trap_data_call.set_is_static(interrupt.is_static);
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

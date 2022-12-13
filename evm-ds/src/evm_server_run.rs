use std::panic::{self, AssertUnwindSafe};
use std::rc::Rc;

use evm::Runtime;
use evm::{
    backend::Apply,
    executor::stack::{MemoryStackState, StackSubstateMetadata},
};

use log::{debug, error, info};

use jsonrpc_core::Result;
use primitive_types::*;
use scillabackend::ScillaBackend;

use crate::cps_executor::{CpsExecutor, CpsReason};
use crate::precompiles::get_precompiles;
use crate::protos::Evm as EvmProto;
use crate::{scillabackend, LoggingEventListener};
use protobuf::Message;

pub async fn run_evm_impl(
    address: H160,
    code: Vec<u8>,
    data: Vec<u8>,
    apparent_value: U256,
    gas_limit: u64,
    backend: ScillaBackend,
    tracing: bool,
    gas_scaling_factor: u64,
    estimate: bool,
    evm_context: String,
) -> Result<String> {
    // We must spawn a separate blocking task (on a blocking thread), because by default a JSONRPC
    // method runs as a non-blocking thread under a tokio runtime, and creating a new runtime
    // cannot be done. And we'll need a new runtime that we can safely drop on a handled
    // panic. (Using the parent runtime and dropping on stack unwind will mess up the parent runtime).
    tokio::task::spawn_blocking(move || {
        debug!(
            "Running EVM: origin: {:?} address: {:?} gas: {:?} value: {:?} code: {:?} data: {:?}, extras: {:?}, estimate: {:?}",
            backend.origin, address, gas_limit, apparent_value, hex::encode(&code), hex::encode(&data),
            backend.extras, estimate);
        let code = Rc::new(code);
        let data = Rc::new(data);
        // TODO: handle call_l64_after_gas problem: https://zilliqa-jira.atlassian.net/browse/ZIL-5012
        let config = evm::Config { estimate, call_l64_after_gas: false, ..evm::Config::london()};
        let context = evm::Context {
            address,
            caller: backend.origin,
            apparent_value,
        };
        let mut runtime = evm::Runtime::new(code.clone(), data.clone(), context, &config);
        // Scale the gas limit.
        let gas_limit = gas_limit * gas_scaling_factor;
        let metadata = StackSubstateMetadata::new(gas_limit, &config);
        let state = MemoryStackState::new(metadata, &backend);

        let precompiles = get_precompiles();

        //let mut executor =
          //  evm::executor::stack::StackExecutor::new_with_precompiles(state, &config, &precompiles);

        let mut executor = CpsExecutor::new_with_precompiles(state, &config, &precompiles);

        let mut listener = LoggingEventListener{traces : Default::default()};

        // We have to catch panics, as error handling in the Backend interface of
        // do not have Result, assuming all operations are successful.
        //
        // We are asserting it is safe to unwind, as objects will be dropped after
        // the unwind.
        let executor_result = panic::catch_unwind(AssertUnwindSafe(|| {
            if tracing {
                evm::tracing::using(&mut listener, || executor.execute(&mut runtime))
            } else {
                executor.execute(&mut runtime)
            }
        }));

        // Scale back remaining gas to Scilla units (no rounding!).
        let remaining_gas = executor.gas() / gas_scaling_factor;

        if let Err(panic) = executor_result {
            let panic_message = panic
                    .downcast::<String>()
                    .unwrap_or_else(|_| Box::new("unknown panic".to_string()));
                error!("EVM panicked: '{:?}'", panic_message);
                let mut result = EvmProto::EvmResult::new();
                let mut fatal = EvmProto::ExitReason_Fatal::new();
                fatal.set_kind(EvmProto::ExitReason_Fatal_Kind::OTHER);
                let mut exit_reason = EvmProto::ExitReason::new();
                exit_reason.set_fatal(fatal);
                result.set_exit_reason(exit_reason);
                result.set_trace(listener.traces.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                return Ok(base64::encode(result.write_to_bytes().unwrap()));
        }

        let cps_result = executor_result.unwrap();


        let result = match cps_result {
            CpsReason::NormalExit(exit_reason) => {
                match exit_reason {
                    evm::ExitReason::Succeed(_) => {}
                    _ => {
                        debug!("Machine: position: {:?}, memory: {:?}, stack: {:?}",
                               runtime.machine().position(),
                               &runtime.machine().memory().data().iter().take(128).collect::<Vec<_>>(),
                               &runtime.machine().stack().data().iter().take(128).collect::<Vec<_>>());
                    }
                }
                let mut result = EvmProto::EvmResult::new();
                result.set_exit_reason(exit_reason.into());
                result.set_return_value(runtime.machine().return_value().into());
                let (state_apply, logs) = executor.into_state().deconstruct();
                result.set_apply(state_apply
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
                                             let storage_proto = storage.into_iter().map(
                                                 |(k, v)| backend.encode_storage(k, v).into()).collect();
                                             modify.set_storage(storage_proto);
                                             result.set_modify(modify);
                                         }
                                     };
                                   result
                                 })
                                 .collect());
                result.set_trace(listener.traces.into_iter().map(Into::into).collect());
                result.set_logs(logs.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                info!(
                    "EVM execution summary: context: {:?}, origin: {:?} address: {:?} gas: {:?} value: {:?} code: {:?} data: {:?}, extras: {:?}, estimate: {:?}, result: {:?}", evm_context,
                    backend.origin, address, gas_limit, apparent_value, hex::encode(code.as_ref()), hex::encode(data.as_ref()),
                    backend.extras, estimate, result);
                result
            },
            CpsReason::Other => {
                let mut result = EvmProto::EvmResult::new();
                let mut fatal = EvmProto::ExitReason_Fatal::new();
                fatal.set_kind(EvmProto::ExitReason_Fatal_Kind::OTHER);
                let mut exit_reason = EvmProto::ExitReason::new();
                exit_reason.set_fatal(fatal);
                result.set_exit_reason(exit_reason);
                result.set_trace(listener.traces.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                result
            }
        };
        Ok(base64::encode(result.write_to_bytes().unwrap()))
    })
    .await
    .unwrap()
}

fn build_result(
    executor: CpsExecutor,
    runtime: &Runtime,
    backend: &ScillaBackend,
    listener: LoggingEventListener,
    exit_reason: evm::ExitReason,
    remaining_gas: u64,
) -> EvmProto::EvmResult {
    let mut result = EvmProto::EvmResult::new();
    result.set_exit_reason(exit_reason.into());
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
    result.set_trace(listener.traces.into_iter().map(Into::into).collect());
    result.set_logs(logs.into_iter().map(Into::into).collect());
    result.set_remaining_gas(remaining_gas);
    result
}

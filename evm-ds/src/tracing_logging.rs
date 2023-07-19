use serde::{Deserialize, Serialize};

// File to keep all of the misc logging and tracing code
// made when calling the evm

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
                    call_type: 0,
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

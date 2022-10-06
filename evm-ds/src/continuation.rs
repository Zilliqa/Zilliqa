/// A Continuation captures the computation that could be done in the future.
#[derive(Default)]
pub struct Continuation {
    pub context: Option<evm::Context>,
    pub memory: Option<evm::Memory>,
    pub stack: Option<evm::Stack>,
}

impl Continuation {
    /// Default continuation that does nothing.
    pub fn none() -> Self {
        Self::default()
    }

    pub fn take_context(&mut self) -> Option<evm::Context> {
        self.context.take()
    }
}

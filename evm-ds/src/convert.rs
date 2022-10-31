use crate::protos::Evm;
use byteorder::{BigEndian, ByteOrder};
use primitive_types::*;
use std::ops::Shr;

impl From<H160> for Evm::Address {
    fn from(address: H160) -> Self {
        let address = address.as_fixed_bytes();
        let mut addr = Evm::Address::new();
        addr.set_x0(BigEndian::read_u32(&address[0..4]));
        addr.set_x1(BigEndian::read_u64(&address[4..12]));
        addr.set_x2(BigEndian::read_u64(&address[12..20]));
        addr
    }
}

impl From<U256> for Evm::UInt256 {
    fn from(num: U256) -> Self {
        let mut result = Self::new();
        result.set_x3(num.low_u64());
        result.set_x2(num.shr(8).low_u64());
        result.set_x1(num.shr(16).low_u64());
        result.set_x0(num.shr(24).low_u64());
        result
    }
}

impl From<H256> for Evm::H256 {
    fn from(h: H256) -> Self {
        let mut result = Self::new();
        let buf = h.as_fixed_bytes();
        result.set_x0(BigEndian::read_u64(&buf[0..8]));
        result.set_x1(BigEndian::read_u64(&buf[8..16]));
        result.set_x2(BigEndian::read_u64(&buf[16..24]));
        result.set_x3(BigEndian::read_u64(&buf[24..32]));
        result
    }
}

impl From<ethereum::Log> for Evm::EvmLog {
    fn from(log: ethereum::Log) -> Self {
        let mut evm_log = Evm::EvmLog::new();
        evm_log.set_address(log.address.into());
        evm_log.set_topics(log.topics.into_iter().map(Into::into).collect());
        evm_log.set_data(log.data.into());
        evm_log
    }
}

impl From<evm::ExitSucceed> for Evm::ExitSucceed {
    fn from(arg: evm::ExitSucceed) -> Self {
        match arg {
            evm::ExitSucceed::Stopped => Evm::ExitSucceed::STOPPED,
            evm::ExitSucceed::Returned => Evm::ExitSucceed::RETURNED,
            evm::ExitSucceed::Suicided => Evm::ExitSucceed::SUICIDED,
        }
    }
}

impl From<evm::ExitError> for Evm::ExitError {
    fn from(arg: evm::ExitError) -> Self {
        let mut result = Self::new();
        match arg {
            evm::ExitError::StackUnderflow => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_STACK_UNDERFLOW);
            }
            evm::ExitError::StackOverflow => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_STACK_OVERFLOW);
            }
            evm::ExitError::InvalidJump => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_INVALID_JUMP);
            }
            evm::ExitError::InvalidRange => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_INVALID_RANGE);
            }
            evm::ExitError::DesignatedInvalid => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_DESIGNATED_INVALID);
            }
            evm::ExitError::CallTooDeep => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_CALL_TOO_DEEP);
            }
            evm::ExitError::CreateCollision => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_CREATE_COLLISION);
            }
            evm::ExitError::CreateContractLimit => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_CREATE_CONTRACT_LIMIT);
            }
            evm::ExitError::InvalidCode => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_INVALID_CODE);
            }
            evm::ExitError::OutOfOffset => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_OUT_OF_OFFSET);
            }
            evm::ExitError::OutOfGas => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_OUT_OF_GAS);
            }
            evm::ExitError::OutOfFund => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_OUT_OF_FUND);
            }
            evm::ExitError::PCUnderflow => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_PC_UNDERFLOW);
            }
            evm::ExitError::CreateEmpty => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_CREATE_EMPTY);
            }
            evm::ExitError::Other(error_string) => {
                result.set_kind(Evm::ExitErrorKind::EXIT_ERROR_PC_UNDERFLOW);
                result.set_error_string(error_string.into_owned().into());
            }
        }
        result
    }
}

impl From<evm::ExitFatal> for Evm::ExitFatal {
    fn from(arg: evm::ExitFatal) -> Self {
        let mut result = Self::new();
        match arg {
            evm::ExitFatal::NotSupported => {
                result.set_kind(Evm::ExitFatalKind::EXIT_FATAL_NOT_SUPPORTED);
            }
            evm::ExitFatal::UnhandledInterrupt => {
                result.set_kind(Evm::ExitFatalKind::EXIT_FATAL_UNHANDLED_INTERRUPT);
            }
            evm::ExitFatal::CallErrorAsFatal(error) => {
                result.set_kind(Evm::ExitFatalKind::EXIT_FATAL_CALL_ERROR_AS_FATAL);
                result.set_error(error.into());
            }
            evm::ExitFatal::Other(error_string) => {
                result.set_kind(Evm::ExitFatalKind::EXIT_FATAL_OTHER);
                result.set_error_string(error_string.into_owned().into());
            }
        }
        result
    }
}

impl From<evm::ExitReason> for Evm::ExitReason {
    fn from(exit_reason: evm::ExitReason) -> Self {
        let mut result = Self::new();
        match exit_reason {
            evm::ExitReason::Succeed(arg) => result.set_succeed(arg.into()),
            evm::ExitReason::Error(arg) => result.set_error(arg.into()),
            evm::ExitReason::Revert(_) => result.set_revert(Evm::ExitRevert::EXIT_REVERTED),
            evm::ExitReason::Fatal(arg) => result.set_fatal(arg.into()),
        }
        result
    }
}

impl From<(H256, H256)> for Evm::Storage {
    fn from(storage: (H256, H256)) -> Self {
        let mut result = Evm::Storage::new();
        result.set_key(storage.0.into());
        result.set_value(storage.1.into());
        result
    }
}

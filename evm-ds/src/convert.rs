use crate::protos::Evm as EvmProto;
use byteorder::{BigEndian, ByteOrder};
use bytes::Bytes;
use primitive_types::*;
use std::ops::Shr;

impl From<H160> for EvmProto::Address {
    fn from(address: H160) -> Self {
        let address = address.as_fixed_bytes();
        let mut addr = EvmProto::Address::new();
        addr.set_x0(BigEndian::read_u32(&address[0..4]));
        addr.set_x1(BigEndian::read_u64(&address[4..12]));
        addr.set_x2(BigEndian::read_u64(&address[12..20]));
        addr
    }
}

impl From<&EvmProto::Address> for H160 {
    fn from(address: &EvmProto::Address) -> Self {
        let mut buf = [0u8; 20];
        BigEndian::write_u32(&mut buf[0..4], address.x0);
        BigEndian::write_u64(&mut buf[4..12], address.x1);
        BigEndian::write_u64(&mut buf[12..20], address.x2);
        H160::from_slice(&buf)
    }
}

impl From<U256> for EvmProto::UInt256 {
    fn from(num: U256) -> Self {
        let mut result = Self::new();
        result.set_x3(num.low_u64());
        result.set_x2(num.shr(64).low_u64());
        result.set_x1(num.shr(128).low_u64());
        result.set_x0(num.shr(192).low_u64());
        result
    }
}

impl From<&EvmProto::UInt256> for U256 {
    fn from(num: &EvmProto::UInt256) -> Self {
        let mut buf = [0u8; 32];
        BigEndian::write_u64(&mut buf[0..8], num.x0);
        BigEndian::write_u64(&mut buf[8..16], num.x1);
        BigEndian::write_u64(&mut buf[16..24], num.x2);
        BigEndian::write_u64(&mut buf[24..32], num.x3);
        U256::from_big_endian(&buf)
    }
}

impl From<H256> for EvmProto::H256 {
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

impl From<ethereum::Log> for EvmProto::EvmLog {
    fn from(log: ethereum::Log) -> Self {
        let mut evm_log = EvmProto::EvmLog::new();
        evm_log.set_address(log.address.into());
        evm_log.set_topics(log.topics.into_iter().map(Into::into).collect());
        evm_log.set_data(log.data.into());
        evm_log
    }
}

impl From<evm::ExitSucceed> for EvmProto::ExitReason_Succeed {
    fn from(arg: evm::ExitSucceed) -> Self {
        match arg {
            evm::ExitSucceed::Stopped => EvmProto::ExitReason_Succeed::STOPPED,
            evm::ExitSucceed::Returned => EvmProto::ExitReason_Succeed::RETURNED,
            evm::ExitSucceed::Suicided => EvmProto::ExitReason_Succeed::SUICIDED,
        }
    }
}

impl From<evm::ExitError> for EvmProto::ExitReason_Error {
    fn from(arg: evm::ExitError) -> Self {
        let mut result = Self::new();
        match arg {
            evm::ExitError::StackUnderflow => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::STACK_UNDERFLOW);
            }
            evm::ExitError::StackOverflow => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::STACK_OVERFLOW);
            }
            evm::ExitError::InvalidJump => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::INVALID_JUMP);
            }
            evm::ExitError::InvalidRange => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::INVALID_RANGE);
            }
            evm::ExitError::DesignatedInvalid => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::DESIGNATED_INVALID);
            }
            evm::ExitError::CallTooDeep => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::CALL_TOO_DEEP);
            }
            evm::ExitError::CreateCollision => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::CREATE_COLLISION);
            }
            evm::ExitError::CreateContractLimit => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::CREATE_CONTRACT_LIMIT);
            }
            evm::ExitError::InvalidCode(opcode) => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::INVALID_CODE);
            }
            evm::ExitError::OutOfOffset => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::OUT_OF_OFFSET);
            }
            evm::ExitError::OutOfGas => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::OUT_OF_GAS);
            }
            evm::ExitError::OutOfFund => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::OUT_OF_FUND);
            }
            evm::ExitError::PCUnderflow => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::PC_UNDERFLOW);
            }
            evm::ExitError::CreateEmpty => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::CREATE_EMPTY);
            }
            evm::ExitError::Other(error_string) => {
                result.set_kind(EvmProto::ExitReason_Error_Kind::PC_UNDERFLOW);
                result.set_error_string(error_string.into_owned().into());
            }
        }
        result
    }
}

impl From<evm::ExitFatal> for EvmProto::ExitReason_Fatal {
    fn from(arg: evm::ExitFatal) -> Self {
        let mut result = Self::new();
        match arg {
            evm::ExitFatal::NotSupported => {
                result.set_kind(EvmProto::ExitReason_Fatal_Kind::NOT_SUPPORTED);
            }
            evm::ExitFatal::UnhandledInterrupt => {
                result.set_kind(EvmProto::ExitReason_Fatal_Kind::UNHANDLED_INTERRUPT);
            }
            evm::ExitFatal::CallErrorAsFatal(error) => {
                result.set_kind(EvmProto::ExitReason_Fatal_Kind::CALL_ERROR_AS_FATAL);
                result.set_error(error.into());
            }
            evm::ExitFatal::Other(error_string) => {
                result.set_kind(EvmProto::ExitReason_Fatal_Kind::OTHER);
                result.set_error_string(error_string.into_owned().into());
            }
        }
        result
    }
}

impl From<evm::ExitReason> for EvmProto::ExitReason {
    fn from(exit_reason: evm::ExitReason) -> Self {
        let mut result = Self::new();
        match exit_reason {
            evm::ExitReason::Succeed(arg) => result.set_succeed(arg.into()),
            evm::ExitReason::Error(arg) => result.set_error(arg.into()),
            evm::ExitReason::Revert(_) => result.set_revert(EvmProto::ExitReason_Revert::REVERTED),
            evm::ExitReason::Fatal(arg) => result.set_fatal(arg.into()),
        }
        result
    }
}

impl From<(Bytes, Bytes)> for EvmProto::Storage {
    fn from(storage: (Bytes, Bytes)) -> Self {
        let mut result = EvmProto::Storage::new();
        result.set_key(storage.0);
        result.set_value(storage.1);
        result
    }
}

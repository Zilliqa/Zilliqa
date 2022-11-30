use crate::protos::Evm as EvmProto;
use byteorder::{BigEndian, ByteOrder};
use bytes::Bytes;
use primitive_types::*;
use std::ops::Shr;
use evm::backend::{Log};
use evm::Opcode;

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

impl From<Log> for EvmProto::EvmLog {
    fn from(log: Log) -> Self {
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
                result.set_error_string(as_string(&opcode).into());
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

fn as_string(opcode: &Opcode) -> String {
    match opcode {
        &Opcode::STOP => "STOP".to_string(),
        &Opcode::ADD => "ADD".to_string(),
        &Opcode::MUL => "MUL".to_string(),
        &Opcode::SUB => "SUB".to_string(),
        &Opcode::DIV => "DIV".to_string(),
        &Opcode::SDIV => "SDIV".to_string(),
        &Opcode::MOD => "MOD".to_string(),
        &Opcode::SMOD => "SMOD".to_string(),
        &Opcode::ADDMOD => "ADDMOD".to_string(),
        &Opcode::MULMOD => "MULMOD".to_string(),
        &Opcode::EXP => "EXP".to_string(),
        &Opcode::SIGNEXTEND => "SIGNEXTEND".to_string(),
        &Opcode::LT => "LT".to_string(),
        &Opcode::GT => "GT".to_string(),
        &Opcode::SLT => "SLT".to_string(),
        &Opcode::SGT => "SGT".to_string(),
        &Opcode::EQ => "EQ".to_string(),
        &Opcode::ISZERO => "ISZERO".to_string(),
        &Opcode::AND => "AND".to_string(),
        &Opcode::OR => "OR".to_string(),
        &Opcode::XOR => "XOR".to_string(),
        &Opcode::NOT => "NOT".to_string(),
        &Opcode::BYTE => "BYTE".to_string(),
        &Opcode::CALLDATALOAD => "CALLDATALOAD".to_string(),
        &Opcode::CALLDATASIZE => "CALLDATASIZE".to_string(),
        &Opcode::CALLDATACOPY => "CALLDATACOPY".to_string(),
        &Opcode::CODESIZE => "CODESIZE".to_string(),
        &Opcode::CODECOPY => "CODECOPY".to_string(),
        &Opcode::SHL => "SHL".to_string(),
        &Opcode::SHR => "SHR".to_string(),
        &Opcode::SAR => "SAR".to_string(),
        &Opcode::POP => "POP".to_string(),
        &Opcode::MLOAD => "MLOAD".to_string(),
        &Opcode::MSTORE => "MSTORE".to_string(),
        &Opcode::MSTORE8 => "MSTORE8".to_string(),
        &Opcode::JUMP => "JUMP".to_string(),
        &Opcode::JUMPI => "JUMPI".to_string(),
        &Opcode::PC => "PC".to_string(),
        &Opcode::MSIZE => "MSIZE".to_string(),
        &Opcode::JUMPDEST => "JUMPDEST".to_string(),
        &Opcode::PUSH1 => "PUSH1".to_string(),
        &Opcode::PUSH2 => "PUSH2".to_string(),
        &Opcode::PUSH3 => "PUSH3".to_string(),
        &Opcode::PUSH4 => "PUSH4".to_string(),
        &Opcode::PUSH5 => "PUSH5".to_string(),
        &Opcode::PUSH6 => "PUSH6".to_string(),
        &Opcode::PUSH7 => "PUSH7".to_string(),
        &Opcode::PUSH8 => "PUSH8".to_string(),
        &Opcode::PUSH9 => "PUSH9".to_string(),
        &Opcode::PUSH10 => "PUSH10".to_string(),
        &Opcode::PUSH11 => "PUSH11".to_string(),
        &Opcode::PUSH12 => "PUSH12".to_string(),
        &Opcode::PUSH13 => "PUSH13".to_string(),
        &Opcode::PUSH14 => "PUSH14".to_string(),
        &Opcode::PUSH15 => "PUSH15".to_string(),
        &Opcode::PUSH16 => "PUSH16".to_string(),
        &Opcode::PUSH17 => "PUSH17".to_string(),
        &Opcode::PUSH18 => "PUSH18".to_string(),
        &Opcode::PUSH19 => "PUSH19".to_string(),
        &Opcode::PUSH20 => "PUSH20".to_string(),
        &Opcode::PUSH21 => "PUSH21".to_string(),
        &Opcode::PUSH22 => "PUSH22".to_string(),
        &Opcode::PUSH23 => "PUSH23".to_string(),
        &Opcode::PUSH24 => "PUSH24".to_string(),
        &Opcode::PUSH25 => "PUSH25".to_string(),
        &Opcode::PUSH26 => "PUSH26".to_string(),
        &Opcode::PUSH27 => "PUSH27".to_string(),
        &Opcode::PUSH28 => "PUSH28".to_string(),
        &Opcode::PUSH29 => "PUSH29".to_string(),
        &Opcode::PUSH30 => "PUSH30".to_string(),
        &Opcode::PUSH31 => "PUSH31".to_string(),
        &Opcode::PUSH32 => "PUSH32".to_string(),
        &Opcode::DUP1 => "DUP1".to_string(),
        &Opcode::DUP2 => "DUP2".to_string(),
        &Opcode::DUP3 => "DUP3".to_string(),
        &Opcode::DUP4 => "DUP4".to_string(),
        &Opcode::DUP5 => "DUP5".to_string(),
        &Opcode::DUP6 => "DUP6".to_string(),
        &Opcode::DUP7 => "DUP7".to_string(),
        &Opcode::DUP8 => "DUP8".to_string(),
        &Opcode::DUP9 => "DUP9".to_string(),
        &Opcode::DUP10 => "DUP10".to_string(),
        &Opcode::DUP11 => "DUP11".to_string(),
        &Opcode::DUP12 => "DUP12".to_string(),
        &Opcode::DUP13 => "DUP13".to_string(),
        &Opcode::DUP14 => "DUP14".to_string(),
        &Opcode::DUP15 => "DUP15".to_string(),
        &Opcode::DUP16 => "DUP16".to_string(),
        &Opcode::SWAP1 => "SWAP1".to_string(),
        &Opcode::SWAP2 => "SWAP2".to_string(),
        &Opcode::SWAP3 => "SWAP3".to_string(),
        &Opcode::SWAP4 => "SWAP4".to_string(),
        &Opcode::SWAP5 => "SWAP5".to_string(),
        &Opcode::SWAP6 => "SWAP6".to_string(),
        &Opcode::SWAP7 => "SWAP7".to_string(),
        &Opcode::SWAP8 => "SWAP8".to_string(),
        &Opcode::SWAP9 => "SWAP9".to_string(),
        &Opcode::SWAP10 => "SWAP10".to_string(),
        &Opcode::SWAP11 => "SWAP11".to_string(),
        &Opcode::SWAP12 => "SWAP12".to_string(),
        &Opcode::SWAP13 => "SWAP13".to_string(),
        &Opcode::SWAP14 => "SWAP14".to_string(),
        &Opcode::SWAP15 => "SWAP15".to_string(),
        &Opcode::SWAP16 => "SWAP16".to_string(),
        &Opcode::RETURN => "RETURN".to_string(),
        &Opcode::REVERT => "REVERT".to_string(),
        &Opcode::INVALID => "INVALID".to_string(),
        &Opcode::EOFMAGIC => "EOFMAGIC".to_string(),
        &Opcode::SHA3 => "SHA3".to_string(),
        &Opcode::ADDRESS => "ADDRESS".to_string(),
        &Opcode::BALANCE => "BALANCE".to_string(),
        &Opcode::SELFBALANCE => "SELFBALANCE".to_string(),
        &Opcode::BASEFEE => "BASEFEE".to_string(),
        &Opcode::ORIGIN => "ORIGIN".to_string(),
        &Opcode::CALLER => "CALLER".to_string(),
        &Opcode::CALLVALUE => "CALLVALUE".to_string(),
        &Opcode::GASPRICE => "GASPRICE".to_string(),
        &Opcode::EXTCODESIZE => "EXTCODESIZE".to_string(),
        &Opcode::EXTCODECOPY => "EXTCODECOPY".to_string(),
        &Opcode::EXTCODEHASH => "EXTCODEHASH".to_string(),
        &Opcode::RETURNDATASIZE => "RETURNDATASIZE".to_string(),
        &Opcode::RETURNDATACOPY => "RETURNDATACOPY".to_string(),
        &Opcode::BLOCKHASH => "BLOCKHASH".to_string(),
        &Opcode::COINBASE => "COINBASE".to_string(),
        &Opcode::TIMESTAMP => "TIMESTAMP".to_string(),
        &Opcode::NUMBER => "NUMBER".to_string(),
        &Opcode::DIFFICULTY => "DIFFICULTY".to_string(),
        &Opcode::GASLIMIT => "GASLIMIT".to_string(),
        &Opcode::SLOAD => "SLOAD".to_string(),
        &Opcode::SSTORE => "SSTORE".to_string(),
        &Opcode::GAS => "GAS".to_string(),
        &Opcode::LOG0 => "LOG0".to_string(),
        &Opcode::LOG1 => "LOG1".to_string(),
        &Opcode::LOG2 => "LOG2".to_string(),
        &Opcode::LOG3 => "LOG3".to_string(),
        &Opcode::LOG4 => "LOG4".to_string(),
        &Opcode::CREATE => "CREATE".to_string(),
        &Opcode::CREATE2 => "CREATE2".to_string(),
        &Opcode::CALL => "CALL".to_string(),
        &Opcode::CALLCODE => "CALLCODE".to_string(),
        &Opcode::DELEGATECALL => "DELEGATECALL".to_string(),
        &Opcode::STATICCALL => "STATICCALL".to_string(),
        &Opcode::SUICIDE => "SUICIDE".to_string(),
        &Opcode::CHAINID => "CHAINID".to_string(),
        _ => format!("UNKNOWNOP: {:?}", opcode),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_address_to_proto() {
        let addr: H160 = "000102030405060708090A0B0C0D0E0F10111213".parse().unwrap();
        let addr_proto: EvmProto::Address = addr.into();
        assert_eq!(addr_proto.x0, 0x00010203);
        assert_eq!(addr_proto.x1, 0x0405060708090A0B);
        assert_eq!(addr_proto.x2, 0x0C0D0E0F10111213);
    }

    #[test]
    fn test_proto_to_address() {
        let mut addr_proto = EvmProto::Address::new();
        addr_proto.set_x0(0x00010203);
        addr_proto.set_x1(0x0405060708090A0B);
        addr_proto.set_x2(0x0C0D0E0F10111213);
        let addr: H160 = (&addr_proto).into();
        assert_eq!(
            addr,
            "000102030405060708090A0B0C0D0E0F10111213".parse().unwrap(),
        );
    }

    #[test]
    fn test_proto_to_u256() {
        let mut u256_proto = EvmProto::UInt256::new();
        u256_proto.set_x0(0x0001020304050607);
        u256_proto.set_x1(0x08090a0b0c0d0e0f);
        u256_proto.set_x2(0x1011121314151617);
        u256_proto.set_x3(0x18191a1b1c1d1e1f);
        let result: U256 = (&u256_proto).into();
        assert_eq!(
            result,
            "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
                .parse()
                .unwrap()
        );
    }

    #[test]
    fn test_u256_to_proto() {
        let input: U256 = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
            .parse()
            .unwrap();
        let u256_proto: EvmProto::UInt256 = input.into();
        assert_eq!(u256_proto.x0, 0x0001020304050607);
        assert_eq!(u256_proto.x1, 0x08090a0b0c0d0e0f);
        assert_eq!(u256_proto.x2, 0x1011121314151617);
        assert_eq!(u256_proto.x3, 0x18191a1b1c1d1e1f);
    }
}

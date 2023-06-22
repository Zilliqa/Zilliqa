extern crate protoc_rust;
use std::path::Path;

fn main() {
    let mut scilla_message_proto = "../src/libPersistence/ScillaMessage.proto";
    let mut evm_proto = "../src/libUtils/Evm.proto";
    let mut scilla_message_proto_inc = "../src/libPersistence/";
    let mut evm_proto_inc = "../src/libUtils/";

    // If we have the files local to this dir, go ahead
    if Path::new("ScillaMessage.proto").is_file() && Path::new("Evm.proto").is_file() {
        scilla_message_proto = "ScillaMessage.proto";
        evm_proto = "Evm.proto";
        scilla_message_proto_inc = "./";
        evm_proto_inc = "./";
    }

    protoc_rust::Codegen::new()
        .out_dir("src/protos")
        .inputs([scilla_message_proto])
        .include(scilla_message_proto_inc)
        .customize(protoc_rust::Customize {
            carllerche_bytes_for_bytes: Some(true),
            carllerche_bytes_for_string: Some(true),
            ..Default::default()
        })
        .run()
        .expect("Running protoc failed for ScillaMessage.proto");

    protoc_rust::Codegen::new()
        .out_dir("src/protos")
        .inputs([evm_proto])
        .include(evm_proto_inc)
        .customize(protoc_rust::Customize {
            carllerche_bytes_for_bytes: Some(true),
            carllerche_bytes_for_string: Some(true),
            ..Default::default()
        })
        .run()
        .expect("Running protoc failed for EVM.proto");

    println!("cargo:rerun-if-changed={}", scilla_message_proto);
    println!("cargo:rerun-if-changed={}", evm_proto);
}

extern crate protoc_rust;

fn main() {
    protoc_rust::Codegen::new()
        .out_dir("src/protos")
        .inputs(&["../src/libPersistence/ScillaMessage.proto"])
        .include("../src/libPersistence")
        .customize(protoc_rust::Customize {
            carllerche_bytes_for_bytes: Some(true),
            carllerche_bytes_for_string: Some(true),
            ..Default::default()
        })
        .run()
        .expect("Running protoc failed for ScillaMessage.proto");

    protoc_rust::Codegen::new()
        .out_dir("src/protos")
        .inputs(&["../src/libUtils/Evm.proto"])
        .include("../src/libUtils")
        .customize(protoc_rust::Customize {
            carllerche_bytes_for_bytes: Some(true),
            carllerche_bytes_for_string: Some(true),
            ..Default::default()
        })
        .run()
        .expect("Running protoc faile for EVM.proto");
}

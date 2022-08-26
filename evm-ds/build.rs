extern crate protoc_rust;

fn main() {
    protoc_rust::Codegen::new()
        .out_dir("src/protos")
        .inputs(&["protos/ScillaMessage.proto"])
        .include("protos")
        .customize(protoc_rust::Customize {
            carllerche_bytes_for_bytes: Some(true),
            carllerche_bytes_for_string: Some(true),
            ..Default::default()
        })
        .run()
        .expect("Running protoc failed.");
}

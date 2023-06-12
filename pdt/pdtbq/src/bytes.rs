use serde::{Serializer, Deserialize, Deserializer};


#[derive(Serialize, Deserialize, Debug)]
pub struct Bytes {
    data: Vec<u8>,
}

impl Bytes {
    pub fn serialize<S>(bytes: &Vec<u8>, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        serializer.
    }
    
}

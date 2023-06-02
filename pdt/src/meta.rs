use crate::{context, utils};
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use std::fs::File;
use std::io::{BufReader, BufWriter};
use std::path::{Path, PathBuf};

const META_FILE_NAME: &str = "meta.json";

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Meta {
    max_blk: u64,
}

impl Meta {
    pub fn from_data(max_blk: u64) -> Result<Self> {
        Ok(Meta { max_blk })
    }

    pub fn load(from_dir: &Path) -> Result<Self> {
        let my_name = Path::new(from_dir).join(META_FILE_NAME);
        let file = File::open(my_name)?;
        let reader = BufReader::new(file);
        let some_data = serde_json::from_reader(reader)?;
        Ok(some_data)
    }

    pub fn save(&self, to_dir: &Path) -> Result<()> {
        let my_name = Path::new(to_dir).join(META_FILE_NAME);
        let file = File::create(my_name)?;
        let writer = BufWriter::new(file);
        serde_json::to_writer(writer, self);
        Ok(())
    }

    pub fn max_block(&self) -> u64 {
        self.max_blk
    }
}

// Bigquery importer.
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use async_trait::async_trait;
use pdtbq::bq::ZilliqaBQProject;
use pdtlib::exporter::Exporter;
use std::ops::Range;

#[async_trait]
pub trait Importer {
    /// Retrieve an id so we know what importer we're talking about.
    fn get_id(&self) -> String;

    /// Set the client id
    fn set_client_id(&mut self, client_id: &str);

    /// Retrieve the max block for this round of import
    async fn get_max_block(&self, exp: &Exporter) -> Result<i64>;

    /// Get the next range for this import
    async fn maybe_range(
        &self,
        project: &ZilliqaBQProject,
        last_max: i64,
    ) -> Result<Option<Range<i64>>>;

    /// Set up an internal buffer.
    async fn extract_start(
        &mut self,
        project: &ZilliqaBQProject,
        exporter: &Exporter,
    ) -> Result<()>;

    /// Extract a range from the database to an internal buffer.
    async fn extract_range(
        &mut self,
        project: &ZilliqaBQProject,
        exporter: &Exporter,
        range: &Range<i64>,
    ) -> Result<()>;

    /// Insert the internal buffer into the database
    async fn extract_done(&mut self, project: &ZilliqaBQProject, exporter: &Exporter)
        -> Result<()>;
}

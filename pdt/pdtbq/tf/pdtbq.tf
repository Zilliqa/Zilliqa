terraform {
  backend "gcs" {
    bucket = "${var.bucket}"
    prefix = "pdt/${var.project}/${var.dataset}"
  }
}


resource "google_bigquery_dataset" "main_ds" {
  dataset_id = "${var.dataset}"
  description = "Dataset for imported records"
  location = "ASIA"
}
  

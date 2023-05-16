# Persistence

Persistence is stored in an S3 bucket.

The structure, beyond a prefix, is:

```
/historical-data -> contains historical data
/persistence     -> contains persistence increments
```

## historical-data

`historical-data` is unchanging, and was traditionally downloaded by `download_static_DB.py`.

There used to be a compressed and an uncompressed version, but we currently only support compressed historical persistence.




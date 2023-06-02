# Persistence

Persistence is stored in an S3 bucket.

The S3 bucket for mainnet is c5b686 04-8540-4887-ad29-2ab9e680f997 .
The S3 bucket for testnets is 301978b4-0c0a-4b6b-ad7b-3a2f63c5182c .

The structure, beyond a prefix, is:

```
/historical-data -> contains historical data
/persistence     -> contains persistence increments
/incremental     -> Contains incremental persistence
```

## historical-data

`historical-data` is unchanging, and was traditionally downloaded by `download_static_DB.py`.

There used to be a compressed and an uncompressed version, but we currently only support compressed historical persistence.

## persistance incremental db

There is quite a lot of this :-), rooted at `/persistence/<netname>`.

`incremental/../.lock` is an upload lock which means someone is currently uploading to the bucket.
`incremental/../.currentTxBlk` is the current Tx block being uploaded (NOTE! this is updated before syncing)
`incremental/../persistence` contains all of persistence as at (I think) the last vacuous epoch
`incremental/../diff_persistence_<blknum>.tar.gz` contains persistence diffs. For each DS epoch, (blockNum % NUM_FINAL_BLOCK_PER_POW) we upload the whole of persistence. For other blocks, we update persistence, and then upload the diffs (just what we changed) to diff_persistence_<block>.tar.gz .
`statedelta/../stateDelta_<blknum>.tar.gz` contains state deltas - we do this by updating `incremental/../persistence/stateDelta` and then recording what we did in the state delta diffs.

The way we do this in this tool is to download the whole database, and then compose a persistence at a particular block from there.


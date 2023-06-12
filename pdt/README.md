
## Determining whether a transaction is EVM or not

This is done (`AccountStore.cpp`) by a truly horrid set of heuristics:

 - If data is not empty and the destination is not the null address and code is empty, it's a contract call.
 - If code is not empty and it's to the null address, it's a contract creation.
 - If data is empty and it's not to the null address, and code is also empty, it's NON_CONTRACT
 - Otherwise error.

Now:

 * If this is a contract creation, and the code begins 'E' 'V' 'M' (hopefully no legal scilla ever starts this way!) it is an EVM transaction.
 * Otherwise, look at the contents of the to address, if it exists.
 * If the to address has code which is EVM, this transaction is EVM, otherwise it isn't.

## Permissions needed in bq:

 - Data Owner

## Validating queries

```
-- Check that there are no duplicates.
SELECT id FROM `rrw-bigquery-test-id.zilliqa.transactions` GROUP BY id HAVING COUNT(1) > 1 LIMIT 1000
```

## Views

```
SELECT DIV(block,10000) AS blk, COUNT(DISTINCT(from_addr)) AS addr, COUNT(*) AS txns, SUM(amount) AS amount FROM `rrw-bigquery-test-id.zilliqa.transactions` GROUP BY DIV(block, 10000)
```


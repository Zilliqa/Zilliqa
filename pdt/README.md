Permissions needed in bq:

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


Permissions needed in bq:

 - Data Owner

## Validating queries

```
-- Check that there are no duplicates.
SELECT id FROM `rrw-bigquery-test-id.zilliqa.transactions` GROUP BY id HAVING COUNT(1) > 1 LIMIT 1000
```


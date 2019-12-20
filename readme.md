The OLEDB bulk copy operation always loads `true` for my bit columns.

This little repro program comes from [Microsoft's sample code](https://docs.microsoft.com/en-us/sql/connect/oledb/ole-db-how-to/bulk-copy-data-using-irowsetfastload-ole-db?view=sql-server-ver15).  

Loading ten rows of alernating `0x00` and `0xFF` values into the blob results in this data in SQL server:

```
col_bit
-------
1
1
1
1
1
1
1
1
1
1

(10 row(s) affected)
```
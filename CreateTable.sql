use Test -- Create Database Test

IF EXISTS (SELECT name FROM sysobjects WHERE name = 'IRFLTable')  
     DROP TABLE IRFLTable  
GO  
  
CREATE TABLE IRFLTable (col_bit bit)  

delete from IRFLTable
select * from IRFLTable
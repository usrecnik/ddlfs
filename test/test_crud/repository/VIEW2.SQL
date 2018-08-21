CREATE OR REPLACE FORCE EDITIONABLE VIEW "DDLFS_TESTCASE"."VIEW2" AS
SELECT
      '1' as DUMMY /* my comment */
       FROM (select 1 from dual where 1 = 1)

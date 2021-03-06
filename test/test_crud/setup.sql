-- alter session set container=...;
-- drop user ddlfs_testcase cascade;
grant dba, create session to ddlfs_testcase identified by "testcase";
grant create table to ddlfs_testcase;

create table ddlfs_testcase.table1 (col_a number, col_b number);
alter table ddlfs_testcase.table1 add constraint table1_pk primary key (col_a);
create table ddlfs_testcase.table2 (col_b number, col_d number);

CREATE OR REPLACE TRIGGER "DDLFS_TESTCASE"."TRIGGER1"
BEFORE UPDATE ON "DDLFS_TESTCASE"."TABLE1"
FOR EACH ROW
BEGIN
   dbms_output.put_line('hello world, 1');
END;
/

CREATE OR REPLACE TRIGGER "DDLFS_TESTCASE"."TRIGGER2"
BEFORE UPDATE ON "DDLFS_TESTCASE"."TABLE1"
FOR EACH ROW
BEGIN
   dbms_output.put_line('hello world, 2');
END;
/

CREATE MATERIALIZED VIEW "DDLFS_TESTCASE"."MVIEW1" AS
    select * /* first */ from ddlfs_testcase.table1;

CREATE MATERIALIZED VIEW "DDLFS_TESTCASE"."MVIEW2" AS
    select * /* second */ from dual;

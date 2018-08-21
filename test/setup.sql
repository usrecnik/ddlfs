-- alter session set container=...;
-- drop user ddlfs_testcase cascade;
grant dba, create session to ddlfs_testcase identified by "testcase";
create table ddlfs_testcase.table1 (col_a number, col_b number);
alter table ddlfs_testcase.table1 add constraint table1_pk primary key (col_a);

CREATE OR REPLACE EDITIONABLE FUNCTION "DDLFS_TESTCASE"."FUNCTION2" RETURN NUMBER AS
BEGIN
    dbms_output.put_line('dummy1');
    dbms_output.put_line('dummy2');
    dbms_output.put_line('dummy3')
    RETURN 123;
END;

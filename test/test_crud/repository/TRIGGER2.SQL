CREATE OR REPLACE TRIGGER "DDLFS_TESTCASE"."TRIGGER2"
BEFORE UPDATE ON "DDLFS_TESTCASE"."TABLE1"
FOR EACH ROW
BEGIN
   dbms_output.put_line('hello world');
   :new.col_b := :new.col_b + 1;
END;

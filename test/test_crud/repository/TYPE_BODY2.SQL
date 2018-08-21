CREATE OR REPLACE EDITIONABLE TYPE BODY "DDLFS_TESTCASE"."TYPE2" AS
   MEMBER FUNCTION do_example RETURN VARCHAR2
   BEGIN
      -- this is example
      dbms_output.put_line('Hello world!');
   END;
END;

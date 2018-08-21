CREATE OR REPLACE EDITIONABLE PACKAGE BODY "DDLFS_TESTCASE"."PACKAGE2" AS

   -- my comment
   PROCEDURE proc_1(p_param IN NUMBER) AS
   BEGIN
      dbms_output.put_line('Hello world');
   END;

   FUNCTION func_1(p_param1 IN VARCHAR2, 
                   p_param2 IN VARCHAR2) RETURN NUMBER AS
   BEGIN
      -- another comment
      dbms_output.put_line('p1=[' || p_param1 || ']');
      dbms_output.put_line('p2=[' || p_param2 || ']');
      return length(p_param1);
   END;

END;

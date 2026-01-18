#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "define_lite.h"

variable test_suite_errors := 0;
variable test_suite_verbose := false;
variable test_suite_assertions := 0;

procedure assertTrue(variable desc, variable a) begin
   test_suite_assertions++;

   if (not a) then begin
      display_msg("Assertion failed \""+desc+"\": is not true");
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("Assert \""+desc+"\" ok");
   end
end

procedure assertFalse(variable desc, variable a) begin
   test_suite_assertions++;

   if (a) then begin
      display_msg("Assertion failed \""+desc+"\": is not false");
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("Assert \""+desc+"\" ok");
   end
end

procedure assertEquals(variable desc, variable a, variable b) begin
   test_suite_assertions++;

   if (a != b or typeof(a) != typeof(b)) then begin
      display_msg("FAIL \""+desc+"\": "+a+" != "+b);
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("ok \""+desc+"\"");
   end
end

procedure assertNotEquals(variable desc, variable a, variable b) begin
   test_suite_assertions++;

   if (a == b) then begin
      display_msg("FAIL \""+desc+"\": "+a+" == "+b);
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("ok \""+desc+"\"");
   end
end


procedure assertFloat(variable desc, variable a, variable b, variable tolerance = 0.001) begin
   test_suite_assertions++;

   variable diff := abs(a - b);
   if (diff > tolerance) then begin
      display_msg("FAIL \""+desc+"\": "+a+" != "+b+" (diff: "+diff+")");
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("ok \""+desc+"\"");
   end
end

procedure report_test_results(variable desc) begin
   
   display_msg("DONE " + desc + " " + 
     (test_suite_assertions-test_suite_errors) + "/" + test_suite_assertions + "");
   display_msg("================");


   #define GL_VAR_TESTING_INFO_ARRAY_ID "TestArra"
   #define TEST_ARR_IDENTIFIER_KEY "__testing_info_array__"
   variable arr_id = get_sfall_global_int(GL_VAR_TESTING_INFO_ARRAY_ID);
   if arr_id != 0 then begin
      // Let's check that it is actually our testing array
      if array_key(arr_id, -1) == 0 then begin
         // It is not even associative array, so it is not our testing array
         arr_id = 0;
      end else begin
         if arr_id[TEST_ARR_IDENTIFIER_KEY] != 1 then begin
            // It is associative array, but it is not our testing array
            arr_id = 0;
         end
      end
   end
   if arr_id == 0 then begin
      arr_id = create_array(-1, 0);
      arr_id[TEST_ARR_IDENTIFIER_KEY] = 1;
      set_sfall_global(GL_VAR_TESTING_INFO_ARRAY_ID, arr_id);
   end


   #define TEST_SUITES "Test suites"
   #define TEST_ASSERTIONS_TOTAL "Tests assertings total"
   #define TEST_ASSERTIONS_FAILED "Tests assertings failed"
   
   if arr_id[TEST_SUITES] == 0 then begin
      arr_id[TEST_SUITES] = create_array(-1, 0);
   end

   //set_sfall_global("test_suite_errors", test_suite_errors);
   
   arr_id[TEST_ASSERTIONS_TOTAL] += test_suite_assertions;
   arr_id[TEST_ASSERTIONS_FAILED] += test_suite_errors;
   arr_id[TEST_SUITES][desc] += 1;

   variable total_suites_names = "";
   variable total_suites_count = 0;

   variable key, item;
   foreach (key: item in arr_id[TEST_SUITES]) begin
      total_suites_count += item;
      if (total_suites_names != "") then begin
         total_suites_names += ", ";
      end
      total_suites_names += key;
   end
   

   variable assertions_stats_str = (arr_id[TEST_ASSERTIONS_TOTAL]-arr_id[TEST_ASSERTIONS_FAILED]) + 
      "/" + arr_id[TEST_ASSERTIONS_TOTAL];
   if (arr_id[TEST_ASSERTIONS_FAILED] == 0) then begin
      float_msg(dude_obj, "Tested " + total_suites_count + " cases: " + total_suites_names + ". " +
          assertions_stats_str + " assertions passed!", FLOAT_MSG_GREEN);
   end else begin
      float_msg(dude_obj, "Tested " + total_suites_count + " cases: " + total_suites_names + ". " +
          assertions_stats_str + " assertions passed", FLOAT_MSG_RED);
   end
end

#endif

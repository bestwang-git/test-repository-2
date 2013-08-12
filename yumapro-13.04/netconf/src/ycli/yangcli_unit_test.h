/*
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifndef _H_yangcli_unit_test
#define _H_yangcli_unit_test

/*  FILE: yangcli_unit_test.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

  
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
17-augr-12    trshueh      Begun

*/

#include <xmlstring.h>

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
/********************************************************************
*                                                                   *
*                        C O N S T A N T S                          *
*                                                                   *
*********************************************************************/
/* these 3 constants not used */
#define ut_M_test (const xmlChar *)"yumaworks-test"
#define ut_R_test (const xmlChar *)"2013-03-31"
#define ut_test_suites (const xmlChar *) "test-suites"

#define ut_suite      (const xmlChar *) "test-suite"
#define ut_suite_description (const xmlChar *) "description"
#define ut_suite_name (const xmlChar *) "name"
#define ut_suite_setup (const xmlChar *) "setup"
#define ut_suite_setup_string (const xmlChar *) "string"
#define ut_suite_cleanup (const xmlChar *) "cleanup"
#define ut_suite_cleanup_string (const xmlChar *) "string"
#define ut_suite_run_test (const xmlChar *) "run-test"
#define ut_suite_test     (const xmlChar *) "test"
#define ut_suite_test_name  (const xmlChar *) "name"
#define ut_suite_test_description (const xmlChar *) "description"
#define ut_suite_test_must_pass (const xmlChar *) "must-pass"
#define ut_suite_test_step (const xmlChar *) "step"
#define ut_suite_test_step_name (const xmlChar *) "name"
#define ut_suite_test_step_description (const xmlChar *) "description"
#define ut_suite_test_step_session_name (const xmlChar *) "session-name"
#define ut_suite_test_step_result_type  (const xmlChar *) "result-type"
#define ut_suite_test_step_result_error_tag \
    (const xmlChar *) "result-error-tag"
#define ut_suite_test_step_result_error_apptag \
    (const xmlChar *) "result-error-apptag"
#define ut_suite_test_step_result_error_info \
    (const xmlChar *) "result-error-info"
#define ut_suite_test_step_command (const xmlChar *) "command"

#define ut_suite_test_step_result_data_type \
    (const xmlChar *) "result-data-type"
#define ut_suite_test_step_result_rpc_reply_data \
    (const xmlChar *) "rpc-reply-data"

#define result_type_is_none (const xmlChar *) "none"
#define result_type_is_ok (const xmlChar *) "ok"
#define result_type_is_error (const xmlChar *) "error"
#define result_type_is_data (const xmlChar *) "data"
#define result_type_is_nothing (const xmlChar *) ""

#define data_response_type_is_any_data \
              (const xmlChar *) "any-data"
#define data_response_type_is_data_empty \
              (const xmlChar *) "data-empty"
#define data_response_type_is_data_non_empty \
              (const xmlChar *) "data-non-empty"
#define data_response_type_is_data_match \
              (const xmlChar *) "data-match"

/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/
/********************************************************************
* FUNCTION fullspec_for_ut_data
*
* INPUTS:
*    suite_name == test suite name to use
*    test_name == test name to use
*    step_name == step name to use
*    status == return status
* RETURNS:
*    fullspec or NULL
*********************************************************************/
extern xmlChar*
    fullspec_for_ut_data (xmlChar *suite_name,
                          xmlChar *test_name,
                          xmlChar *step_name,
                          boolean recording,
                          status_t* res);


/********************************************************************
* FUNCTION check_this_suite_test_exist
*
* INPUTS:
*   server ==  server context to use.
*   suite_name == name of test-suite.
*   test_name  == name of test.
*
* RETURNS
*   res == return status
*********************************************************************/
extern boolean
   check_this_suite_test_exist (
                 server_cb_t *server_cb,
                 const xmlChar    *suite_name,
                 const xmlChar    *test_name);

/********************************************************************
* FUNCTION add_to_ut_suiteQ_testQ_runQ
*
* INPUTS:
*   context == record-test context to use.
*   server ==  server context to use.
*   suite_name == name of test-suite to add to ut suiteQ.
*   test_name  == name of test to add to ut testQ and runQ.
*
* RETURNS
*   res == return status
*********************************************************************/
extern status_t
   add_to_ut_suiteQ_testQ_runQ (server_cb_t *server_cb,
                     yangcli_ut_context_t *rt_context,
                     const xmlChar        *suite_name,
                     const xmlChar        *test_name);

/********************************************************************
* FUNCTION response_type_str
*
* Convert response type enum to string
*
* INPUTS:
*   rt == enumeration for this response type
* RETURNS:
*   response type string
*********************************************************************/
extern const xmlChar *
    response_type_str (response_type_t rt);


/********************************************************************
 * FUNCTION yangcli_testfile_save
 *
 * Save the unit-test config file into memory
 *
 * INPUTS:
 *   server == server context to use
 *   testconf == test suite config filespec
 *
 * RETURNS:
 *  status
 *********************************************************************/
extern status_t
    yangcli_testfile_save (const yangcli_ut_context_t *context,
                       const xmlChar *fspec);


/********************************************************************
* FUNCTION find_test
*
* find_test pointer
*
* INPUTS:
*   suite == test suite struct to check
*   test_name == run_test name to find
*
* RETURNS:
*   pointer to found record or NULL if not found
*********************************************************************/
extern yangcli_ut_test_t *
    find_test (yangcli_ut_suite_t *suite,
               const xmlChar *test_name);


/********************************************************************
* FUNCTION find_suite
*
* find_suite pointer
*
* INPUTS:
*   context == yangcli_ut_context_t
*   tempQ == Q of yangcli_ut_suite_t to use
*   suite_name == test suite name to find
*
* RETURNS:
*   pointer to found record or NULL if not found
*********************************************************************/
extern yangcli_ut_suite_t *
    find_suite (yangcli_ut_context_t *context,
                dlq_hdr_t *tempQ,
                const xmlChar *suite_name);


/********************************************************************
* FUNCTION clean_context
*
* clean any result vars in the context and test suites
*
* INPUTS:
*   context == context results to clean
*   suite_ptr == suite to clean or NULL to clean all
*********************************************************************/
extern void
    clean_context (yangcli_ut_context_t *context,
                   yangcli_ut_suite_t *suite_ptr);

/********************************************************************
* FUNCTION free_context_cache
*
* Cleanup the server test-suite context cache
*
* INPUTS:
*   context == context to use
* RETURNS:
*   none
*********************************************************************/
extern void
    free_context_cache (yangcli_ut_context_t *context);


/********************************************************************
* FUNCTION free_suite
*
* free a suite struct
*
* INPUTS:
*   unit-test suite == struct to free
*
*********************************************************************/
extern void
    free_suite (yangcli_ut_suite_t *suite);

/********************************************************************
* FUNCTION free_test
*
* free a pointer
*
* INPUTS:
*   test_ptr == a test to free
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
extern void
    free_test (yangcli_ut_test_t *test_ptr);

/********************************************************************
* FUNCTION free_step
*
* free a step cache entry
*
* INPUTS:
*   step == test step entry to free
*
*********************************************************************/
extern void
    free_step (yangcli_ut_step_t *step);

/********************************************************************
* FUNCTION free_mustpass_name
*
* free a pointer
*
* INPUTS:
*   grptr == group to free
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
extern void
    free_mustpass_name (mustpass_t *ptr);

/********************************************************************
* FUNCTION free_result_error_info
*
* free a result error info
*
* INPUTS:
*   ptr == result_error_info to free
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
extern void
    free_result_error_info (result_error_info_t *ptr);

/********************************************************************
* FUNCTION free_run_test
*
* free a run stest struct
*
* INPUTS:
*   run_test_ptr == a run_test to free
*
* RETURNS:
*   none
*********************************************************************/
extern void
    free_run_test (run_test_t *run_test_ptr);

/********************************************************************
 * FUNCTION do_test_suite (local RPC)
 * 
 * test-suite load[=filespec]
 * test-suite save[=filespec]
 * test-suite show
 * test-suite start
 * test-suite run
 * Get the specified parameter and run the specified test suite
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *    status
 *********************************************************************/
extern status_t
    do_test_suite (server_cb_t *server_cb,
                   obj_template_t *rpc,
                   const xmlChar *line,
                   uint32  len);

extern void
    yangcli_ut_init (server_cb_t *server_cb);

extern void
    yangcli_ut_cleanup (server_cb_t *server_cb);

extern status_t
    yangcli_ut_load (server_cb_t *server_cb,
                     const xmlChar *testconf,
                     boolean file_error);

extern status_t
    yangcli_ut_save (server_cb_t *server_cb,
                     const xmlChar *testconf);

extern status_t
    yangcli_ut_start (server_cb_t *server_cb,
                      const xmlChar *suite_name,
                      const xmlChar *logfile);

extern boolean
    yangcli_ut_active (server_cb_t *server_cb);

extern xmlChar *
    yangcli_ut_getline (server_cb_t *server_cb,
                        status_t *res);

extern void
    yangcli_ut_handle_return (server_cb_t *server_cb,
                              status_t res);

extern void
    yangcli_ut_handle_reply (server_cb_t *server_cb,
                             response_type_t resp_type,
                             const xmlChar *msg_id,
                             val_value_t *reqmsg,
                             val_value_t *rpydata);

extern status_t
    yangcli_ut_delete_suite (server_cb_t *server_cb,
                             val_value_t *valset);

extern status_t
    yangcli_ut_delete_test (server_cb_t *server_cb,
                            val_value_t *valset);


extern const xmlChar *
    yangcli_ut_get_target_session (server_cb_t *server_cb);

extern void
    yangcli_ut_stop (server_cb_t *server_cb,
                     status_t res,
                     const xmlChar *errstring);

/********************************************************************
 * FUNCTION get_top_obj_ut
 *
 * Get the unit-test top object
 *
 * RETURNS:
 *   pointer to object template for top object 'saved-sessions'
 *********************************************************************/
extern obj_template_t *
    get_top_obj_ut (void);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_unit_test */

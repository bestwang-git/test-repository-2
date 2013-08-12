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
/*  FILE: yangcli_unit_test.c

object identifiers:

container /unit-test
list /unit-test/test-suite
leaf /unit-test/test-suite/name
leaf /unit-test/test-suite/description
container /unit-test/test-suite/setup
container /unit-test/test-suite/cleanup
leaf-list /unit-test/test-suite/run-test
list /unit-test/test-suite/test
leaf /unit-test/test-suite/test/name
leaf /unit-test/test-suite/test/description
leaf-list /unit-test/test-suite/test/must-pass
list /unit-test/test-suite/test/step
leaf /unit-test/test-suite/test/step/name
leaf /unit-test/test-suite/test/step/description
leaf /unit-test/test-suite/test/step/session-name
leaf /unit-test/test-suite/test/step/result-type
leaf /unit-test/test-suite/test/step/result-error-tag
leaf /unit-test/test-suite/test/step/result-error-apptag
leaf-list /unit-test/test-suite/test/step/result-error-info
leaf /unit-test/test-suite/test/step/command


Design Description:

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24Aug12      trshueh      begun; 


*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <assert.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>

#include "procdefs.h"
#include "conf.h"
#include "dlq.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncx_list.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_unit_test.h"
#include "yangcli_util.h"
#include "xml_util.h"
#include "xml_wr.h"
#include "mgr_load.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define YANGCLI_UT_DEBUG 1
#endif

#define ANY_STEP_ERR(S) ((S)->step_result_wrong || \
                         (S)->step_timed_out || \
                         (S)->step_local_error || \
                         (S)->step_error_tag_wrong || \
                         (S)->step_error_apptag_wrong || \
                         (S)->step_error_info_wrong)


/********************************************************************
*                                                                    *
*                             T Y P E S                              *
*                                                                    *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

#define get_ut_context(server_cb) (&server_cb->ut_context)

static xmlChar *
    get_full_rawline (yangcli_ut_context_t *context,
                      status_t *res);

static void
    show_test_suite (yangcli_ut_context_t *context,
                     yangcli_ut_suite_t *testsuite,
                     help_mode_t mode);

static void
    clean_suite (yangcli_ut_suite_t *suite);

static boolean
    any_test_step_errors (yangcli_ut_test_t *test);


/********************************************************************
* FUNCTION  update_mustpass_list
*
* update must_pass list if this run test failed.
*
* INPUTS: ut context to use
*
* RETURNS: void
*********************************************************************/
static void
    update_mustpass_list ( yangcli_ut_context_t *context)
{
    yangcli_ut_suite_t *suite = context->cur_suite;
    xmlChar  *this_test_name = context->cur_run_test->run_test_name;
    yangcli_ut_test_t *this_test = context->cur_run_test->test;

    if (!(this_test->test_started)) {
        return;
    } 

   if (LOGDEBUG3) {
       log_debug3 ("\nUpdating must_pass_list for %s/%s \n",
                       context->cur_suite->name,
            context->cur_run_test->run_test_name);
   }

    yangcli_ut_test_t *test;
    mustpass_t  *must;
    for (test = (yangcli_ut_test_t *)dlq_firstEntry(&suite->test_listQ);
        test != NULL;
        test = (yangcli_ut_test_t *)dlq_nextEntry(test)) {

        for (must = (mustpass_t  *)dlq_firstEntry(&test->mustpass_leaflistQ);
                   must != NULL; must = (mustpass_t *)dlq_nextEntry(must)) {

             if (!xml_strcmp(this_test_name, must->mustpass_name)) { 
                 must->checked = TRUE;
                 if (any_test_step_errors(this_test) == TRUE){
                    log_error("\n(%s)/(%s) failed which is a must_pass for (%s)/(%s)",
                              suite->name,must->mustpass_name,suite->name,test->test_name);
                     must->passed = FALSE;
                 } else {
                     must->passed = TRUE;
                 }
             }
         }
    }
}  /* update_mustpass_list */

/********************************************************************
* FUNCTION mustpass_failed
*
* Check the test's must_pass list for any failed test name.
*
* INPUTS:
*   test == test to use
*
* RETURNS:
*   Pointer to name of the failed test or null (no failed test).
*********************************************************************/
static mustpass_t* mustpass_failed_name (yangcli_ut_test_t *test)
{
    mustpass_t *must = (mustpass_t *)
         dlq_firstEntry(&test->mustpass_leaflistQ);

    for (; must != NULL;
        must = (mustpass_t *)dlq_nextEntry(must)) {
        if (must->checked) {
            if ( must->passed == FALSE ) {
               return must;
            }
        }
    }

    return NULL;
} /* mustpass_failed_name */

/********************************************************************
* FUNCTION run_test_with_no_failed_mustpass
*
* Find a test to run which does not have a fail test in its must_pass list.
*
* INPUTS:
*   run_test == run_test to use
* RETURNS:
*   NULL or next run_test
*********************************************************************/
static run_test_t*
    run_test_with_no_failed_mustpass (run_test_t* run_test ) {

    mustpass_t *must;

    for (; run_test; run_test = (run_test_t *)dlq_nextEntry(run_test)) {
        if (run_test) {
           if ( (must = mustpass_failed_name(run_test->test)) == NULL ) {
               return run_test;
           } else {
               log_error("\n(%s) will be skipped, its must_pass (%s) failed.", 
                         run_test->run_test_name, must->mustpass_name);
           }
        }
    } /* for */

    return NULL;

} /* run_test_with_no_failed_mustpass */

/********************************************************************
* FUNCTION get_data_response_type
*
* Convert response type string to enum
*
* INPUTS:
*    rtstr == response type string
* RETURNS:
*   enumeration for this response type
*********************************************************************/
static response_type_t
    get_data_response_type (const xmlChar *rtstr)
{
    if (!xml_strcmp(rtstr, data_response_type_is_any_data)) {
        return UT_RPC_ANY_DATA;
    } else if (!xml_strcmp(rtstr, data_response_type_is_data_empty)) {
        return UT_RPC_DATA_EMPTY;
    } else if (!xml_strcmp(rtstr, data_response_type_is_data_non_empty)) {
        return UT_RPC_DATA_NON_EMPTY;
    } else if (!xml_strcmp(rtstr, data_response_type_is_data_match)) {
        return UT_RPC_DATA_MATCH;
    }
    return UT_RPC_DATA_NONE;

}  /* get_response_type */



/********************************************************************
* FUNCTION get_response_type
*
* Convert response type string to enum
*
* INPUTS: 
*    rtstr == response type string
* RETURNS:
*   enumeration for this response type
*********************************************************************/
static response_type_t
    get_response_type (const xmlChar *rtstr)
{
    if (!xml_strcmp(rtstr, result_type_is_ok)) {
        return UT_RPC_OK;
    } else if (!xml_strcmp(rtstr, result_type_is_error)) {
        return UT_RPC_ERROR;
    } else if (!xml_strcmp(rtstr, result_type_is_data)) {
        return UT_RPC_DATA;
    } else if (!xml_strcmp(rtstr, result_type_is_none)) {
        return UT_RPC_NO;
    }
    return UT_RPC_NONE;

}  /* get_response_type */


/********************************************************************
* FUNCTION start_setup_banner
*
* Print the banner to start a test-suite setup section

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    start_setup_banner (yangcli_ut_context_t *context)
{
    // TBD add timstamp, more details
    log_info("\nStarting test-suite '%s' setup block\n", 
             context->cur_suite->name);

}  /* start_setup_banner */


/********************************************************************
* FUNCTION start_cleanup_banner
*
* Print the banner to start a test-suite cleanup section

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    start_cleanup_banner (yangcli_ut_context_t *context)
{
    // TBD add timstamp, more details
    log_info("\nStarting test-suite '%s' cleanup block\n", 
             context->cur_suite->name);

}  /* start_cleanup_banner */


/********************************************************************
* FUNCTION start_suite_banner
*
* Print the banner to start a test-suite

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    start_suite_banner (yangcli_ut_context_t *context)
{
    // TBD add timstamp, more details
    log_info("\nStarting test-suite '%s'\n  sending results to %s\n", 
             context->cur_suite->name, 
             (context->logfile) ? context->logfile : YANGCLI_STDOUT);
    if (context->cur_suite->description) {
        log_info_append("Description: %s\n", context->cur_suite->description);
    }

}  /* start_suite_banner */


/********************************************************************
* FUNCTION start_test_banner
*
* Print the banner to start a test

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    start_test_banner (yangcli_ut_context_t *context)
{
    // TBD add timstamp, more details
    log_info("\nStarting test '%s/%s'\n", 
             context->cur_suite->name, 
             context->cur_run_test->run_test_name);
    if (context->cur_run_test->test->description) {
        log_info_append("Description %s\n", 
                        context->cur_run_test->test->description);
    }

   context->cur_run_test->test->test_errors = FALSE;

}  /* start_test_banner */


/********************************************************************
* FUNCTION start_step_banner
*
* Print the banner to start a test step

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    start_step_banner (yangcli_ut_context_t *context,
                       const xmlChar *line)
{
    // TBD add timstamp, more details
    log_info("\nStarting step %s in test '%s/%s'\n", 
             context->cur_step->name,
             context->cur_suite->name, 
             context->cur_run_test->run_test_name);
    if (LOGDEBUG && context->cur_step->description) {
        log_info_append("Description: %s\n", context->cur_step->description);
    }
    if (line) {
        log_info_append("%s\n", line);
    }

}  /* start_step_banner */


/********************************************************************
* FUNCTION report_test_results
*
* Print the test results

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    report_test_results (yangcli_ut_context_t *context)
{
    // TBD add timstamp, more details
    log_info("\nTest  %s/%s done\n", 
             context->cur_suite->name,
             context->cur_run_test->run_test_name);

    /*
     *  Update must_pass lists,
     */
    update_mustpass_list (context);

}  /* report_test_results */


/********************************************************************
* FUNCTION report_suite_results
*
* Print the test suite results

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static void
    report_suite_results (yangcli_ut_context_t *context)
{
    // TBD add timstamp, more details
    log_info("\n Test suite %s results:\n", 
             context->cur_suite->name);

    help_mode_t mode = HELP_MODE_NORMAL;
    if (LOGDEBUG) {
        mode = HELP_MODE_FULL;
    }
    if (LOGINFO) {
        show_test_suite(context, context->cur_suite, mode);
    }

}  /* report_suite_results */


/********************************************************************
* FUNCTION next_suite_to_run
*
* INPUTS:
*    context == unit test context control block to use
* OUTPUTS:
*   pointer to line to use or NULL if no next suite to run 
*********************************************************************/
static xmlChar*
    next_suite_to_run (yangcli_ut_context_t *context,
                      status_t *res)
{
    xmlChar *line = NULL;

    yangcli_ut_suite_t *suite = (yangcli_ut_suite_t*)
                    dlq_nextEntry(context->cur_suite);

    if ( suite == NULL) {
        return NULL;
    }


    /* Clean any result from earlier run */
    clean_suite(suite); 

    run_test_t *runtest = (run_test_t *)
        dlq_firstEntry(&suite->run_test_leaflistQ);

    /* clean any result vars from the last suite run */
    /* make sure the test state is ready to run */
    m__free(context->test_sesname);
    context->test_sesname = NULL;
    m__free(context->linebuff);
    context->linebuff = NULL;
    context->linebuff_size = 0;
    context->cur_step = NULL;
    context->cur_rawline = NULL;

    /* make sure all the run-test statements reference real tests
    * store back-ptrs to the test to run in each run-test stmt
    */
    run_test_t *duptest = runtest;
    for (; duptest != NULL;
        duptest = (run_test_t *)dlq_nextEntry(duptest)) {
        yangcli_ut_test_t *test = find_test(suite, duptest->run_test_name);
        if (test == NULL) {
            log_info("\nTest-suite '%s' has no test named '%s'; "
                     "exiting test suite", suite->name,
                     runtest->run_test_name);
            context->ut_state = UT_STATE_ERROR;
            *res = ERR_NCX_NOT_FOUND;
            return NULL;
        } else {
            duptest->test = test;
        }
    }

    /* setup state to start test. */
    context->cur_suite = suite;
    context->cur_run_test = runtest;
    context->ut_input_mode = TRUE;
    context->ut_status = NO_ERR;
    suite->suite_started = TRUE;
    suite->suite_errors = FALSE;

    start_suite_banner(context);

    if (!dlq_empty(&suite->setup_rawlineQ)) {
         context->ut_state = UT_STATE_SETUP;
         context->cur_rawline = (rawline_t *)
            dlq_firstEntry(&suite->setup_rawlineQ);
        start_setup_banner(context);
        line = get_full_rawline(context, res);
    } else {
        context->ut_state = UT_STATE_RUNTEST;
        start_test_banner(context);
        context->cur_step = (yangcli_ut_step_t *)
           dlq_firstEntry(&context->cur_run_test->test->step_listQ);
        line = context->cur_step->command;
        context->cur_run_test->test->test_started = TRUE;
        start_step_banner(context, line);
    } 

    return line;

}  /* next_suite_to_run */


/********************************************************************
* FUNCTION report_wrong_response_type
*
* Print a wrong response type error for a step
*
* INPUTS: 
*    context == unit test context control block to use
*    resp_type == response type that was received
*********************************************************************/
static void
    report_wrong_response_type (yangcli_ut_context_t *context,
                                response_type_t resp_type)
{
    // TBD add timstamp, more details
    const xmlChar *resp_typ_str1 = 
        response_type_str(context->cur_step->result_type);
    const xmlChar *resp_typ_str2 = response_type_str(resp_type);

    log_info("\nError: step %s in test '%s/%s'\n"
             "Got response type '%s', expected '%s'\n", 
             context->cur_step->name,
             context->cur_suite->name, 
             context->cur_run_test->run_test_name,
             resp_typ_str2, resp_typ_str1);

}  /* report_wrong_response_type */


/********************************************************************
* FUNCTION finish_test_suite
*
* Finish up running a test suite OK

* INPUTS: 
*    context == unit test context control block to use
*********************************************************************/
static boolean
    finish_test_suite (yangcli_ut_context_t *context)
{
    boolean final_finish = TRUE;

    /**** Check if there is other suite to run ***/

    if (!( context->single_suite )) {
      yangcli_ut_suite_t *next_suite = (yangcli_ut_suite_t*)
                           dlq_nextEntry(context->cur_suite);
      if ( next_suite != NULL) {
         final_finish = FALSE;
      } 
    }

    /**** finish up test-suite run ****/

    if (final_finish){
       if (context->logfile_open) {
           log_close();
           context->logfile_open = FALSE;
       }
    }

    context->ut_state = UT_STATE_DONE;
    context->ut_input_mode = FALSE;
    report_suite_results(context);

    return final_finish;

}  /* finish_test_suite */


/********************************************************************
* FUNCTION validate_step_rpc_error
*
* Validate that the <rpc-error> received contains the
* fields that were expected for this error
*
* INPUTS: 
*    context == unit test context control block to use
*    rpydata == response message
*********************************************************************/
static void
    validate_step_rpc_error (yangcli_ut_context_t *context,
                             val_value_t *rpydata)
{
    /* get the first <rpc-error> from the reply
     * TBD: support tracking of multiple <rpc-error> elements
     * instead of just 1
     */
    val_value_t *rpcerr = 
        val_find_child(rpydata, NC_MODULE, NCX_EL_RPC_ERROR);
    if (rpcerr == NULL) {
        log_error("\nError: no <rpc-error> element found\n");
        return;
    }

    //  check <error-tag> node expected
    if (context->cur_step->result_error_tag) {
        val_value_t *errtag = 
            val_find_child(rpcerr, NC_MODULE, NCX_EL_ERROR_TAG);
        if (errtag == NULL) {
            context->cur_step->step_error_tag_wrong = TRUE;
            log_error("\nError: no <error-tag> element found\n");
            return;
        }
        if (xml_strcmp(context->cur_step->result_error_tag,
                       VAL_STR(errtag))) {
            context->cur_step->step_error_tag_wrong = TRUE;
            context->cur_step->step_error_tag =
                xml_strdup(VAL_STR(errtag));
            // do not check if malloc failed!
        }
    }

    //  check <error-app-tag> node expected
    if (context->cur_step->result_error_apptag) {
        val_value_t *apptag = 
            val_find_child(rpcerr, NC_MODULE, NCX_EL_ERROR_APP_TAG);
        if (apptag == NULL) {
            context->cur_step->step_error_apptag_wrong = TRUE;
            log_error("\nError: no <error-app-tag> element found\n");
            return;
        }
        if (xml_strcmp(context->cur_step->result_error_apptag,
                       VAL_STR(apptag))) {
            context->cur_step->step_error_apptag_wrong = TRUE;
            context->cur_step->step_error_apptag =
                xml_strdup(VAL_STR(apptag));
            // do not check if malloc failed!
        }
    }

    //  check <error-info> nodes expected
    result_error_info_t *errinfo = (result_error_info_t *)
        dlq_firstEntry(&context->cur_step->result_error_infoQ);
    if (errinfo) {
        /* there is at least 1 errinfo to check
         * get <error-info> container from the <rpc-error>
         */
        val_value_t *errinfo_val = 
            val_find_child(rpcerr, NC_MODULE, NCX_EL_ERROR_INFO);

        /* check each error-info child node expected */
        for (; errinfo != NULL;
             errinfo = (result_error_info_t *)dlq_nextEntry(errinfo)) {

            boolean doerr = FALSE;

            if (errinfo_val) {
                val_value_t *chval = 
                    val_find_child(errinfo_val, NULL, 
                                   errinfo->result_error_info);
                if (chval == NULL) {
                    doerr = TRUE;
                } else {
                    errinfo->found = TRUE;
                }
            } else {
                doerr = TRUE;
            }

            if (doerr) {
                context->cur_step->step_error_info_wrong = TRUE;
                errinfo->found = FALSE;
                if (errinfo_val) {
                    log_error("\nError: no <%s> child within "
                              "<error-info> element found\n",
                              errinfo->result_error_info);
                } else {
                    log_error("\nError: no <error-info> element found "
                              "looking for <%s> child node",
                              errinfo->result_error_info);
                }
            }
        }
    }

}  /* validate_step_rpc_error */


/********************************************************************
* FUNCTION validate_reply_data
*
* Validate that the <rpc-reply> data contents received matches
* the data expected for this reply
*
* INPUTS: 
*    context == unit test context control block to use
*    rpydata == response message
*********************************************************************/
static void
    validate_reply_data (server_cb_t *server,
                         yangcli_ut_context_t *context,
                         val_value_t *rpydata)
{
    xmlChar   *record_fullspec;
    xmlChar   *run_fullspec;
    status_t  res = NO_ERR;
    xml_attrs_t  attrs;
    session_cb_t *session = server->cur_session_cb;
    ncx_display_mode_t dispmode = session->display_mode;

    if (LOGDEBUG3) {
        log_debug3("\nValidate data for test-suite:'%s' ",
             context->cur_suite->name);
        log_debug3("test-case:'%s' ",
             context->cur_run_test->run_test_name);
        log_debug3("STEP:'%s'\n",
            context->cur_step->name);
    }

    run_fullspec = fullspec_for_ut_data (
             context->cur_suite->name,
             context->cur_run_test->run_test_name,
             context->cur_step->name, FALSE, &res);

    if ((res == NO_ERR) && (run_fullspec)) {
        xml_init_attrs(&attrs);
        res = xml_wr_file(run_fullspec,
                     rpydata, &attrs, XMLMODE,
                     session->use_xmlheader,
                     (dispmode == NCX_DISPLAY_MODE_XML_NONS)
                     ? FALSE : TRUE, 0,
                     session->defindent);
    }

    /* Get current run data value */
    val_value_t* newval =
           mgr_load_extern_file(run_fullspec, NULL, &res);

    /* Get recorded data value */
    record_fullspec = fullspec_for_ut_data (
             context->cur_suite->name,
             context->cur_run_test->run_test_name,
             context->cur_step->name, TRUE, &res);

    val_value_t* oldval = 
         mgr_load_extern_file(record_fullspec, NULL, &res);

    /* If there is no existing recorded data for reference, 
     * record this current data as a referenced one 
     */
    if (oldval == NULL) {
        log_info("\nNo existing recorded data of STEP '%s'\n",
                   context->cur_step->name);
        log_info("\nStart to record this reply data of STEP '%s'\n",
                   context->cur_step->name);

        xml_init_attrs(&attrs);
        res = xml_wr_file(record_fullspec,
                 rpydata, &attrs, XMLMODE,
                 session->use_xmlheader,
                 (dispmode == NCX_DISPLAY_MODE_XML_NONS)
                 ? FALSE : TRUE, 0,
                 session->defindent);
    }else {   
        int32 cmpval = val_compare(oldval, newval);
        if (cmpval != 0) {
        log_info("\nValidate reply data failed at STEP '%s'\n",
                   context->cur_step->name);
        } else {
            log_info("\nValidate reply data O.K. at STEP '%s'\n",
                   context->cur_step->name);
        }

        if (LOGDEBUG3) {
            log_debug3 ("\n The value stored in the reference file.");
            val_dump_value_full(oldval,
                         0,   /* startindent */
                         get_defindent(),
                         DUMP_VAL_LOG, /* dumpmode */
                         NCX_DISPLAY_MODE_PLAIN,
                         FALSE,    /* withmeta */
                         FALSE,   /* configonly */
                         TRUE);   /* conf_mode */

            log_debug3 ("\n The current new value in rpydata.");
            val_dump_value_full(newval,
                         0,   /* startindent */
                         get_defindent(),
                         DUMP_VAL_LOG, /* dumpmode */
                         NCX_DISPLAY_MODE_PLAIN,
                         FALSE,    /* withmeta */
                         FALSE,   /* configonly */
                         TRUE);   /* conf_mode */
           }
     }

    if (run_fullspec) {
        m__free(run_fullspec);
    }

    if (record_fullspec) {
        m__free(record_fullspec);
    }

    if (oldval) {
        val_free_value(oldval);
    }

    if (newval) {
        val_free_value(newval);
    }

}  /* validate_reply_data */

/********************************************************************
* FUNCTION ut_state_busy
*
* INPUTS: 
*    context == unit test context control block to use
* RETURNS:
*   TRUE if unit-test state is busy; FALSE if idle
*********************************************************************/
static boolean
    ut_state_busy (yangcli_ut_context_t *context)
{
    switch (context->ut_state) {
    case UT_STATE_NONE:
    case UT_STATE_INIT:
    case UT_STATE_READY:
        return FALSE;
    case UT_STATE_SETUP:
    case UT_STATE_RUNTEST:
    case UT_STATE_CLEANUP:
        return TRUE;
    case UT_STATE_DONE:
        return FALSE;
    case UT_STATE_ERROR:
        return FALSE;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }
    return FALSE;

} /* ut_state_busy */


/********************************************************************
* FUNCTION ut_state_str
*
* convert state enum to a string for display
*
* INPUTS: 
*    state == ut state enumeration
* RETURNS:
*   pointer to state name string
*********************************************************************/
static const xmlChar *
    ut_state_str (ut_state_t state)
{
    switch (state) {
    case UT_STATE_NONE:
        return NCX_EL_NONE;
    case UT_STATE_INIT:
        return (const xmlChar *)"init";
    case UT_STATE_READY:
        return (const xmlChar *)"ready";
    case UT_STATE_SETUP:
        return (const xmlChar *)"setup";
    case UT_STATE_RUNTEST:
        return (const xmlChar *)"runtest";
    case UT_STATE_CLEANUP:
        return (const xmlChar *)"cleanup";
    case UT_STATE_DONE:
        return (const xmlChar *)"done";
    case UT_STATE_ERROR:
        return (const xmlChar *)"error";
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }
    return NCX_EL_NONE;

} /* ut_state_str */



/********************************************************************
* FUNCTION any_test_step_errors
*
* Check the test for any step with errors
*
* INPUTS:
*   suite == test-suite to use
*
* RETURNS:
*   TRUE if any test errors; FALSE if no test errors
*********************************************************************/
static boolean
    any_test_step_errors (yangcli_ut_test_t *test)
{
    yangcli_ut_step_t *ptr;

    for (ptr = (yangcli_ut_step_t *)dlq_firstEntry(&test->step_listQ);
         ptr != NULL;
         ptr = (yangcli_ut_step_t *)dlq_nextEntry(ptr)) {
         if (ANY_STEP_ERR(ptr)){
            return TRUE;
         }
    }

    return FALSE;

}  /* any_step_errors */


/********************************************************************
* FUNCTION any_test_errors
*
* Check the test-suite for any tests with errors
*
* INPUTS: 
*   suite == test-suite to use
*
* RETURNS:
*   TRUE if any test errors; FALSE if no test errors
*********************************************************************/
static boolean
    any_test_errors (yangcli_ut_suite_t *suite)
{
    boolean test_fail = FALSE;

    yangcli_ut_test_t *test = (yangcli_ut_test_t *)
        dlq_firstEntry(&suite->test_listQ);
    for (; test; test = (yangcli_ut_test_t *)dlq_nextEntry(test)) {
        if (test->test_started) {
            if (any_test_step_errors(test) == TRUE){
               test_fail = TRUE;
               test->test_errors = TRUE;
            } else {
               test->test_errors = FALSE;
            }
        }
    }

    return test_fail;

}  /* any_test_errors */


/********************************************************************
* FUNCTION get_full_rawline
*
* Combine all line continuation partial lines into 1 complete buffer
*
* INPUTS: 
*   context == context to use
*   res == address of return status
*
* OUTPUTS:
*   *res = return status
* RETURNS:
*   pointer to line to use or NULL if some error
*********************************************************************/
static xmlChar *
    get_full_rawline (yangcli_ut_context_t *context,
                      status_t *res)
{
    uint32 total = 0;
    rawline_t *first_rawline = context->cur_rawline;
    rawline_t *last_rawline = first_rawline;
    const xmlChar *lineval = first_rawline->line;
    boolean done = FALSE;
    while (!done) {
        uint32 len = xml_strlen(lineval);
        total += len;
        if (len && lineval[len-1] == '\\') {
            rawline_t *test_rawline = 
                (rawline_t *)dlq_nextEntry(last_rawline);
            if (test_rawline == NULL) {
                log_warn("\nWarning: Expected more input after "
                         "line continuation (%s)", lineval);
                done = TRUE;
            } else {
                last_rawline = test_rawline;
                lineval = last_rawline->line;
            }
        } else {
            done = TRUE;
        }
    }
    if (first_rawline == last_rawline) {
        /* normal case, no line continuation */
        *res = NO_ERR;
        /* Remove single quotes if any */
        removeChar( (char*) first_rawline->line, '\'' ); 
        return first_rawline->line;
    } else {
        context->cur_rawline = last_rawline;

        /* need to concat all the strings together */
        if (context->linebuff == NULL || context->linebuff_size < total) {
            /* get a new line buff */
            m__free(context->linebuff);
            context->linebuff_size = 0;
            context->linebuff = m__getMem(total);
            if (context->linebuff == NULL) {
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
            context->linebuff_size = total;
        }
        xmlChar *p = context->linebuff;
        rawline_t *next_rawline = NULL;
        for (; first_rawline != NULL;
             first_rawline = next_rawline) {

            uint32 len = xml_strlen(first_rawline->line);
            if (len) {
                p += xml_strncpy(p, first_rawline->line, len-1);
            }

            if (first_rawline == last_rawline) {
                next_rawline = NULL;
            } else {
                next_rawline = (rawline_t *)
                    dlq_nextEntry(first_rawline);
            }
        }
        *p = 0;
        *res = NO_ERR;

        /* Remove single quotes if any */
        removeChar( (char*) context->linebuff, '\'' );  // remove single quote
        return context->linebuff;
    }

}  /* get_full_rawline */


/********************************************************************
* FUNCTION show_step
*
* Show command for a test step
*
* INPUTS:
*   context == ut context to use
*   suite_name == suite containing this step
*   test_name == test containing this step
*   step == test step struct to use
*   mode == show mode to use
* RETURNS:
*   none
*********************************************************************/
static void
    show_step (yangcli_ut_context_t *context,
               const xmlChar *suite_name,
               const xmlChar *test_name,
               yangcli_ut_step_t *step,
               help_mode_t mode)
{
    log_info("\nStep %s in test %s/%s", step->name,
             suite_name, test_name);

    boolean withres =
        (context->ut_state == UT_STATE_DONE ||
         context->ut_state == UT_STATE_ERROR);

    if (withres) {
        const xmlChar *status = NULL;
        if (!step->step_done) {
            status = (const xmlChar *)"INCOMPLETE";
        } else if (ANY_STEP_ERR(step)) {
            status = (const xmlChar *)"FAIL";
            log_info_append(" [%s]", status);
        } else {
            status = (const xmlChar *)"PASS";
            log_info_append(" [%s]", status);
        }
  //      log_info_append(" [%s]", status);
    }

    if (mode != HELP_MODE_BRIEF) {
        if (step->description) {
            log_info_append("\nDescription: %s", step->description);
        }
    }

    const xmlChar *name;
    if (step->session_name) {
        name = step->session_name;
    } else if (context->test_sesname) {
        name = context->test_sesname;
    } else {
        name = NCX_EL_DEFAULT;
    }

    log_info_append("\nSession: %s", name);

    log_info_append("\nCommand: %s", step->command);

    if (mode == HELP_MODE_BRIEF) {
        return;
    }

    log_info_append("\nResponse Type: %s", 
                    response_type_str(step->result_type));

    if (mode != HELP_MODE_FULL) {
        log_info_append("\n");
        return;
    }

    if (step->result_error_tag) {
        log_info_append("\nerror-tag: %s", step->result_error_tag);
        if ((withres) && (step->step_done)) {
            if (step->step_error_tag_wrong) {
                log_info_append(" [FAIL: got %s]", 
                                step->step_error_tag ? 
                                step->step_error_tag : NCX_EL_NONE);
            } else {
                log_info_append(" [PASS]");
            }
        }
    }
    if (step->result_error_apptag) {
        log_info_append("\nerror-app-tag: %s", 
                        step->result_error_apptag);

        if ((withres) && (step->step_done)){
            if (step->step_error_apptag_wrong) {
                log_info_append(" [FAIL: got %s]", 
                                step->step_error_apptag ?
                                step->step_error_apptag : NCX_EL_NONE);
            } else {
                log_info_append(" [PASS]");
            }
        }
    }

    result_error_info_t *errinfo = (result_error_info_t *)
        dlq_firstEntry(&step->result_error_infoQ);
    for (; errinfo != NULL;
         errinfo = (result_error_info_t *)dlq_nextEntry(errinfo)) {

        log_info_append("\nerror-info: %s", 
                        errinfo->result_error_info);

        if ((withres) && (step->step_done)) {
            if (errinfo->found) {
                log_info_append(" [PASS]");
            } else {
                log_info_append(" [FAIL]");
            }
        }
    }

    log_info_append("\n");

} /* show_step */ 


/********************************************************************
* FUNCTION show_test
*
* Show command for a test
*
* INPUTS:
*   context == ut context to use
*   suite_name == suite containing this test
*   test == test struct to use
*   mode == show mode to use
* RETURNS:
*   none
*********************************************************************/
static void
    show_test (yangcli_ut_context_t *context,
               const xmlChar *suite_name,
               yangcli_ut_test_t *test,
               help_mode_t mode)
{
    log_info("\nTest : %s/%s", suite_name, test->test_name);

    boolean withres =
        (context->ut_state == UT_STATE_DONE ||
         context->ut_state == UT_STATE_ERROR);

    if (withres) {
        const xmlChar *status = NULL;
        if (test->test_started) {
            if (test->test_errors) {
                status = (const xmlChar *)"FAIL";
            } else {
                status = (const xmlChar *)"PASS";
            }
        } else {
            status = (const xmlChar *)"NOT RUN";
        }
        log_info_append(" [%s]", status);
    }

    if (mode != HELP_MODE_BRIEF) {
        if (test->description) {
            log_info_append("\nDescription: %s\n", test->description);
        }
    }

    /* show must-pass Q for this test */
    if (mode == HELP_MODE_FULL) {
        mustpass_t *must = (mustpass_t *)
            dlq_firstEntry(&test->mustpass_leaflistQ);
        if (must != NULL) {
           log_info_append ("must-pass:");
        } 
        for (; must != NULL;
            must = (mustpass_t *)dlq_nextEntry(must)) {

            if (must->checked != TRUE) {
               log_info_append ("\(%s) not checked. ", must->mustpass_name);
            } else if ( must->passed == FALSE )  { 
                log_info_append ("\(%s) failed. ", must->mustpass_name);
            } else {
                log_info_append ("\(%s) passed. ", must->mustpass_name);
            } 
        } /* for */

        log_info_append ("\n");
    }

    if (mode != HELP_MODE_BRIEF) {
        yangcli_ut_step_t *ptr;
        for (ptr = (yangcli_ut_step_t *)dlq_firstEntry(&test->step_listQ);
             ptr != NULL;
             ptr = (yangcli_ut_step_t *)dlq_nextEntry(ptr)) {

            show_step(context, suite_name, test->test_name, ptr, mode);
        }
    }

} /* show_test */ 


/********************************************************************
* FUNCTION show_test_suite
*
* Show command for the test-suite
*
* INPUTS:
*   context == ut context to use
*   testsuite == test suite struct to use
*   mode == show mode to use
* RETURNS:
*   none
*********************************************************************/
static void
    show_test_suite (yangcli_ut_context_t *context,
                     yangcli_ut_suite_t *testsuite,
                     help_mode_t mode)
{
    log_info("\nTest suite: %s", testsuite->name);

    boolean withres =
        (context->ut_state == UT_STATE_DONE ||
         context->ut_state == UT_STATE_ERROR);

    if (withres) {
        const xmlChar *status = NULL;
        switch (context->ut_state) {
        case UT_STATE_NONE:
        case UT_STATE_INIT:
        case UT_STATE_READY:
            status = (const xmlChar *)"NOT RUN";
            break;
        case UT_STATE_SETUP:
            status = (const xmlChar *)"IN SETUP";
            break;
        case UT_STATE_RUNTEST:
            status = (const xmlChar *)"IN PROGRESS";
            break;
        case UT_STATE_CLEANUP:
            status = (const xmlChar *)"IN CLEANUP";
            break;
        case UT_STATE_DONE:
            if (!testsuite->suite_started) {
               status = (const xmlChar *)"NOT RUN";
            } else if (!any_test_errors(testsuite)) {
               status = (const xmlChar *)"PASS";
            }  else if (context->ut_status != NO_ERR) { 
               status = (const xmlChar *)"ERROR STOP";
            } else if (any_test_errors(testsuite)) {
               status = (const xmlChar *)"FAIL";
            }
            break;
        case UT_STATE_ERROR:
            status = (const xmlChar *)"ERROR";
            break;
        default:
            status = (const xmlChar *)"invalid";
            SET_ERROR(ERR_INTERNAL_VAL);
        }

        log_info_append(" [%s]", status);
    }
             
    if (mode != HELP_MODE_BRIEF) {
        if (testsuite->description) {
            log_info_append("\nDescription: %s\n", testsuite->description);
        }
    }

    if (mode == HELP_MODE_FULL) {
        /* TBD: show setup section */
    }

    yangcli_ut_test_t *ptr;
    for (ptr = (yangcli_ut_test_t *)dlq_firstEntry(&testsuite->test_listQ);
         ptr != NULL;
         ptr = (yangcli_ut_test_t *)dlq_nextEntry(ptr)) {

        show_test(context, testsuite->name, ptr, mode);
    }

    if (mode == HELP_MODE_FULL) {
        /* TBD: show cleanup section */
    }

} /* show_test_suite */ 


/********************************************************************
* FUNCTION show_test_suites
*
* Show command for the test-suites
*
* INPUTS:
*   context == ut context to use
*   mode == show mode to use
* RETURNS:
*   none
*********************************************************************/
static void
    show_test_suites (yangcli_ut_context_t *context,
                      help_mode_t mode)
{
    log_info("\nTest suite state: %s", 
             ut_state_str(context->ut_state));

    if (!dlq_empty(&context->suite_listQ)) {
        yangcli_ut_suite_t *ptr;
        for (ptr = (yangcli_ut_suite_t *)
                 dlq_firstEntry(&context->suite_listQ);
             ptr != NULL;
             ptr = (yangcli_ut_suite_t *)dlq_nextEntry(ptr)) {
            show_test_suite(context, ptr, mode);
        }
        log_info_append("\n");
    } else {
        log_info("\nNo test suites found\n");
    }
} /* show_test_suites */ 

 
/********************************************************************
* FUNCTION new_run_test
*
* create a new_run_test pointer
*
* INPUTS:
*   name == run_test name string to use
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static run_test_t *
    new_run_test (const xmlChar *test_name)
{
    run_test_t  *ptr;

    ptr = m__getObj(run_test_t);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0x0, sizeof(run_test_t));
    ptr->run_test_name = xml_strdup(test_name);
    if (!ptr->run_test_name) {
        m__free(ptr);
        return NULL;
    }
    return ptr;

}  /* new_run_test */


/********************************************************************
* FUNCTION clean_run_test
*
* clean a run-test results
*
* INPUTS:
*   run_test_ptr == a run_test to clean
*
* RETURNS:
*   none
*********************************************************************/
static void
    clean_run_test (run_test_t *run_test_ptr)
{
    run_test_ptr->test = NULL;

}  /* clean_run_test */


/********************************************************************
* FUNCTION find_run_test
*
* find_run_test pointer
*
* INPUTS:
*   q == q of run_test_t structs
*   test_name == run_test name to find
*
* RETURNS:
*   pointer to found record or NULL if not found
*********************************************************************/
static run_test_t*
     find_run_test(yangcli_ut_suite_t *suite,
                const xmlChar *run_test_name)
{
    run_test_t   *ptr=NULL;

    for (ptr = (run_test_t *)dlq_firstEntry(&suite->run_test_leaflistQ);
         ptr != NULL;
         ptr = (run_test_t *)dlq_nextEntry(ptr)) {

         if (!xml_strcmp(ptr->run_test_name, run_test_name)) {
            return ptr;
        }
    }
    return NULL;

}  /* find_run_test */

/********************************************************************
* FUNCTION new_result_error_info
*
* create a new_result_error_info pointer
*
* INPUTS:
*   name == new_result_error_info_t string to use
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static result_error_info_t *
    new_result_error_info (const xmlChar *result_error_info)
{
    result_error_info_t *ptr;

    ptr = m__getObj(result_error_info_t);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0x0, sizeof(result_error_info_t));
    ptr->result_error_info = xml_strdup(result_error_info);
    if (!ptr->result_error_info) {
        m__free(ptr);
        return NULL;
    }
    return ptr;
}


/********************************************************************
* FUNCTION new_mustpass_name
*
* create a mustpass_name pointer
*
* INPUTS:
*   name == mustpass name string to use
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static mustpass_t *
    new_mustpass_name (const xmlChar *mustpass_name)
{
    mustpass_t *ptr;

    ptr = m__getObj(mustpass_t);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0x0, sizeof(mustpass_t));
    ptr->mustpass_name = xml_strdup(mustpass_name);
    if (!ptr->mustpass_name) {
        m__free(ptr);
        return NULL;
    }
    return ptr;

}  /* new_mustpass_name */

/********************************************************************
* FUNCTION clean_mustpass
*
* clean a must pass struct test results
*
* INPUTS:
*   mptr == mustpass to clean
*
*********************************************************************/
static void
    clean_mustpass (mustpass_t *mptr)
{
    mptr->checked = FALSE;
    mptr->passed = FALSE;

}  /* clean_mustpass */


/********************************************************************
* FUNCTION find_step
*
* find_step pointer
*
* INPUTS:
*   test == test struct to check
*   step_name == step name to find
*
* RETURNS:
*   pointer to found record or NULL if not found
*********************************************************************/
static yangcli_ut_step_t *
    find_step (yangcli_ut_test_t *test,
               const xmlChar *step_name)
{
    yangcli_ut_step_t *ptr;

    for (ptr = (yangcli_ut_step_t *)dlq_firstEntry(&test->step_listQ);
         ptr != NULL;
         ptr = (yangcli_ut_step_t *)dlq_nextEntry(ptr)) {

        if (!xml_strcmp(ptr->name, step_name)) {
            return ptr;
        }
    }

    return NULL;

}  /* find_step */


/********************************************************************
* FUNCTION new_step
*
* create a step cache entry

* INPUTS:
*   test == unit test in progress
*   val == <step> element to parse into step struct
*   res == address of return status
*
* OUTPUTS:
*   *res == return status
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static yangcli_ut_step_t *
    new_step (yangcli_ut_test_t *test,
              val_value_t *val,
              status_t *res)

{
    /*The test-suite/test-list/test/step-list/step:
     *name of the step:  example:step 1 or step xyz.
     *description of the step: test description for this step
     *session_name of the step: session to issure this step
     *result_type of the step:  expected result type for this step:
     * ok, data, or error
     *result_error_tag of the step: expected error tag if 
     * error_type is error.
     *command of the step: test command to execute
    */

    /* Start to cache the test step information in yangcli_ut_step_t. */
    yangcli_ut_step_t *step = m__getObj(yangcli_ut_step_t);
    if (!step) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(step, 0x0, sizeof(yangcli_ut_step_t));
    dlq_createSQue(&step->result_error_infoQ);

    /* Check the key -- name of this step; will be 
     * checked by YANG parser */
    val_value_t *step_parm =
        val_find_child(val, YANGCLI_TEST_MOD,
                       ut_suite_test_step_name);
    if (!step_parm) {
        free_step(step);
        *res = ERR_NCX_MISSING_INDEX;
        return NULL;
    }

    /* make sure step name not already used */
    if (find_step(test, VAL_STR(step_parm))) {
        log_error("\nError: step name '%s' already used",
                  VAL_STR(step_parm));
        free_step(step);
        *res = ERR_NCX_DUP_ENTRY;
        return NULL;
    }

    step->name = xml_strdup(VAL_STR(step_parm));
    if (!step->name) {
        free_step(step);
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    /* Check the optional description of this step.*/
    step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                               ut_suite_test_step_description);
    if (step_parm) {
        step->description = xml_strdup(VAL_STR(step_parm));
        if (!step->description) {
            free_step(step);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

    /* Check the optional session of this step. */
    step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                               ut_suite_test_step_session_name);
    if (step_parm) {
        step->session_name = xml_strdup(VAL_STR(step_parm));
        if (!step->session_name) {
            free_step(step);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

   /* Check the result type of this step. default is 'ok' */
    step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                               ut_suite_test_step_result_type);
    if (step_parm) {
        step->result_type = 
            get_response_type(VAL_ENUM_NAME(step_parm));
    } else {
        /* default is ANY */
        step->result_type = UT_RPC_ANY;
    }
        
    /* ignore optional error parms unless the result_type_is_error */
    if (step->result_type == UT_RPC_ERROR) {
        step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                                   ut_suite_test_step_result_error_tag);
        if (step_parm) {
            step->result_error_tag = xml_strdup(VAL_STR(step_parm));
            if (step->result_error_tag == NULL) {
                free_step(step);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
        }  // else any error-tag OK

        step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                                   ut_suite_test_step_result_error_apptag);
        if (step_parm) {
            step->result_error_apptag = xml_strdup(VAL_STR(step_parm));
            if (step->result_error_apptag == NULL) {
                free_step(step);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
        }  // else any error-app-tag OK

        /* check optional result-error-info leaflist */
        step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                                   ut_suite_test_step_result_error_info);
        for (; step_parm != NULL;
             step_parm = 
                 val_find_next_child(val, YANGCLI_TEST_MOD,
                                     ut_suite_test_step_result_error_info,
                                     step_parm)) {
            result_error_info_t *errinfo = 
                new_result_error_info(VAL_STR(step_parm));
            if (errinfo == NULL) {
                free_step(step);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
            dlq_enque(errinfo, &step->result_error_infoQ);
        }
    }

    /* Check the mandatory command of this step. */
    step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                               ut_suite_test_step_command);
    if (!step_parm) {
        free_step(step);
        *res = ERR_INTERNAL_MEM;
        return NULL;
    } else {
        step->command = xml_strdup(VAL_STR(step_parm));
        if (!step->command) {
            free_step(step);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

    /* rpc-reply-data */

    /* ignore optional data parms unless the result_type_is_data */
    if (step->result_type == UT_RPC_DATA) {
        step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                             ut_suite_test_step_result_data_type);
        if (step_parm) {
            step->result_data_type =
                 get_data_response_type(VAL_ENUM_NAME(step_parm));
        } else {
            /* default is ANY */
            step->result_data_type = UT_RPC_ANY_DATA;
        }

        step_parm = val_find_child(val, YANGCLI_TEST_MOD,
                       ut_suite_test_step_result_rpc_reply_data);

        if (step_parm) {
            step->result_rpc_reply_data = xml_strdup(VAL_STR(step_parm));
            if (step->result_rpc_reply_data == NULL) {
                free_step(step);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
        }  // else any data response OK
    }


   *res = NO_ERR;
    return step;

}  /* new_step */


/********************************************************************
* FUNCTION clean_step
*
* clean any result vars in the step
*
* INPUTS:
*   step == step struct to clean results
*
*********************************************************************/
static void
    clean_step (yangcli_ut_step_t *step)
{
    step->start_tstamp[0] = 0;
    step->stop_tstamp[0] = 0;
    step->step_result = UT_RPC_NONE;

    m__free(step->step_error_tag);
    step->step_error_tag = NULL;
    m__free(step->step_error_apptag);
    step->step_error_apptag = NULL;

    step->step_done = FALSE;
    step->step_result_wrong = FALSE;
    step->step_timed_out = FALSE;
    step->step_local_error = FALSE;
    step->step_error_tag_wrong = FALSE;
    step->step_error_apptag_wrong = FALSE;
    step->step_error_info_wrong = FALSE;

    result_error_info_t *errinfo = (result_error_info_t *)
        dlq_firstEntry(&step->result_error_infoQ);
    for (; errinfo != NULL;
         errinfo = (result_error_info_t *)dlq_nextEntry(errinfo)) {
        errinfo->found = FALSE;
    }
    
}  /* clean_step */


/********************************************************************
* FUNCTION new_test
*
* create a new_test
*
* INPUTS:
*   suite == unit test suite in progress
*   val == <test> element to parse into test struct
*   res == address of return status
*
* OUTPUTS:
*   *res == return status
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static yangcli_ut_test_t *
    new_test (yangcli_ut_suite_t *suite,
              val_value_t *val,
              status_t *res)
{
    yangcli_ut_test_t *ptr = m__getObj(yangcli_ut_test_t);
    if (!ptr) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(ptr, 0x0, sizeof(yangcli_ut_test_t));
    dlq_createSQue(&ptr->mustpass_leaflistQ);
    dlq_createSQue(&ptr->step_listQ);

  /*------------------------------------------------------------*/
  /* get test key -- name field */
    val_value_t *name_val =
        val_find_child(val, YANGCLI_TEST_MOD,
                       ut_suite_test_name);
    if (!name_val) {
        free_test(ptr);
        *res = ERR_NCX_MISSING_INDEX;
        return NULL;
    }

    /* make sure test name not already used */
    if (find_test(suite, VAL_STR(name_val))) {
        log_error("\nError: test name '%s' already used",
                  VAL_STR(name_val));
        free_test(ptr);
        *res = ERR_NCX_DUP_ENTRY;
        return NULL;
    }

    ptr->test_name = xml_strdup(VAL_STR(name_val));
    if (!ptr->test_name) {
        free_test(ptr);
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    /*------------------------------------------------------------*/
    /* get optional test description field */
    val_value_t *desc_val =
        val_find_child(val, YANGCLI_TEST_MOD,
                       ut_suite_test_description);
    if (desc_val) {
        ptr->description = xml_strdup(VAL_STR(desc_val));
        if (!ptr->description) {
            free_test(ptr);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

    /*------------------------------------------------------------*/
    /* unit-test/test-suite/test/mustpass_leaflistQ */ 
    /* find first node */
    val_value_t *mustpass_nameval =
        val_find_child(val, YANGCLI_TEST_MOD,
                       ut_suite_test_must_pass);

     /* Get all the optional must-pass names */
     for (; mustpass_nameval != NULL && *res == NO_ERR;
         mustpass_nameval = val_find_next_child(val, YANGCLI_TEST_MOD,
                                           ut_suite_test_must_pass,
                                           mustpass_nameval)) {
         mustpass_t *mustpass_test = 
             new_mustpass_name(VAL_STR(mustpass_nameval)); 

         if (mustpass_test) {
            dlq_enque(mustpass_test, &ptr->mustpass_leaflistQ);
         } else {
            free_test(ptr);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

  /*------------------------------------------------------------*/
  /* The steplist of this test /unit-test/test-suite/test/step_listQ */
    val_value_t* childval = val_find_child(val, YANGCLI_TEST_MOD,
                                   ut_suite_test_step);
    for (; childval != NULL;
         childval = val_find_next_child(val, YANGCLI_TEST_MOD,
                                        ut_suite_test_step, childval)) {
        yangcli_ut_step_t *step = new_step(ptr, childval, res);
        if (step) {
            dlq_enque(step, &ptr->step_listQ);
        } else {
            // *res already set
            free_test(ptr);
            return NULL;
        }
    }

   *res = NO_ERR;
   return ptr;
} /* new_test */


/********************************************************************
* FUNCTION clean_test
*
* clean any result vars in the test
*
* INPUTS:
*   test_ptr == test struct to clean results
*
*********************************************************************/
static void
    clean_test (yangcli_ut_test_t *test_ptr)
{
    mustpass_t *mptr = (mustpass_t *)
        dlq_firstEntry(&test_ptr->mustpass_leaflistQ);
    for (; mptr != NULL; mptr = (mustpass_t *)dlq_nextEntry(mptr)) {
        clean_mustpass(mptr);
    }

    yangcli_ut_step_t *step = (yangcli_ut_step_t *)
        dlq_firstEntry(&test_ptr->step_listQ);
    for (; step != NULL;
         step = (yangcli_ut_step_t *)dlq_nextEntry(step)) {
        clean_step(step);
    }

    test_ptr->test_started = FALSE;
    test_ptr->test_errors = FALSE;

}  /* clean_test */

/********************************************************************
* FUNCTION new_suite
*
* create a unit-test-suite struct
*
* INPUTS:
*   context == unit test context to use
*   val == <test-suite> element to parse into suite struct
*   tempQ == Q of yangcli_ut_suite_t to check for duplicates
*   res == address of return status
*
* OUTPUTS:
*   *res == return status
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static yangcli_ut_suite_t * 
    new_suite (yangcli_ut_context_t *context,
               val_value_t *val,
               dlq_hdr_t *tempQ,
               status_t *res)
{
    val_value_t *chval;
    rawline_t *rawline;

    yangcli_ut_suite_t *suite_ptr = m__getObj(yangcli_ut_suite_t);
    if (!suite_ptr) {
        return NULL;
    }
    memset(suite_ptr, 0x0, sizeof(yangcli_ut_suite_t));
    dlq_createSQue(&suite_ptr->setup_rawlineQ);
    dlq_createSQue(&suite_ptr->cleanup_rawlineQ);
    dlq_createSQue(&suite_ptr->run_test_leaflistQ);
    dlq_createSQue(&suite_ptr->test_listQ);

    /* check the test-suite/name  */
    val_value_t *suite_name = val_find_child(val, YANGCLI_TEST_MOD,
                                           ut_suite_name);
    if (!suite_name) {
        free_suite(suite_ptr);
        *res = ERR_NCX_MISSING_INDEX;
        return NULL;
    }

    /* check if this suite name is already used */
    if (find_suite(context, tempQ, VAL_STR(suite_name))) {
        log_error("\nError: test suite name '%s' already used",
                  VAL_STR(suite_name));
        free_suite(suite_ptr);
        *res = ERR_NCX_DUP_ENTRY;
        return NULL;
    }

     /* Cache the key -- test-suite/name  */
    suite_ptr->name =  xml_strdup(VAL_STR(suite_name));
    if (!suite_ptr->name) {
        free_suite(suite_ptr);
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    /* check the optional test-suite/description leaf */
    val_value_t *desc_val =
        val_find_child(val, YANGCLI_TEST_MOD,
                       ut_suite_description);
    if (desc_val) {
        suite_ptr->description = xml_strdup(VAL_STR(desc_val));
        if (!suite_ptr->description) {
            free_suite(suite_ptr);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
    }

    /* check the optional test-suite/setup block */
    val_value_t *setup_val = val_find_child(val, YANGCLI_TEST_MOD,
                                           ut_suite_setup);
    if (setup_val) {
        /* Cache the test-suite/setup raw strings */
        chval = val_find_child(setup_val, YANGCLI_TEST_MOD,
                               ut_suite_setup_string);
        for (; chval != NULL;
             chval = val_find_next_child(setup_val, YANGCLI_TEST_MOD,
                                         ut_suite_setup_string,
                                         chval)) {
            rawline = new_rawline(VAL_STR(chval));
            if (rawline == NULL) {
                free_suite(suite_ptr);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
            dlq_enque(rawline, &suite_ptr->setup_rawlineQ);
        }
    }

    /* check the optional test-suite/cleanup block */
    val_value_t *cleanup_val = val_find_child(val, YANGCLI_TEST_MOD,
                                              ut_suite_cleanup);
    if (cleanup_val) {
        /* Cache the test-suite/cleanup raw strings */
        chval = val_find_child(cleanup_val, YANGCLI_TEST_MOD,
                               ut_suite_cleanup_string);
        for (; chval != NULL;
             chval = val_find_next_child(cleanup_val, YANGCLI_TEST_MOD,
                                         ut_suite_cleanup_string,
                                         chval)) {
            rawline = new_rawline(VAL_STR(chval));
            if (rawline == NULL) {
                free_suite(suite_ptr);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
            dlq_enque(rawline, &suite_ptr->cleanup_rawlineQ);
        }
    }

    /* Cache run_tests: leaf-list /unit-test/test-suite/run-test */
    val_value_t *run_test = val_find_child(val, YANGCLI_TEST_MOD, 
                             ut_suite_run_test);
    if (!run_test ) {
        /* there are no run_test in this list; should not happen
        * since YANG parser should check min-elements=1
        */
        free_suite(suite_ptr);
        *res = ERR_NCX_MIN_ELEMS_VIOLATION;
        return NULL;
    }
    for (; run_test != NULL;
         run_test = val_find_next_child(val, YANGCLI_TEST_MOD,
                                        ut_suite_run_test,
                                        run_test)) {
        const xmlChar *run_test_name = VAL_STR(run_test);
        run_test_t *ptr = new_run_test(run_test_name);
        if (ptr) {
            dlq_enque(ptr, &suite_ptr->run_test_leaflistQ);
        } else {
            free_suite(suite_ptr);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
     }

     /* Cache tests: list /unit-test/test-suite/test/ */
     val_value_t *test = val_find_child(val, YANGCLI_TEST_MOD,
                                        ut_suite_test);
     if (!test) {
        free_suite(suite_ptr);
        *res = ERR_NCX_MIN_ELEMS_VIOLATION;
        return NULL;
     }

     for (; test != NULL;
         test = val_find_next_child(val, YANGCLI_TEST_MOD,
                                    ut_suite_test, test )) {
         yangcli_ut_test_t *newtest = new_test(suite_ptr, test, res);
         if (newtest) {
            dlq_enque(newtest, &suite_ptr->test_listQ);
         } else {
           free_suite(suite_ptr);
           // *res already set by new_test
           return NULL;
        }
    }

    *res = NO_ERR;
    return suite_ptr;

}  /* new_suite */


/********************************************************************
* FUNCTION clean_suite
*
* clean any result vars in the test suite
*
* INPUTS:
*   unit-test suite == struct to clean
*
*********************************************************************/
static void
    clean_suite (yangcli_ut_suite_t *suite)
{
    run_test_t *run_test = (run_test_t *)
        dlq_firstEntry(&suite->run_test_leaflistQ);
    for (; run_test != NULL; 
         run_test = (run_test_t *)dlq_nextEntry(run_test)) {
        clean_run_test(run_test);
    }

    yangcli_ut_test_t *test = (yangcli_ut_test_t *)
        dlq_firstEntry(&suite->test_listQ);
    for (; test != NULL;
         test = (yangcli_ut_test_t *)dlq_nextEntry(test)) {
        clean_test(test);
    }

    suite->suite_started = FALSE;
    suite->suite_errors = FALSE;

}  /* clean_suite */

/********************************************************************
 * FUNCTION get_top_obj_ut
 *
 * Get the unit-test top object
 *
 * RETURNS:
 *   pointer to object template for top object 'saved-sessions'
 *********************************************************************/
extern obj_template_t *
    get_top_obj_ut (void)
{
    ncx_module_t *mod = get_unit_test_mod();
    if (mod == NULL) {
        return NULL;
    }

    obj_template_t *ssobj =
        obj_find_template_top(mod, YANGCLI_TEST_MOD,
                              YANGCLI_TEST_SUITES);
    return ssobj;

}  /* get_top_obj_ut */

/********************************************************************
 * FUNCTION make_child_leaf_ut
 *
 * Make a leaf for the specified session_cfg child
 * and return it
 *
 * INPUTS:
 *  sesobj == session parent object
 *  childname == child leaf name of sesobj parent
 *  valstr == string value to set
 *  res == address of return status
 * OUTPUTS:
 *  *res == return status
 * RETURNS:
 *   pointer to malloced value node; must free with val_free_value
 *********************************************************************/
static val_value_t *
    make_child_leaf_ut (obj_template_t *obj,
                     const xmlChar *childname,
                     const xmlChar *valstr,
                     status_t *res)
{
    /* key session/name */
    obj_template_t *chobj =
        obj_find_child(obj, YANGCLI_TEST_MOD, childname);
    if (chobj == NULL) {
        log_error("\nError: missing '%s' element",
                  childname);
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }
    return val_make_simval_obj(chobj, valstr, res);

}  /* make_child_leaf_ut */

/********************************************************************
 * FUNCTION save_step_val
 *
 * Create a val_value_t representation of 1 step struct
 *
 * INPUT:
 *   step == step struct to use
 *   obj == object template to match yangcli_ut_step_t struct
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced val_value_t tree; need to free with val_free_value
 *********************************************************************/
static val_value_t *
    save_step_val (yangcli_ut_step_t *step,
                   obj_template_t *step_obj,
                         status_t *res)
{
    /* go through all the child nodes of the step element
     * and create a complete yangcli_ut_step_t struct
     */

    val_value_t *step_val = val_new_value();
    if (step_val == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(step_val, step_obj);

    val_value_t *chval = NULL;
    /* key step/name */

    chval = make_child_leaf_ut(step_obj, ut_suite_test_step_name,
                                         step->name, res);
    if (chval == NULL) {
        val_free_value(step_val);
        return NULL;
    }
    val_add_child(chval, step_val);

    /* generate the index chain in the indexQ */
    *res = val_gen_index_chain(step_obj, step_val);
    if (*res != NO_ERR) {
        val_free_value(step_val);
        return NULL;
    }

    /* The optional step/description leaf */
    if (step->description) {
        chval = make_child_leaf_ut(step_obj, ut_suite_test_step_description,
                                step->description, res);
        if (chval == NULL) {
            val_free_value(step_val);
            return NULL;
        }
        val_add_child(chval, step_val);
    } /* The optional test/step/description leaf */

    /* The optional step/session_name leaf */
    if (step->session_name) {
        chval = make_child_leaf_ut(step_obj, ut_suite_test_step_session_name,
                                step->session_name, res);
        if (chval == NULL) {
            val_free_value(step_val);
            return NULL;
        }
        val_add_child(chval, step_val);
    } /* The optional test/step/session_nameleaf */

    /* the mandatory command of this step */
    chval = make_child_leaf_ut(step_obj, ut_suite_test_step_command,
                                         step->command, res);
    if (chval == NULL) {
        val_free_value(step_val);
        return NULL;
    }
    val_add_child(chval, step_val);

     /* The result type of this step. */
     if (step->result_type) {
        const xmlChar *resp_typ_str =
              response_type_str(step->result_type);
        chval = make_child_leaf_ut(step_obj, ut_suite_test_step_result_type,
                                         resp_typ_str, res);
        if (chval == NULL) {
            val_free_value(step_val);
            return NULL;
        }
        val_add_child(chval, step_val);
     }

     /* The optional step/result_error_tag leaf */
     if (step->result_error_tag) {
         chval = make_child_leaf_ut (step_obj, ut_suite_test_step_result_error_tag,
                                step->result_error_tag, res);
         if (chval == NULL) {
            val_free_value(step_val);
            return NULL;
        }
        val_add_child(chval, step_val);
    } /* The optional test/step/result_error_tag leaf */

    /* The optional step/result_error_apptag leaf */
    if (step->result_error_apptag) {
        chval = make_child_leaf_ut(step_obj, ut_suite_test_step_result_error_apptag,
                                step->result_error_apptag, res);
        if (chval == NULL) {
            val_free_value(step_val);
            return NULL;
        }
        val_add_child(chval, step_val);
    } /* The optional test/step/result_error_tag leaf */

    /* create the result_error_info_t leaflist */
    result_error_info_t *info = (result_error_info_t *)
        dlq_firstEntry(&step->result_error_infoQ);

    for (; info != NULL;
        info = (result_error_info_t *)dlq_nextEntry(info)) {

        /* result_error_info/name */
        chval = make_child_leaf_ut(step_obj,
            ut_suite_test_step_result_error_info, info->result_error_info, res);
        if (chval == NULL) {
            val_free_value(step_val);
            return NULL;
        }
        val_add_child(chval, step_val);
    }

    *res = NO_ERR;
    return step_val;

} /* save_step_val */

/********************************************************************
 * FUNCTION save_test_val
 *
 * Create a val_value_t representation of 1 test struct
 *
 * INPUT:
 *   test == test struct to use
 *   obj == object template to match yangcli_ut_test_t struct
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced val_value_t tree; need to free with val_free_value
 *********************************************************************/
static val_value_t *
    save_test_val (yangcli_ut_test_t *test,
                   obj_template_t *test_obj,
                         status_t *res)
{
    /* go through all the child nodes of the suite element
     * and create a complete yangcli_ut_test_t struct
     */
    val_value_t *test_val = val_new_value();
    if (test_val == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(test_val, test_obj);

    /* key test/name */
    val_value_t *chval = make_child_leaf_ut(test_obj, ut_suite_test_name,
                                         test->test_name, res);

    if (chval == NULL) {
        val_free_value(test_val);
        return NULL;
    }
    val_add_child(chval, test_val);

   /* generate the index chain in the indexQ */
    *res = val_gen_index_chain(test_obj, test_val);
    if (*res != NO_ERR) {
        val_free_value(test_val);
        return NULL;
    }

   /* The optional test/description leaf */
    if (test->description) {
        chval = make_child_leaf_ut(test_obj, ut_suite_test_description,
                                test->description, res);
        if (chval == NULL) {
            val_free_value(test_val);
            return NULL;
        }
        val_add_child(chval, test_val);
    } /* The optional test/description leaf */

    /* copy Q of test_stepQ structs to val_value_t tree */
    /* list /unit-test/test-suite/test/step */
    obj_template_t *step_obj = obj_find_child(test_obj,
                YANGCLI_TEST_MOD, ut_suite_test_step);

    yangcli_ut_step_t *step = (yangcli_ut_step_t *)
        dlq_firstEntry(&test->step_listQ);

    for (; step != NULL;
        step = (yangcli_ut_step_t  *)dlq_nextEntry(step)) {
        val_value_t *step_val = save_step_val(step,
                                       step_obj, res);
        if (*res != NO_ERR) {
            val_free_value(test_val);
        } else {
            val_add_child(step_val, test_val);
        }
    }

    /* create the must-pass leaflist */
    mustpass_t *must = (mustpass_t *)
        dlq_firstEntry(&test->mustpass_leaflistQ);
    for (; must != NULL;
        must = (mustpass_t *)dlq_nextEntry(must)) {

        /* must-pass/name */
        chval = make_child_leaf_ut(test_obj,
               ut_suite_test_must_pass, must->mustpass_name, res);
        if (chval == NULL) {
            val_free_value(test_val);
            return NULL;
        }
        val_add_child(chval, test_val);
    }

    *res = NO_ERR;
   return test_val;

} /* save_test_val */


/********************************************************************
 * FUNCTION save_suite_val
 *
 * Create a val_value_t representation of 1 suite struct
 *
 * INPUT:
 *   suite == suite struct to use
 *   obj == object template to match yangcli_ut_suite_t struct
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced val_value_t tree; need to free with val_free_value
 *********************************************************************/
static val_value_t *
    save_suite_val (yangcli_ut_suite_t *suite,
                   obj_template_t *suite_obj,
                         status_t *res)
{
    /* go through all the child nodes of the suite element
     * and create a complete yangcli_ut_suite_t struct
     */
    val_value_t *suite_val = val_new_value();
    if (suite_val == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(suite_val, suite_obj);

    /* key test-suite/name */
    val_value_t *chval = make_child_leaf_ut(suite_obj,
                   ut_suite_name, suite->name, res);

   if (chval == NULL) {
        val_free_value(suite_val);
        return NULL;
    }
    val_add_child(chval, suite_val);

     /* generate the index chain in the indexQ */
    *res = val_gen_index_chain(suite_obj, suite_val);
    if (*res != NO_ERR) {
        val_free_value(suite_val);
        return NULL;
    }

   /* Optional test-suite/description leaf */
    if (suite->description) {
        chval = make_child_leaf_ut(suite_obj, ut_suite_description,
                                suite->description, res);
        if (chval == NULL) {
            val_free_value(suite_val);
            return NULL;
        }
        val_add_child(chval, suite_val);
    } /* the optional test-suite/description leaf */

   /* check the optional test-suite/setup block */
    if (!dlq_empty(&suite->setup_rawlineQ)) {
        /* get the container child node */
        obj_template_t *setup_obj =
            obj_find_child(suite_obj, YANGCLI_TEST_MOD,
                                    ut_suite_setup);
        /* create the setup container */
        val_value_t *setup_val = val_new_value();
        if (setup_val == NULL) {
            *res = ERR_INTERNAL_MEM;
            val_free_value(suite_val);
            return NULL;
        }

        val_init_from_template(setup_val, setup_obj);
        val_add_child(setup_val, suite_val);

        xmlns_id_t nsid = ncx_get_mod_nsid(get_unit_test_mod());
        rawline_t *rawline =
            (rawline_t *)dlq_firstEntry(&suite->setup_rawlineQ);
        for (; rawline != NULL;
            rawline = (rawline_t *)dlq_nextEntry(rawline)) {
            chval = val_make_string(nsid, NCX_EL_STRING,
                                    rawline->line);
            if (chval == NULL) {
                *res = ERR_INTERNAL_MEM;
                val_free_value(suite_val);
                return NULL;
            }
            val_add_child(chval, setup_val);
         }
    }  /* The optional test-suite/setup block */

    /* The optional test-suite/cleanup block */
    if (!dlq_empty(&suite->cleanup_rawlineQ)) {
        /* get the container child node */
        obj_template_t *cleanup_obj =
            obj_find_child(suite_obj, YANGCLI_TEST_MOD,
                                   ut_suite_cleanup);
        /* create the cleanup container */
        val_value_t *cleanup_val = val_new_value();
        if (cleanup_val == NULL) {
            *res = ERR_INTERNAL_MEM;
            val_free_value(suite_val);
            return NULL;
        }
        val_init_from_template(cleanup_val, cleanup_obj);
        val_add_child(cleanup_val, suite_val);
        xmlns_id_t nsid = ncx_get_mod_nsid(get_unit_test_mod());
        rawline_t *rawline =
            (rawline_t *)dlq_firstEntry(&suite->cleanup_rawlineQ);
        for (; rawline != NULL;
            rawline = (rawline_t *)dlq_nextEntry(rawline)) {
            chval = val_make_string(nsid, NCX_EL_STRING,
                                    rawline->line);
            if (chval == NULL) {
                *res = ERR_INTERNAL_MEM;
                val_free_value(suite_val);
                return NULL;
            }
            val_add_child(chval, cleanup_val);
         }
     } /* The optional test-suite/cleanup block */

    /* copy Q of runtest_leaflistQ structs to val_value_t tree */
    /* leaf-list /unit-test/test-suite/run-test */

    /* create the run-test leaflist */
    run_test_t *run = (run_test_t *)
        dlq_firstEntry(&suite->run_test_leaflistQ);
    for (; run != NULL;
        run = (run_test_t *)dlq_nextEntry(run)) {
        /* run-test/name */
        val_value_t *run_chval = make_child_leaf_ut(suite_obj,
               ut_suite_run_test, run->run_test_name, res);
        if (chval == NULL) {
            val_free_value(suite_val);
            return NULL;
        }
        val_add_child(run_chval, suite_val);
    }

    /* copy Q of test_listQ structs to val_value_t tree */
    /* leaf-list /unit-test/test-suite/test */
    obj_template_t *test_obj = obj_find_child(suite_obj,
                    YANGCLI_TEST_MOD, ut_suite_test);
    yangcli_ut_test_t *test = (yangcli_ut_test_t *)
        dlq_firstEntry(&suite->test_listQ);
    for (; test != NULL;
        test = (yangcli_ut_test_t *)dlq_nextEntry(test)) {
        val_value_t *test_val = save_test_val(test,
                                     test_obj, res);
        if (*res != NO_ERR) {
            val_free_value(suite_val);
        } else {
            val_add_child(test_val, suite_val);
        }
    }

    *res = NO_ERR;
    return suite_val;

} /* save_suite_val */


/**************    E X T E R N A L   F U N C T I O N S **********/

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
    fullspec_for_ut_data ( xmlChar *suite_name,
                           xmlChar *test_name,
                           xmlChar *step_name,
                           boolean  recording,
                           status_t* res)
{
    #define   BUFFLEN 128
    char      buff[BUFFLEN];
    xmlChar   *fullspec;

    strcpy(buff, "~/.yumapro/recordtest/");
    strcat(buff, (char*) suite_name);
    strcat(buff, "_" );
    strcat(buff, (char*) test_name);
    strcat(buff, "_" );
    strcat(buff, (char*) step_name);
    strcat(buff, "_" );
    if (recording == TRUE) {
        strcat(buff, "record");
    } else {
        strcat(buff, "run");
    }
    strcat(buff, ".xml");

    fullspec = ncx_get_source((const xmlChar *) buff, res);

    return fullspec;

} /* fullspec_for_ut_data */

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
                 const xmlChar    *test_name)
{
    yangcli_ut_suite_t *suite_ut = NULL;
    yangcli_ut_context_t *context_ut =
                   get_ut_context(server_cb);

    /* check if the suite name exists. */
    suite_ut = find_suite (context_ut, NULL, suite_name);

     if (suite_ut == NULL) {
         return FALSE;
     } else {
         /* The suite name exists.
          * Check if the test_name exists.
          */
         yangcli_ut_test_t *test_ut = find_test(
                            suite_ut, test_name);
         if (test_ut != NULL) {
             log_info("\nsuite name %s, test name %s already exists\n",
                            suite_name, test_name);
             return TRUE;
         } else {
             return FALSE;
         }
    }
}

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
                     yangcli_ut_context_t *context_rt,
                     const xmlChar        *suite_name,
                     const xmlChar        *test_name)
{
    yangcli_ut_suite_t *suite_ut = NULL;
    yangcli_ut_suite_t *suite_rt = NULL;

    yangcli_ut_context_t *context_ut =
                       get_ut_context(server_cb);

    /* check if the suite name is a new or existing one. */
    suite_ut = find_suite (context_ut, NULL, suite_name);
    suite_rt = find_suite (context_rt, NULL, suite_name);

    /* 
     *  For a new suite, add it to the ut suiteQ 
     *  For an existing suite name, add the test if it is a  
     *  a new test name. 
     */
     if (suite_ut == NULL) {
         /* Add the new suite to the ut suiteQ */
         dlq_enque(suite_rt, &context_ut->suite_listQ);
         return NO_ERR;
     } else {
         /* The suite name is not new. 
          * Check if the test_name is new.
          */
         yangcli_ut_test_t *test_ut = find_test( 
                              suite_ut, test_name);

         if (test_ut != NULL) {
             log_info("\nDuplicate suite-name %s, test-name %s\n",
                            suite_name, test_name);
             return ERR_NCX_OPERATION_FAILED;
         } else {
            /* Add this new test to an existing suite. */
            /* Remove the test from record's test_listQ */

            yangcli_ut_test_t *test_rt = (yangcli_ut_test_t  *)
                              dlq_deque(&suite_rt->test_listQ);

           /* Add this recorded test to ut test_listQ */
           dlq_enque(test_rt, &suite_ut->test_listQ);
          
           /* Also add the test name to the run test list */
           run_test_t *runtest = new_run_test(test_name);
           if (runtest) {
               dlq_enque(runtest, &suite_ut->run_test_leaflistQ);
           } else {
               dlq_deque(&suite_ut->test_listQ);
               free_test(test_rt);
               return ERR_INTERNAL_MEM;
           }
           return NO_ERR;
       }
    }

}  /* add_to_ut_suiteQ_testQ_runQ */


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
    response_type_str (response_type_t rt)

{
    switch (rt) {
    case UT_RPC_NONE:
    case UT_RPC_NO:
    case UT_RPC_ANY:
        return result_type_is_none;
    case UT_RPC_OK:
        return result_type_is_ok;
    case UT_RPC_DATA:
        return result_type_is_data;
    case UT_RPC_ERROR:
        return result_type_is_error;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return EMPTY_STRING;
    }

}  /* response_type_str */


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
                   yangcli_ut_suite_t *suite_ptr)
{
    if (suite_ptr) {
        clean_suite(suite_ptr);
    } else {
        yangcli_ut_suite_t *suite = (yangcli_ut_suite_t *)
            dlq_firstEntry(&context->suite_listQ);
        for (; suite != NULL;
             suite = (yangcli_ut_suite_t *)dlq_nextEntry(suite)) {
            clean_suite(suite);
        }
    }

    if (context->ut_state == UT_STATE_ERROR && context->logfile &&
        context->logfile_open) {
        log_close();
    }

    context->ut_state = UT_STATE_READY;
    context->ut_input_mode = FALSE;
    m__free(context->logfile);
    context->logfile = NULL;
    context->logfile_open = FALSE;
    m__free(context->test_sesname);
    context->test_sesname = NULL;
    m__free(context->linebuff);
    context->linebuff = NULL;
    context->linebuff_size = 0;
    context->cur_suite = NULL;
    context->cur_run_test = NULL;
    context->cur_step = NULL;
    context->cur_rawline = NULL;

}  /* clean_context */


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
    free_context_cache (yangcli_ut_context_t *context)
{
    while (!dlq_empty(&context->suite_listQ)) {
        yangcli_ut_suite_t *suitelist = (yangcli_ut_suite_t *)
            dlq_deque(&context->suite_listQ);
        free_suite(suitelist);
    }
    m__free(context->logfile);
    m__free(context->test_sesname);
    m__free(context->linebuff);
    memset(context, 0x0, sizeof(yangcli_ut_context_t));

}  /* free_context_cache */

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
                const xmlChar *suite_name)
{
    yangcli_ut_suite_t *ptr;
    for (ptr = (yangcli_ut_suite_t *)
             dlq_firstEntry(&context->suite_listQ);
         ptr != NULL;
         ptr = (yangcli_ut_suite_t *)dlq_nextEntry(ptr)) {

        if (!xml_strcmp(ptr->name, suite_name)) {
            return ptr;
        }
    }
    if (tempQ) {
        for (ptr = (yangcli_ut_suite_t *)dlq_firstEntry(tempQ);
             ptr != NULL;
             ptr = (yangcli_ut_suite_t *)dlq_nextEntry(ptr)) {

            if (!xml_strcmp(ptr->name, suite_name)) {
                return ptr;
            }
        }
    }

    return NULL;

}  /* find_suite */


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
    free_suite (yangcli_ut_suite_t *suite)
{

   if (!suite) {
        return;
    }

    while (!dlq_empty(&suite->run_test_leaflistQ)) {
        run_test_t *run_test = (run_test_t *)
            dlq_deque(&suite->run_test_leaflistQ);
        free_run_test(run_test);
    }

    while (!dlq_empty(&suite->test_listQ)) {
        yangcli_ut_test_t *test = (yangcli_ut_test_t  *)
            dlq_deque(&suite->test_listQ);
        free_test(test);
    }

    if (!dlq_empty(&suite->setup_rawlineQ)) {
        while (!dlq_empty(&suite->setup_rawlineQ)) {
            rawline_t *rawline = (rawline_t *)
                dlq_deque(&suite->setup_rawlineQ);
            free_rawline(rawline);
        }
    }

    if (!dlq_empty(&suite->cleanup_rawlineQ)) {
        while (!dlq_empty(&suite->cleanup_rawlineQ)) {
            rawline_t *rawline = (rawline_t *)
            dlq_deque(&suite->cleanup_rawlineQ);
            free_rawline(rawline);
        }
    }

    m__free(suite->name);
    m__free(suite->description);
    m__free(suite);

}  /* free_suite */

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
               const xmlChar *test_name)
{
    yangcli_ut_test_t *ptr;

    for (ptr = (yangcli_ut_test_t *)dlq_firstEntry(&suite->test_listQ);
         ptr != NULL;
         ptr = (yangcli_ut_test_t *)dlq_nextEntry(ptr)) {

        if (!xml_strcmp(ptr->test_name, test_name)) {
            return ptr;
        }
    }
    return NULL;

}  /* find_test */


/********************************************************************
* FUNCTION free_test
*
* free a pointer
*
* INPUTS:
*   test_ptr == a run_test to free
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
extern void
    free_test (yangcli_ut_test_t *test_ptr)
{
    if (!test_ptr) {
        return;
    }

    if (test_ptr->test_name) {
        m__free(test_ptr->test_name);
    }

    if (test_ptr->description) {
        m__free(test_ptr->description);
    }

    if (!dlq_empty(&test_ptr->mustpass_leaflistQ)) {
        while (!dlq_empty(&test_ptr->mustpass_leaflistQ)) {
            mustpass_t *mptr = (mustpass_t *)
                dlq_deque(&test_ptr->mustpass_leaflistQ);
            free_mustpass_name(mptr);
        }
    }

    if (!dlq_empty(&test_ptr->step_listQ)) {
        while (!dlq_empty(&test_ptr->step_listQ)) {
            yangcli_ut_step_t *step = (yangcli_ut_step_t *)
                dlq_deque(&test_ptr->step_listQ);
            free_step(step);
        }
    }

    m__free(test_ptr);

}  /* free_test */


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
    free_step (yangcli_ut_step_t *step)
{
    if (!step) {
        return;
    }

    result_error_info_t *ptr;
    while (!dlq_empty(&step->result_error_infoQ)) {
        ptr = (result_error_info_t *)
            dlq_deque(&step->result_error_infoQ);
        free_result_error_info (ptr);
    }

    m__free(step->name);
    m__free(step->description);
    m__free(step->session_name);
    m__free(step->result_error_tag);
    m__free(step->result_error_apptag);
    m__free(step->step_error_tag);
    m__free(step->step_error_apptag);
    m__free(step->command);
    // TBD anyxml data
    m__free(step);

}  /* free_step */


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
    free_mustpass_name (mustpass_t *ptr)
{
    if (!ptr) {
        return;
    }
    m__free(ptr->mustpass_name);
    m__free(ptr);

}  /* free_mustpass_ptr */


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
    free_result_error_info (result_error_info_t *ptr)
{
    if (!ptr) {
        return;
    }

    m__free(ptr->result_error_info);
    m__free(ptr);

}  /* free_result_error_info */



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
    free_run_test (run_test_t *run_test_ptr)
{
    if (!run_test_ptr) {
        return;
    }
    m__free(run_test_ptr->run_test_name);
    m__free(run_test_ptr);

}  /* free_run_test */



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
                       const xmlChar *fspec)
{
    status_t res = NO_ERR;
    const xmlChar *testconf = fspec;

    obj_template_t *ut_obj = get_top_obj_ut();
    if (ut_obj == NULL) {
        log_error("\nError: yumaworks-test.yang is missing or "
                      "wrong version; no test saved");
        return ERR_NCX_OPERATION_FAILED;
    }

    val_value_t *ut_val = val_new_value();
    if (ut_val == NULL) {
        return ERR_INTERNAL_MEM;
    }

    xmlChar *fullspec = ncx_get_source(testconf, &res);

    val_init_from_template(ut_val, ut_obj);

    obj_template_t *suite_obj = obj_find_child(ut_obj,
                      YANGCLI_TEST_MOD, ut_suite);

    /* copy Q of suite_listQ structs to val_value_t tree */
    yangcli_ut_suite_t *suite = (yangcli_ut_suite_t *)
        dlq_firstEntry(&context->suite_listQ);

    for (; res == NO_ERR && suite != NULL;
        suite = (yangcli_ut_suite_t *)dlq_nextEntry(suite)) {
        val_value_t *suite_val = save_suite_val(suite,
                                        suite_obj, &res);
        if (res != NO_ERR) {
            m__free(fullspec);
            val_free_value(suite_val);
        } else {
            val_add_child(suite_val, ut_val);
        }
    }
#ifdef YANGCLI_UT_DEBUG
    if (LOGDEBUG3) {
        log_debug3("\nAbout to write recorded test file '%s':\n", testconf);
        val_dump_value_full(ut_val,
                            0,   /* startindent */
                            get_defindent(),
                            DUMP_VAL_LOG, /* dumpmode */
                            NCX_DISPLAY_MODE_PLAIN,
                            FALSE,    /* withmeta */
                            FALSE,   /* configonly */
                            TRUE);   /* conf_mode */
    }
#endif
    if (res == NO_ERR) {
        /* write the value tree as a config file */
        res = log_alt_open_ex((const char *)fullspec, TRUE);
        if (res != NO_ERR) {
            log_error("\nError: sessions file '%s' could "
                        "not be opened (%s)",
                        fullspec, get_error_string(res));
        } else {
            val_dump_value_full(ut_val,
                                0,   /* startindent */
                                get_defindent(),
                                DUMP_VAL_ALT_LOG, /* dumpmode */
                                NCX_DISPLAY_MODE_PLAIN,
                                FALSE,    /* withmeta */
                                FALSE,   /* configonly */
                                TRUE);   /* conf_mode */
            log_alt_close();
        }
    }
    m__free(fullspec);
    val_free_value(ut_val);
    return NO_ERR;

}  /* yangcli_testfile_save */

/********************************************************************
 * FUNCTION do_test_suite (local RPC)
 * 
 * test-suite load[=filespec]
 * test-suite save[=filespec]
 * test-suite show
 * test-suite start=suite-name
 * test-suite run-all
 * test-suite delete=suite-name
 * test-suite delete-test test-name=x test-name=x
 *
 *
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
status_t
    do_test_suite (server_cb_t *server_cb,
                   obj_template_t *rpc,
                   const xmlChar *line,
                   uint32  len)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    status_t res = NO_ERR;
    val_value_t *parm = NULL;
    val_value_t *valset = get_valset(server_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
        const xmlChar *parmval = NULL;
        boolean done = FALSE;

        /* get the 1 of N 'test-suite-action' choice */            

        /* test-suite load */
        if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_LOAD);
            if (parm) {
                if (ut_state_busy(context)) {
                    log_error("\nError: tests are active, "
                              "cannot load test-suites\n");
                    res = ERR_NCX_IN_USE;
                } else {
                    parmval = VAL_STR(parm);
                    if (*parmval == 0) {
                        parmval = get_test_suite_file();
                    }
                
                    boolean file_error = !val_set_by_default(parm);
                    res = yangcli_ut_load(server_cb, 
                                          parmval, file_error);
                    if (res == NO_ERR) {
                        log_info("\nLoaded test-suites OK from '%s'\n",
                                 parmval);
                    } else {
                        log_error("\nLoad test suites from '%s' "
                                  "failed (%s)\n", parmval, 
                                  get_error_string(res));
                    }
                }
                done = TRUE;
            }
        }

        /* test-suite delete suite-name=xxx */
       if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_DELETE);
            if (parm) {
               if (ut_state_busy(context)) {
                    log_error("\nError: tests are active, "
                              "cannot delete test-suite\n");
                    res = ERR_NCX_IN_USE;
                } else {
                    res = yangcli_ut_delete_suite(server_cb, valset);
                }

                done = TRUE;
            }
        }

       /* test-suite delete-test suite-name=xxx test-name=xxx*/
       if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_DELETE_TEST);
            if (parm) {
              if (ut_state_busy(context)) {
                    log_error("\nError: tests are active, "
                              "cannot delete test-suite\n");
                    res = ERR_NCX_IN_USE;
                } else {
                    res = yangcli_ut_delete_test(server_cb, valset);
                }


                done = TRUE;
            }
        }

        /* test-suite run-all or start */
        if (!done) {
            context->single_suite = TRUE;

            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_RUN_ALL);
            if (parm) {
                context->single_suite = FALSE;
                yangcli_ut_suite_t*  ptr = (yangcli_ut_suite_t *)
                          dlq_firstEntry(&context->suite_listQ);
                parmval = ptr->name;
            } else {
                parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_START);
                if (parm) {
                   parmval = VAL_STR(parm);
                }
            }

            if (parm) {
                if (ut_state_busy(context)) {
                    log_error("\nError: tests are active, "
                              "cannot start another test-suite\n");
                    res = ERR_NCX_IN_USE;
                } else {
                    const xmlChar *logparmval = NULL;
                    val_value_t *logparm =
                        val_find_child(valset, YANGCLI_MOD, NCX_EL_LOG);
                    if (logparm) {
                        logparmval = VAL_STR(logparm);
                    }
                    res = yangcli_ut_start(server_cb, parmval,
                                                  logparmval);
                }
                done = TRUE;
            }
        }

        /* test-suite save */
        if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SAVE);
            if (parm) {
                parmval = VAL_STR(parm);
                if (*parmval == 0) {
                    parmval = get_test_suite_file();
                }

                res = yangcli_ut_save(server_cb, parmval);
                if (res == NO_ERR) {
                    log_info("\nSaved test suites OK to '%s'\n",
                             parmval);
                } else {
                    log_error("\nSave test suites to '%s' "
                              "failed (%s)\n", parmval,
                              get_error_string(res));
                }
                done = TRUE;
            }
        }

        /* test-suite show : last because it is the default case */
        if (!done) {
            /* this is the default, so don't really need to get this parm */
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SHOW);
            help_mode_t mode = (parm && !val_set_by_default(parm)) 
                ? HELP_MODE_NORMAL : HELP_MODE_BRIEF;
            
            /* check if the 'brief' or 'full' flags are set first */
            val_value_t *showparm = 
                val_find_child(valset, YANGCLI_MOD, YANGCLI_BRIEF);
            if (showparm && showparm->res == NO_ERR) {
                mode = HELP_MODE_BRIEF;
            } else {
                showparm = val_find_child(valset, YANGCLI_MOD, 
                                          YANGCLI_FULL);
                if (showparm && showparm->res == NO_ERR) {
                    mode = HELP_MODE_FULL;
                } else {
                    showparm = val_find_child(valset, YANGCLI_MOD, 
                                              YANGCLI_NORMAL);
                    if (showparm && showparm->res == NO_ERR) {
                        mode = HELP_MODE_NORMAL;
                    }
                }
            }
            show_test_suites(context, mode);
            done = TRUE;
        }

        log_info_append("\n");
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

    }  /* do_test_suite */


/********************************************************************
* FUNCTION yangcli_ut_init
*
* INPUTS: 
*    server_cb == server control block to use
* RETURNS:
*   none
*********************************************************************/
    void
    yangcli_ut_init (server_cb_t *server_cb)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    memset(context, 0x0, sizeof(yangcli_ut_context_t));
    dlq_createSQue(&context->suite_listQ);
    context->ut_state = UT_STATE_INIT;

} /* yangcli_ut_init */


/********************************************************************
* FUNCTION yangcli_ut_cleanup
*
* Cleanup the yangcli-unit-test access control module
*
* INPUTS:
*   server_cb == server context to use
* RETURNS:
*   none
*********************************************************************/
void
    yangcli_ut_cleanup (server_cb_t *server_cb)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    free_context_cache(context);

} /* yangcli_ut_cleanup */ 


/********************************************************************
 * FUNCTION yangcli_ut_load
 * 
 * Load the unit-test config file into memory
 *
 * INPUTS:
 *   server_cb == server context to use
 *   testconf == test suite config filespec
 *   file_error == TRUE to treat unopened file as an error
 *              == FALSE to treat unopened file as no-error
 * RETURNS:
 *  status
 *********************************************************************/
status_t
    yangcli_ut_load (server_cb_t *server_cb,
                     const xmlChar *testconf,
                     boolean file_error)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (ut_state_busy(context)) {
        log_error("\nError: test suite active\n");
        return ERR_NCX_IN_USE;
    }
        
    status_t res = NO_ERR;

    ncx_module_t *unit_test_mod = get_unit_test_mod();
    if (unit_test_mod == NULL) {
        log_error("\nError: '%s' module not found", 
                  YANGCLI_TEST_MOD);
        return ERR_NCX_OPERATION_FAILED;
    }

    obj_template_t *unit_test_obj = 
        obj_find_template_top(unit_test_mod,
                              YANGCLI_TEST_MOD,
                              YANGCLI_TEST_SUITES);
    if (unit_test_obj == NULL) {
        log_error("\nError: unit-test object not found");
        return ERR_NCX_DEF_NOT_FOUND;
    }

    val_value_t *unit_test_val = val_new_value();
    if (unit_test_val == NULL) {
        log_error("\nError: malloc failed");
        return ERR_INTERNAL_MEM;
    }

    val_init_from_template(unit_test_val, unit_test_obj);
    boolean opened = FALSE;
    const xmlChar *banner = (const xmlChar *)"test suites";
    res = conf_parse_val_from_filespec_ex(testconf, banner, unit_test_val,
                                          TRUE, file_error, &opened);
    if (res != NO_ERR) {
        log_error("\nError: Parse test suite file '%s' failed (%s)", 
                  testconf, get_error_string(res));
    } else if (!opened) {
        if (file_error) {
            log_error("\nError: Test suite file '%s' not opened", testconf);
            res = ERR_FIL_OPEN;
        } else {
            log_debug("\nTest suite file '%s' not opened", testconf);
        }
    } else if (LOGDEBUG2) {
        log_debug2("\nUnit test config '%s':\n", testconf);
        val_dump_value(unit_test_val, 2);
    } else if (LOGDEBUG) {
        log_debug("\nUnit test config '%s' parsed OK", testconf);
    }

    if (res == NO_ERR && opened) {
        dlq_hdr_t tempQ;
        dlq_createSQue(&tempQ);

        /* convert the val_value_t tree to C data structures */
        val_value_t *ut_val = val_get_first_child(unit_test_val);
        for (; res == NO_ERR && ut_val != NULL;
             ut_val = val_get_next_child(ut_val)) {
            yangcli_ut_suite_t *suite = 
                new_suite(context, ut_val, &tempQ, &res);
            if (res != NO_ERR) {
                free_suite(suite);
            } else {
                dlq_enque(suite, &tempQ);
            }
        }

        if (res == NO_ERR) {
            dlq_block_enque(&tempQ, &context->suite_listQ);
            context->ut_state = UT_STATE_READY;
        } else {
            while (!dlq_empty(&tempQ)) {
                yangcli_ut_suite_t *suitelist = 
                    (yangcli_ut_suite_t *)dlq_deque(&tempQ);
                free_suite(suitelist);
            }
        }
    }

    val_free_value(unit_test_val);

    /* Save mtime of this test file */
    if (res == NO_ERR && opened) {
        xmlChar   *fullspec;
        fullspec = ncx_get_source(testconf, &res);
        res = update_def_yangcli_file_mtime (TESTSUITE_FILE, fullspec);
        m__free(fullspec);
    }

    return res;

}  /* yangcli_ut_load */


/********************************************************************
 * FUNCTION yangcli_ut_save
 *
 * Save test to the unit-test config file
 *
 * INPUTS:
 *   server_cb == server context to use
 *   testconf == test suite config filespec
 *
 * RETURNS:
 *  status
 *********************************************************************/
status_t
    yangcli_ut_save (server_cb_t *server_cb,
                     const xmlChar *fspec)
{
     yangcli_ut_context_t *context =
            get_ut_context(server_cb);

    status_t res = yangcli_testfile_save (context, fspec);

    return res;
}/* yangcli_ut_save */

/********************************************************************
* FUNCTION yangcli_ut_delete_suite
*
* Begin delete suite process.
*
* INPUTS:
*   server_cb == server context to use
* RETURNS:
*   status
*********************************************************************/
status_t
    yangcli_ut_delete_suite (server_cb_t *server_cb,
                             val_value_t *valset)
{
    status_t res = NO_ERR;
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (context == NULL) {
        return ERR_INTERNAL_INIT_SEQ;
    }

    /* make sure right state to delete a suite */
    if (ut_state_busy(context)) {
        context->ut_state = UT_STATE_ERROR;
        return ERR_INTERNAL_INIT_SEQ;
    }

    val_value_t*  val_suite = val_find_child(valset,
             YANGCLI_MOD, YANGCLI_RECORD_SUITENAME);
    if (val_suite) {
        xmlChar *suite_name = VAL_STR(val_suite);
        
        yangcli_ut_suite_t *suite = find_suite( 
                         context, NULL, suite_name);
        if (suite != NULL) {
            dlq_remove(suite);
            free_suite(suite);
            /* updated ut test file */
            const xmlChar *ut_file = get_test_suite_file();
            res = yangcli_ut_save(server_cb, ut_file);
            log_info(
              "\nsuite-name='%s'is deleted.", suite_name);
        } else {
             log_error("\nError: test-suite '%s' not found",
                  suite_name);
             return ERR_NCX_NOT_FOUND;
         } 
    }

    return res;
} /* yangcli_ut_delete_suite */

/********************************************************************
* FUNCTION yangcli_ut_delete_test
*
* Begin delete a test.
*
* INPUTS:
*   server_cb == server context to use
* RETURNS: 
*   status
*********************************************************************/
status_t
    yangcli_ut_delete_test (server_cb_t *server_cb,
                            val_value_t *valset)
{
    status_t res = NO_ERR;
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (context == NULL) {
        return ERR_INTERNAL_INIT_SEQ;
    }

    /* make sure right state to delete a test */
    if (ut_state_busy(context)) {
        context->ut_state = UT_STATE_ERROR;
        return ERR_INTERNAL_INIT_SEQ;
    }  

    val_value_t*  val_suite = val_find_child(valset,
             YANGCLI_MOD, YANGCLI_RECORD_SUITENAME);
    if (!val_suite) {
        return ERR_NCX_NOT_FOUND;
    } else {
        xmlChar *suite_name = VAL_STR(val_suite);
        yangcli_ut_suite_t *suite = 
                    find_suite(context, NULL, suite_name);
        if (suite == NULL) {
             log_error("\nError: test-suite '%s' not found",
                  suite_name);
             return ERR_NCX_NOT_FOUND;
         } else {
             val_value_t*  val_test = val_find_child(valset,
             YANGCLI_MOD, YANGCLI_RECORD_TESTNAME);
             if (!val_test) {
                 return ERR_NCX_NOT_FOUND;
             } else {
                  xmlChar *test_name = VAL_STR(val_test);
                  yangcli_ut_test_t *test = 
                             find_test(suite, test_name);
                  if (test == NULL) {
                      log_error(
                      "\nError:suite-name='%s',test-name='%s' not found", 
                                     suite_name, test_name);
                      return ERR_NCX_NOT_FOUND;
                  }else{
                      dlq_remove(test);
                      free_test(test);
                      log_info(
                      "\nsuite-name='%s',test-name='%s' is deleted ",
                                             suite_name, test_name);
                  }
                   run_test_t *runtest =
                          find_run_test (suite, test_name);
                   if (runtest !=NULL) {
                       dlq_remove(runtest);
                       free_run_test(runtest);
                   }

                   /* updated ut test file */
                   const xmlChar *ut_file = get_test_suite_file();
                   res = yangcli_ut_save(server_cb, ut_file);
              }
          }
     }

    return res;
} /* yangcli_ut_delete_test */      

/********************************************************************
* FUNCTION yangcli_ut_start
*
* Begin testing process.
*
* INPUTS:
*   server_cb == server context to use
*   suite_name == name of test-suite to run; tests must already be
*           loaded into memory
*   logfile == logfile to use; NULL = STDOUT
* RETURNS:
*   status
*********************************************************************/
status_t
    yangcli_ut_start (server_cb_t *server_cb,
                      const xmlChar *suite_name,
                      const xmlChar *logfile)
{
    status_t res = NO_ERR;
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (context == NULL) {
        return ERR_INTERNAL_INIT_SEQ;
    }

    /* make sure right state to start a test */
    if (ut_state_busy(context)) {
        context->ut_state = UT_STATE_ERROR;
        return ERR_INTERNAL_INIT_SEQ;
    }

    /* find the test suite */
    yangcli_ut_suite_t *suite = find_suite(context, NULL, suite_name);
    if (suite == NULL) {
        log_error("\nError: test-suite '%s' not found",
                  suite_name);
        context->ut_state = UT_STATE_ERROR;
        return ERR_NCX_NOT_FOUND;
    }

    /* make sure there is at least one run-test statement */
    run_test_t *runtest = 
        (run_test_t *)dlq_firstEntry(&suite->run_test_leaflistQ);
    if (runtest == NULL) {
        log_info("\nTest-suite '%s' has no run-test statements; "
                 "exiting test suite", suite_name);
        context->ut_state = UT_STATE_ERROR;
        return ERR_NCX_SKIPPED;
    }

    /* check if this whole session is logged, and logging requested
     * for this test; not supported yet
     */
    if (log_is_open() && logfile) {
        log_error("\nError: log file is already open so "
                  "test log '%s' cannot be opened", logfile);
        return ERR_NCX_RESOURCE_DENIED;
    }

    /* make sure the test state is ready to run */
    if (context->ut_state != UT_STATE_READY) {
        clean_context(context, suite);
    }

    /* make sure all the run-test statements reference real tests
     * store back-ptrs to the test to run in each run-test stmt
     */
    run_test_t *duptest = runtest;
    for (; duptest != NULL;
         duptest = (run_test_t *)dlq_nextEntry(duptest)) {

        yangcli_ut_test_t *test = find_test(suite, duptest->run_test_name);
        if (test == NULL) {
            log_info("\nTest-suite '%s' has no test named '%s'; "
                     "exiting test suite", suite_name, 
                     runtest->run_test_name);
            context->ut_state = UT_STATE_ERROR;
            return ERR_NCX_NOT_FOUND;
        } else {
            duptest->test = test;
        }
    }

    /* expand the logfile if specified */
    xmlChar *newlogfile = NULL;
    if (logfile) {
        newlogfile = ncx_get_source(logfile, &res);
        if (res != NO_ERR) {
            context->ut_state = UT_STATE_ERROR;
            return res;
        }
        context->logfile = newlogfile;
        res = log_open((const char *)newlogfile, FALSE, TRUE);
        if (res == NO_ERR) {
            context->logfile_open = TRUE;
        } else {
            context->ut_state = UT_STATE_ERROR;
            log_error("\nCannot open logfile '%s' (%s)",
                      newlogfile, get_error_string(res));
            return res;
        }
    }

    session_cb_t *session_cb = server_cb->cur_session_cb;
    context->test_sesname = xml_strdup(get_session_name(session_cb));
    if (context->test_sesname == NULL) {
        context->ut_state = UT_STATE_ERROR;
        return ERR_INTERNAL_MEM;
    }

    /* setup state to start test */
    suite->suite_errors = FALSE;
    suite->suite_started = TRUE;
    if (!dlq_empty(&suite->setup_rawlineQ)) {
        context->ut_state = UT_STATE_SETUP;
    } else {
        context->ut_state = UT_STATE_RUNTEST;
    }
    context->cur_suite = suite;
    context->cur_run_test = runtest;
    context->ut_input_mode = TRUE;
    context->ut_status = NO_ERR;

    start_suite_banner(context);

    return NO_ERR;

} /* yangcli_ut_start */


/********************************************************************
* FUNCTION yangcli_ut_active
*
* Check if the test-suite input mode is active
*
* INPUTS:
*   server_cb == server context to use
*
* RETURNS:
*   TRUE if test-suite input mode active
*   FALSE if test-suite input mode not active
*********************************************************************/
boolean
    yangcli_ut_active (server_cb_t *server_cb)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    return context->ut_input_mode;

}  /* yangcli_ut_active */


/********************************************************************
* FUNCTION yangcli_ut_getline
*
* Get an input line from the autotest
*
* INPUTS:
*   server_cb == server context to use
*   res == address of return status
*
* OUTPUTS:
*   *res == return status
*    will be set to ERR_NCX_SKIPPED if test-suite is done
* RETURNS:
*   pointer to input line to use; do not free!!!
*   NULL if no more input or some error
*********************************************************************/
xmlChar *
    yangcli_ut_getline (server_cb_t *server_cb,
                        status_t *res)
{
    boolean final_finish = FALSE;
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (!ut_state_busy(context)) {
        *res = ERR_INTERNAL_INIT_SEQ;
        return NULL;
    }

    yangcli_ut_suite_t *suite = context->cur_suite;
    yangcli_ut_test_t *test = (context->cur_run_test) ?
        context->cur_run_test->test : NULL;
    xmlChar *line = NULL;
    boolean done = FALSE;

    switch (context->ut_state) {
    case UT_STATE_SETUP:
        /* the the next rawline in the setup rawline Q */
        if (context->cur_rawline == NULL) {
            context->cur_rawline = (rawline_t *)
                dlq_firstEntry(&suite->setup_rawlineQ);
            start_setup_banner(context);
        } else {
            context->cur_rawline = (rawline_t *)
                dlq_nextEntry(context->cur_rawline);
        }

        if (context->cur_rawline != NULL) {
            line = get_full_rawline(context, res);
            break;
        } else {
            context->ut_state = UT_STATE_RUNTEST;
        }
        // fall through it done with setup phase
    case UT_STATE_RUNTEST:
        if (test == NULL) {
            done = TRUE;
        }
        while (!done) {
            if (context->cur_step == NULL) {
                context->cur_step = (yangcli_ut_step_t *)
                    dlq_firstEntry(&test->step_listQ);
                start_test_banner(context);
                test->test_started = TRUE;
            } else {
                context->cur_step = (yangcli_ut_step_t *)
                    dlq_nextEntry(context->cur_step);
            }

            if (context->cur_step != NULL) {
                line = context->cur_step->command;
                start_step_banner(context, line);

                *res = NO_ERR;
                done = TRUE;
            } else {
                /* test is complete */
                report_test_results(context);

                run_test_t *run_test = (run_test_t *)
                     dlq_nextEntry(context->cur_run_test);
                /* 
                 * Find next run test which does not have a failed must pass
                 */
                context->cur_run_test = 
                     run_test_with_no_failed_mustpass (run_test);

                if (context->cur_run_test) {
                    test = context->cur_run_test->test;
                    start_test_banner(context);
                } else {
                    if (!dlq_empty(&suite->cleanup_rawlineQ)) {
                        context->ut_state = UT_STATE_CLEANUP;
                    } else {
                        if ( finish_test_suite(context) ) {
                           *res = ERR_NCX_SKIPPED;
                           line = NULL;
                        }
                    }
                    done = TRUE;
                }
            }
        }

        if (context->ut_state != UT_STATE_CLEANUP) {
            if (line == NULL) {
                if (context->single_suite == FALSE) {
                    line = next_suite_to_run (context, res);
                }
            }
            break;
        } // else fall-through

    case UT_STATE_CLEANUP:
        /* the the next rawline in the cleanup rawline Q */
        if (context->cur_rawline == NULL) {
            context->cur_rawline = (rawline_t *)
                dlq_firstEntry(&suite->cleanup_rawlineQ);
            start_cleanup_banner(context);
        } else {
            context->cur_rawline = (rawline_t *)
                dlq_nextEntry(context->cur_rawline);
        }

        if (context->cur_rawline != NULL) {
            line = get_full_rawline(context, res);
            return line;
        } else {
            final_finish = finish_test_suite(context);
        }

        if (final_finish) {
             *res = ERR_NCX_SKIPPED;
             return NULL;
         } else {
             line = next_suite_to_run (context, res);
         } 

        break;

    case UT_STATE_ERROR:
        if (context->logfile && context->logfile_open) {
            log_close();
            context->logfile_open = FALSE;
        }
        context->ut_input_mode = FALSE;
        break;
    default:
        *res = SET_ERROR(ERR_INTERNAL_VAL);
        line = NULL;
    }

    return line;


}  /* yangcli_ut_getline */


/********************************************************************
* FUNCTION yangcli_ut_handle_return
*
* Handle the return status for invoking a command in autotest mode
* If a local command fails or a remote command request cannot be
* sent, then the test in progress will terminated
*
* INPUTS:
*   server_cb == server context to use
*   res == return status of invoked command
*
*********************************************************************/
void
    yangcli_ut_handle_return (server_cb_t *server_cb,
                                     status_t res)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (!ut_state_busy(context)) {
        return;
    }

    boolean killsuite = FALSE;
    boolean killtest = 
        (res == NO_ERR || res == ERR_NCX_SKIPPED) ? FALSE : TRUE;
    yangcli_ut_suite_t *suite = context->cur_suite;
    yangcli_ut_test_t *test = (context->cur_run_test) ?
        context->cur_run_test->test : NULL;
    yangcli_ut_step_t *step = context->cur_step;

    switch (context->ut_state) {
    case UT_STATE_SETUP:
        if (killtest) {
            killsuite = TRUE;
        }
        break;
    case UT_STATE_CLEANUP:
        killtest = FALSE;
        break;
    case UT_STATE_RUNTEST:
        if (killtest && test) {
            test->test_errors = TRUE;
            suite->suite_errors = TRUE;
        }
        if (step != NULL) {
            /* this is a local command if it is done now.
             * if not then this field will get changed
             * by the reply handler
             */
            step->step_result = UT_RPC_NO;
        } // else internal error?
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (killsuite) {
        /* errors caused the entire test to be ended */
        context->ut_state = UT_STATE_ERROR;
        context->ut_input_mode = FALSE;
        log_info("\nError exit for test-suite '%s' (%s)\n",
                 suite->name, get_error_string(res));
    } else if (killtest) {
        /* skip to the next test */
        context->cur_step = NULL;
        if (context->cur_run_test) {
           run_test_t *run_test = (run_test_t *)
                      dlq_nextEntry(context->cur_run_test);
           context->cur_run_test = 
               run_test_with_no_failed_mustpass (run_test);
        }
    }

}  /* yangcli_ut_handle_return */


/********************************************************************
* FUNCTION yangcli_ut_handle_reply
*
* Handle the <rpc-reply> for a previous command
* in autotest mode
*
* INPUTS:
*   server_cb == server context to use
*   res == return status of invoked command
*
*********************************************************************/
void
    yangcli_ut_handle_reply (server_cb_t *server_cb,
                             response_type_t resp_type,
                             const xmlChar *msg_id,
                             val_value_t *reqmsg,
                             val_value_t *rpydata)
{
    (void)reqmsg;
    yangcli_ut_context_t *context = get_ut_context(server_cb);

    if (context->cur_step) {
        context->cur_step->step_result_wrong = FALSE;
    }

    if (!ut_state_busy(context)) {
        log_debug("\nAutotest: Dropping reply for message '%s' because "
                  "autotest not active\n", msg_id);
        return;
    }

    switch (context->ut_state) {
    case UT_STATE_SETUP:
    case UT_STATE_CLEANUP:
        /* responses during setup/cleanup phases are not tracked */
        break;
    case UT_STATE_RUNTEST:
        if (context->cur_step != NULL) {
            context->cur_step->step_done = TRUE;
            context->cur_step->step_result = resp_type;
            if (context->cur_step->result_type != resp_type) {
                /* did not get the correct response type */
                context->cur_step->step_result_wrong = TRUE;
                report_wrong_response_type(context, resp_type);
            } else {
                switch (context->cur_step->result_type) {
                case UT_RPC_NO:
                case UT_RPC_OK:
                case UT_RPC_ANY:
                    break;
                case UT_RPC_ERROR:
                    // check the fields in the error response
                    validate_step_rpc_error(context, rpydata);
                    break;
                case UT_RPC_DATA:
                    // check the data 
                    validate_reply_data(server_cb, context, rpydata);
                    break;
                default:
                    SET_ERROR(ERR_INTERNAL_VAL);
                }
            }
        }  // else internal error?
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

}  /* yangcli_ut_handle_reply */


/********************************************************************
* FUNCTION yangcli_ut_get_target_session
*
* Get the name of the target session for the input line that
* was just given to get_input_line.  The current context state
* is used to determine which session should be active when
* the command is processed
*
* INPUTS:
*   server_cb == server context to use
*
* RETURNS:
*   pointer to name of the session to use
*   NULL if no session is active for the test-suite in progress
*   could happen if there was an error exit
*********************************************************************/
const xmlChar *
    yangcli_ut_get_target_session (server_cb_t *server_cb)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (!ut_state_busy(context)) {
        return NULL;
    }

    const xmlChar *sesname = context->test_sesname;
    if (context->ut_state == UT_STATE_RUNTEST) {
        if (context->cur_step) {
            if (context->cur_step->session_name) {
                sesname = context->cur_step->session_name;
            } else if (context->cur_step->result_type == UT_RPC_NO) {
                sesname = get_session_name(server_cb->cur_session_cb);
            }
        }
    }

    return sesname;

}  /* yangcli_ut_get_target_session */


/********************************************************************
* FUNCTION yangcli_ut_stop
*
* Stop the current test in progress if any
*
* INPUTS:
*   server_cb == server context to use
*    res == termination status
*    errstring == optional error string
*********************************************************************/
void
    yangcli_ut_stop (server_cb_t *server_cb,
                     status_t res,
                     const xmlChar *errstring)
{
    yangcli_ut_context_t *context = get_ut_context(server_cb);
    if (context->ut_state == UT_STATE_DONE) {
        context->ut_input_mode = FALSE;
        context->ut_status = res;

        log_info("\nFinished test-suite '%s': %s\n",
                 context->cur_suite->name,
                 get_error_string(context->ut_status));
    } else if (ut_state_busy(context)) {
        context->ut_input_mode = FALSE;
        if (res == NO_ERR || res == ERR_NCX_CANCELED) {
            context->ut_status = ERR_NCX_CANCELED;
            context->ut_state = UT_STATE_DONE;
        } else {
            context->ut_state = UT_STATE_ERROR;
            context->ut_status = res;
        }

        log_info("\nStopping test-suite '%s': %s\n",
                 context->cur_suite->name,
                 (errstring) ? errstring : 
                 (const xmlChar *)get_error_string(context->ut_status));
    } else {
        log_error("\nError: no test-suite is active");
    }

}  /* yangcli_ut_stop */



/* END file yangcli_unit_test.c */

/*
 * Copyright (c) 2008 - 2013, Andy Bierman, All Rights Reserved.
 * Copyright (c) 2013, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangcli_record_test.c

   NETCONF YANG-based CLI Tool

   record-test command

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
15-Jan-13    trshue      begun; 

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libssh2.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "libtecla.h"

#include "procdefs.h"
#include "log.h"
#include "mgr.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "obj_help.h"
#include "runstack.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "xml_wr.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_show.h"
#include "yangcli_record_test.h"
#include "yangcli_util.h"
#include "yangcli_unit_test.h"

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define YANGCLI_RT_DEBUG 1
#endif

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
#define get_record_context(server) (&server->tr_context)


/********************************************************************
*                                                                   *
*                     F U N C T I O N S                             *
*                                                                   *
*********************************************************************/

/********************************************************************
* FUNCTION record_reply_data
*
* INPUTS:
*    server  == server control block to use
*    context == unit test context control block to use
*    rpydata == response message
*********************************************************************/
static void
    record_reply_data (server_cb_t *server,
                       yangcli_ut_context_t *context,
                       val_value_t *rpydata)
{
    xmlChar   *fullspec;
    status_t   res = NO_ERR;
    xml_attrs_t  attrs;
    session_cb_t *session = server->cur_session_cb;
    ncx_display_mode_t dispmode = session->display_mode;

    yangcli_ut_suite_t *cur_suite = context->cur_suite;
    yangcli_ut_test_t  *cur_test  = (yangcli_ut_test_t *)
                   dlq_firstEntry(&cur_suite->test_listQ);

    fullspec = fullspec_for_ut_data (
             cur_suite->name, cur_test->test_name,
             context->cur_step->name, TRUE, &res);

    if ((res == NO_ERR) && (fullspec)) { 
        xml_init_attrs(&attrs);
        res = xml_wr_file(fullspec,
                     rpydata, &attrs, XMLMODE,
                     session->use_xmlheader,
                     (dispmode == NCX_DISPLAY_MODE_XML_NONS)
                     ? FALSE : TRUE, 0,
                     session->defindent);
    }

    if (LOGDEBUG3) {
        val_dump_value_full(rpydata,
                            0,   /* startindent */
                            get_defindent(),
                            DUMP_VAL_LOG, /* dumpmode */
                            NCX_DISPLAY_MODE_PLAIN,
                            FALSE,    /* withmeta */
                            FALSE,   /* configonly */
                            TRUE);   /* conf_mode */
    }

    if (fullspec) {
         m__free(fullspec);
    }

}  /* record_reply_data */


/********************************************************************
* FUNCTION check_test_record
*
* Check test recording operation for a specified test .  
*
* INPUTS:
*    context == unit test context control block to use
*    suite_name == test suite name to check 
*    test_name == test name to check
*    op == test record selecion: cancel,start,finish,pause,resume
* RETURNS:
*   TRUE if suite_name and test_name matches the current recording
*        test for the selected operation.
*********************************************************************/
static boolean
    check_test_record (yangcli_ut_context_t *context,
                       const xmlChar *suite_name,
                       const xmlChar *test_name,
                       test_record_op op)
{
    boolean  suite_ok = FALSE;
    boolean  test_ok = FALSE;
    boolean  check_ok = FALSE;
    yangcli_ut_suite_t *cur_record_suite = context->cur_suite;
    yangcli_ut_test_t *cur_record_test = NULL;
    xmlChar  *cur_record_suite_name = NULL;
    xmlChar  *cur_record_test_name = NULL;

    // Check if a record-test exists
    if (cur_record_suite != NULL ) {
        cur_record_suite_name = cur_record_suite->name;
        cur_record_test = (yangcli_ut_test_t *)
            dlq_firstEntry(&cur_record_suite->test_listQ);
        if (cur_record_test != NULL) {
            cur_record_test_name = cur_record_test->test_name;
        }
    }

    if (cur_record_suite_name != NULL) {
       if (!xml_strcmp(suite_name, cur_record_suite_name)) {
           suite_ok = TRUE;
       }
    }

    if (cur_record_test_name != NULL) {
        if (!xml_strcmp(test_name, cur_record_test_name)) {
            test_ok = TRUE;
        }
    }

    if ((suite_ok == TRUE) && (test_ok == TRUE)){
       check_ok = TRUE; 
    } 

    switch (context->ut_state) {
       case UT_STATE_RECORD_IN_PROGRESS:
           if (check_ok == TRUE) {
               if ((op == TEST_RECORD_PAUSE) || 
                   (op == TEST_RECORD_CANCEL) || 
                   (op == TEST_RECORD_FINISH)){
                      return check_ok;
               } 
           }

           log_info("\nRecording is active: suite=%s test=%s",
                                      cur_record_suite_name,
                                      cur_record_test_name);
         
            break;

         case UT_STATE_RECORD_PAUSE:
             if (check_ok == TRUE) {
                 if ((op == TEST_RECORD_RESUME) ||
                     (op == TEST_RECORD_CANCEL) ||
                     (op == TEST_RECORD_FINISH)) {
                     return check_ok;
                 } 
             }

             log_info("\nRecording paused: suite=%s test=%s\n",
                                   cur_record_suite_name,
                                   cur_record_test_name);
             break;

         case UT_STATE_NONE:
             if (op != TEST_RECORD_START) {
                check_ok = FALSE;
                log_info("\nRecording is already active");
             } else {
                check_ok = TRUE;
             }
             break;

         default:
             SET_ERROR(ERR_INTERNAL_VAL);
             break;
     }
   

    return check_ok;

} /* check_test_record */

/********************************************************************
* FUNCTION get_next_step_to_record
*
# get a step name for test recording.
* INPUTS:
*    server == server control block to use
* RETURNS:
*    NULL: no test to record.
*    xmlChar* step name to record the step.
*********************************************************************/
static const char *
    get_next_step_to_record (server_cb_t *server)
{
    #define STEP_BUFFLEN 12 

    yangcli_ut_context_t *context;  
    yangcli_ut_test_t *test = NULL;
    int32  step_num;
    char   num_buff[STEP_BUFFLEN];
    char   *step_name = NULL;

    context = get_record_context(server);
    if (context->cur_suite != NULL ) {
        test = (yangcli_ut_test_t *)
            dlq_firstEntry(&context->cur_suite->test_listQ);
        if (test != NULL) {
            test->step_num_to_record++;
            step_num = test->step_num_to_record;
            sprintf(num_buff, "%d", step_num);
            step_name = (char *) num_buff;
        }
    }

    return step_name;

} /* get_next_step_to_record */


/********************************************************************
* FUNCTION new_record_test
*
* create a unit-test struct
*
* INPUTS:
*    suite_name == test suite name to check
*    test_name == test name to check
*    *res == return status
* OUTPUTS:
*   *res == return status
*
* RETURNS: new test.
*********************************************************************/
static yangcli_ut_test_t* 
                 new_record_test ( 
                 yangcli_ut_suite_t *suite,
                 const xmlChar *test_name,
                 status_t *res)
{
    *res = ERR_INTERNAL_MEM;

    yangcli_ut_test_t *test_ptr =
            find_test(suite, test_name);

    if (!test_ptr) {
        /* Create test  */
        test_ptr = m__getObj(yangcli_ut_test_t);
        if (!test_ptr) {
            return NULL;
        }

        memset(test_ptr, 0x0, sizeof(yangcli_ut_test_t));
        dlq_createSQue(&test_ptr->step_listQ);
        dlq_createSQue(&test_ptr->mustpass_leaflistQ);

        test_ptr->test_name = xml_strdup(test_name);
        if (!test_ptr->test_name) {
            free_test(test_ptr);
            return NULL;
        }

        test_ptr->step_num_to_record = 0;
    }

    *res = NO_ERR;
    return test_ptr;

} /* new_record_test */

/********************************************************************
* FUNCTION new_record_suite
*
* create a unit-test-suite struct
*
* INPUTS:
*    suite_name == test suite name to check
*    test_name == test name to check
*    *res == for return status
*
* OUTPUTS:
*   *res == return status
*
* RETURNS:
*   new suite
*********************************************************************/
static yangcli_ut_suite_t *
    new_record_suite (const xmlChar *suite_name,
               const xmlChar *test_name,
               status_t *res)
{
    *res = ERR_INTERNAL_MEM;

    /* Create suite */
    yangcli_ut_suite_t *suite_ptr = m__getObj(yangcli_ut_suite_t);
    if (!suite_ptr) {
        return NULL;
    } else {
        memset(suite_ptr, 0x0, sizeof(yangcli_ut_suite_t));
        dlq_createSQue(&suite_ptr->setup_rawlineQ);
        dlq_createSQue(&suite_ptr->cleanup_rawlineQ);
        dlq_createSQue(&suite_ptr->run_test_leaflistQ);
        dlq_createSQue(&suite_ptr->test_listQ);
        suite_ptr->name =  xml_strdup(suite_name);
        if (!suite_ptr->name) {
            free_suite(suite_ptr);
            return NULL;
        }
    }

    /* Create run test */
    run_test_t  *runtest_ptr;
    runtest_ptr = m__getObj(run_test_t);
    if (!runtest_ptr) {
        free_suite(suite_ptr);
        return NULL;
    } else {
        memset(runtest_ptr, 0x0, sizeof(run_test_t));
        runtest_ptr->run_test_name = xml_strdup(test_name);
        if (!runtest_ptr->run_test_name) {
            free_suite(suite_ptr);
            m__free(runtest_ptr);
            return NULL;
        } else {
            dlq_enque(runtest_ptr, &suite_ptr->run_test_leaflistQ);
        }
    }

    /* Create test  */
    yangcli_ut_test_t *test_ptr = 
         new_record_test(suite_ptr, test_name, res);
    if (test_ptr) {
         dlq_enque(test_ptr, &suite_ptr->test_listQ);
     } else {
         free_suite(suite_ptr);
         return NULL;
    }
    *res = NO_ERR;
    return suite_ptr;

} /* new_record_suite */


/********************************************************************
 * FUNCTION do_cancel
 * 
 * INPUTS:
 *  server == server control block to use
 *  suite     == suite name
 *  test      == test name
 * RETURNS:
 *  status
 *********************************************************************/
static status_t
    do_cancel (server_cb_t *server,
               const xmlChar *suite_name,
               const xmlChar *test_name)
{
    status_t res = NO_ERR;
    yangcli_ut_context_t *context = NULL;
    boolean check_record = TRUE;

    /* Check recording suite/test */
    context = get_record_context(server);
    check_record = check_test_record (context,
        suite_name, test_name, TEST_RECORD_CANCEL);

    if (check_record == FALSE){
        return ERR_NCX_NOT_FOUND;
    } else {
        free_context_cache(context);
        record_test_init(server);
        log_info("\nRecording canceled: suite=%s test=%s\n",
                                       suite_name, test_name);

    }

    return res;

}  /* do_cancel */

/********************************************************************
 * FUNCTION do_resume
 * INPUTS:
 *  server == server control block to use
 *   suite     == suite name
 *   test      == test name
 * RETURNS:
 *  status
 *********************************************************************/
static status_t
    do_resume (server_cb_t *server,
              const xmlChar *suite_name,
              const xmlChar *test_name)
{
    status_t res = NO_ERR;
    yangcli_ut_context_t *context = NULL;
    boolean check_record = TRUE;

    /* Check current recording suite/test */
    context = get_record_context(server);
    check_record = check_test_record (context, 
          suite_name, test_name, TEST_RECORD_RESUME);

    if (check_record == FALSE){
        return ERR_NCX_NOT_FOUND;
    }

    switch (context->ut_state) {
    case UT_STATE_RECORD_PAUSE:
        log_info("\nResume recording: suite=%s test=%s\n",
                             suite_name, test_name);
        context->ut_state = UT_STATE_RECORD_IN_PROGRESS;
        break;

    default:
        break;
    }

    return res;

}  /* do_resume */


/********************************************************************
 * FUNCTION do_pause 
 * INPUTS:
 *  server == server control block to use
*   suite     == suite name
*   test      == test name
* RETURNS:
*  status
 *********************************************************************/
static status_t
    do_pause (server_cb_t *server,
         const xmlChar *suite_name,
         const xmlChar *test_name)
{
    status_t res = NO_ERR;
    boolean check_record = TRUE;
    yangcli_ut_context_t *context = NULL;

    /* Check current suite/test in recording */
    context = get_record_context(server);
    check_record = check_test_record (context,
         suite_name, test_name, TEST_RECORD_PAUSE);

    if (check_record == FALSE){
        return ERR_NCX_NOT_FOUND;
    }

    switch (context->ut_state) {
       case UT_STATE_RECORD_IN_PROGRESS:
           log_info("\nPause recording: suite=%s test=%s\n",
                                    suite_name, test_name);
           context->ut_state = UT_STATE_RECORD_PAUSE;
           break;

       default:
            break;
     }
   
    return res;

}  /* do_pause */

/********************************************************************
 * FUNCTION do_finish 
 * 
 * INPUTS:
 *   server == server control block to use
 *   suite    == suite name
 *   test     == test name
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_finish (server_cb_t *server,
               const xmlChar *suite_name,
               const xmlChar *test_name)
{

    status_t res = NO_ERR;
    yangcli_ut_context_t *context = NULL;
    boolean check_record = FALSE;

    context = get_record_context(server);
    check_record = check_test_record (context,
         suite_name, test_name, TEST_RECORD_FINISH);

    if (check_record == FALSE) {
        return ERR_NCX_NOT_FOUND;
    } 

    /* save recorded test to a ut test file */

    /* Find suite for this suite_name */
    yangcli_ut_suite_t *suite =
             find_suite (context, NULL, suite_name);

    yangcli_ut_test_t *test_ptr =
            find_test(suite, test_name);

    /* An empty test will be not be saved.*/
    if (test_ptr->step_num_to_record == 0){
        log_info("\nAn empty test will be not be saved.");
        res = do_cancel (server, suite_name, test_name);
        return res;
    }

    /* Add this recorded suite to unit test's suiteQ */
     res = add_to_ut_suiteQ_testQ_runQ (
              server, context, suite_name, test_name);

    /* Free suite if it failed to queue to unit test suiteQ */ 
    if (res != NO_ERR) {
       dlq_deque(&context->suite_listQ);
       free_suite(suite);
    } else {
        /* save recorded test to a ut test file */
        const xmlChar *ut_file = get_test_suite_file();
        res = yangcli_ut_save(server, ut_file);

        if (res == NO_ERR) {
            log_info("\nFinish recording: suite %s test %s\n",
                                      suite_name, test_name);
            log_info("\nSaved recorded test to '%s'\n",
                         ut_file);
        } else {
             log_error("\nSave recorded test to '%s' "
                      "failed (%s)\n", ut_file,
                     get_error_string(res));
        }
    }

    /* cleanup and reset state */
    record_test_init(server);

    return res;

} /* do_finish */


/********************************************************************
 * FUNCTION do_start
 * 
 * INPUTS:
 *    server == server control block to use
 *    suite     == suite name
 *    test      == test name
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_start (server_cb_t *server, 
              const xmlChar *suite_name,
              const xmlChar *test_name)
{
    status_t res = NO_ERR;
    yangcli_ut_context_t *context = NULL;
    yangcli_ut_suite_t *suite;
    boolean check_record = FALSE;

    context = get_record_context(server);

    check_record = check_test_record (context,
        suite_name, test_name, TEST_RECORD_START);

    if (check_record == FALSE){
       return ERR_NCX_RESOURCE_DENIED;
    }

    switch (context->ut_state) {
    case UT_STATE_RECORD_IN_PROGRESS:
    case UT_STATE_RECORD_PAUSE:
        return ERR_NCX_RESOURCE_DENIED;
        break;

    case UT_STATE_NONE:
        /* check if this suite/test is a duplicate unit test name */
        if (check_this_suite_test_exist(server,  
                  suite_name, test_name) == TRUE ){
            log_info("Use a different suite or test name.\n");
            return ERR_NCX_RESOURCE_DENIED;
        }

        suite = new_record_suite (suite_name, test_name, &res); 
        if (suite && res == NO_ERR) {
            dlq_hdr_t tempQ;
            dlq_createSQue(&tempQ);
            dlq_enque(suite, &tempQ);
            dlq_block_enque(&tempQ, &context->suite_listQ);
            context->ut_state = UT_STATE_RECORD_IN_PROGRESS;
            log_info("\nStart recording:  suite=%s test=%s\n",
                                  suite_name, test_name);
            context->cur_suite = suite;
            context->ut_state = UT_STATE_RECORD_IN_PROGRESS;
       }
       break;

    default:
        free_context_cache(context);
        record_test_init(server);
        SET_ERROR(ERR_INTERNAL_VAL);
        break;
    }

    return res;

} /* do_start */

/********************************************************************
* FUNCTION check_rpcname
*
*  Check if the get_foo function should fill in this parameter
*  or not
*
* INPUTS:
*   parm == object template to check
*
* RETURNS:
*    TRUE if value should be filled in
*    FALSE if it should be skipped
*********************************************************************/
static boolean
    check_rpcname (const xmlChar *rpcname) {

    // log_info("\ncheck_rpcname : %s", rpcname);

    if ((!xml_strcmp(rpcname, (const xmlChar *) "source"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "select"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "varref"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "urltarget"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "locals"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "globals"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "oids"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "files"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "scripts"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "commands"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "objects"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "vars"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "modules"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "version"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "system"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "cli"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "local"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "global"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "var"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "module"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "finish"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "resume"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "cancel"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "pause"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "start"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "expr"))||
        (!xml_strcmp(rpcname, (const xmlChar *) "target"))) {
       return FALSE;
    } else {
       return TRUE;
    }

}  /* check_rpcname */


/**************    E X T E R N A L   F U N C T I O N S **********/

/*********************************************************************
*
* record_new_step_valset
*
* INPUTS:
*   server_cb_t *server
*   rpc == RPC method that is being called
*   valset == commad line value to get  
* RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
status_t
    record_new_step_valset (server_cb_t *server,
                          obj_template_t *rpc,
                          val_value_t *valset)
{
    if (is_test_recording_on(server) == FALSE) {
       return NO_ERR;
    }

    const xmlChar *rpcname = obj_get_name(rpc);
    if (!xml_strcmp(rpcname, YANGCLI_RECORD_TEST)) {
       yangcli_ut_context_t *context = get_record_context(server);
       context->cur_step_record_done = TRUE;
       return NO_ERR;
    }

    if (check_rpcname(rpcname) != TRUE) {
       return NO_ERR;
    }

    if (valset == NULL) {
       return ERR_NCX_OPERATION_FAILED;
    }

#ifdef YANGCLI_RT_DEBUG
    if (LOGDEBUG2) {
        val_dump_value(valset, 0);
    }
#endif

    /* first get the length of the command */
    uint32 command_len = xml_strlen(rpcname);
    status_t res = NO_ERR;
    val_value_t *childval = NULL;

    val_value_t *val = val_get_first_child(valset);
    for (; res == NO_ERR && val != NULL;
             val = val_get_next_child(val)) {

        /* skip values set by default, just save the ones set by user */
        if (val_set_by_default(val)) {
            continue;
        }

        command_len++;  /*  account for space */
        command_len += xml_strlen(val->name);

        if (val->btyp == NCX_BT_EMPTY) {
            ;
        } else if (typ_is_simple(val->btyp)) {
            command_len++;   /* account for '=' */

            if (val->obj && obj_is_password(val->obj)) {
                command_len += xml_strlen(VAL_STR(val));
                if (val_need_quotes(VAL_STR(val))) {
                    command_len += 2;
                }
            } else {
                uint32 val_len = 0;
                res = val_sprintf_simval_nc(NULL, val, &val_len);
                if (res == NO_ERR) {
                    command_len += val_len;
                }
                if (typ_is_string(val->btyp) &&
                    val_need_quotes(VAL_STR(val))) {
                    command_len += 2;
                }
            }
        } else if (val->btyp == NCX_BT_CONTAINER) {
            if (!xml_strcmp(val->name, NCX_EL_SOURCE) ||
                !xml_strcmp(val->name,NCX_EL_TARGET)) {
                childval = val_get_first_child(val);

                if ((childval != NULL) && (childval->btyp == NCX_BT_EMPTY)) {
                    command_len++;   /* account for '=' */
                    command_len += xml_strlen(childval->name);
                }
            }
        } else {
            log_error("\nError: cannot record complex value '%s'\n", val->name);
            res = ERR_NCX_INVALID_VALUE;
        }
    }

    if (res != NO_ERR) {
        return res;
    }

    xmlChar *buffer = m__getMem(command_len+1);
    if (buffer == NULL) {
        return ERR_INTERNAL_MEM;
    }

    xmlChar *p = buffer;
    p += xml_strcpy(p, rpcname);

    val = val_get_first_child(valset);
    for (; res == NO_ERR && val != NULL;
         val = val_get_next_child(val)) {

        /* skip values set by default, just save the ones set by user */
        if (val_set_by_default(val)) {
            continue;
        }

#ifdef YANGCLI_RT_DEBUG
        if (LOGDEBUG2) {
            val_dump_value(val, 0);
        }
#endif

        *p++ = ' ';
        p += xml_strcpy(p, val->name);

        if (val->btyp == NCX_BT_EMPTY) {
            break;
        } else if (typ_is_simple(val->btyp)) {
            *p++ = '=';
            if (val->obj && obj_is_password(val->obj)) {
                boolean needq = val_need_quotes(VAL_STR(val));
                if (needq) {
                    *p++ = '"';
                }
                p += xml_strcpy(p, VAL_STR(val));
                if (needq) {
                    *p++ = '"';
                    *p = 0;
                }
            } else {
                uint32 val_len = 0;
                boolean needvalq = FALSE;
                if (typ_is_string(val->btyp)) {
                    needvalq = val_need_quotes(VAL_STR(val));
                }
                if (needvalq) {
                    *p++ = '"';
                }
                res = val_sprintf_simval_nc(p, val, &val_len);
                if (res == NO_ERR) {
                    p += val_len;
                    if (needvalq) {
                        *p++ = '"';
                        *p = 0;
                    }
                }
            }
        } else if (val->btyp == NCX_BT_CONTAINER) {
            if (!xml_strcmp(val->name, NCX_EL_SOURCE) || 
                !xml_strcmp(val->name,NCX_EL_TARGET)) {
                childval = val_get_first_child(val);
                if ((childval!=NULL) && (childval->btyp == NCX_BT_EMPTY)) {
                    *p++ = '=';
                    p += xml_strcpy(p, childval->name);
                }
            }
        } else {
            res = ERR_NCX_OPERATION_FAILED;
        }
    }

    if (res == NO_ERR) {
        new_record_step(server, rpc, buffer);
    }

    m__free(buffer);
    return res;
    
} /* record_new_step_valset */

/*********************************************************************
 * record_step_reply
 *
 * INPUTS:
 *   server_cb_t *server
*    response_type_t resp_type
*    const xmlChar *msg_id
*    val_value_t *reqmsg
*    val_value_t *rpydata
*
* RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
extern status_t
    record_step_reply (server_cb_t *server,
                       response_type_t resp_type,
                       const xmlChar *msg_id,
                       val_value_t *reqmsg,
                       val_value_t *rpydata)
{
    (void)msg_id;
    (void)reqmsg;
    status_t  res = NO_ERR;

    yangcli_ut_context_t *context = NULL;
    context = get_record_context(server);

    if (is_test_recording_on (server) == FALSE) {
       return NO_ERR;
    }

    if (context->cur_step == NULL) {
        return NO_ERR;
    }

    if (context->cur_step_record_done == TRUE ){
       return NO_ERR;
    }

    yangcli_ut_step_t *step = context->cur_step;
    step->result_type =  resp_type;

    switch (resp_type) {
        case UT_RPC_NO:
        case UT_RPC_OK:
        case UT_RPC_ANY:
            break;
        case UT_RPC_ERROR:
            // check the fields in the error response
            if (rpydata != NULL) {
                val_value_t *rpcerr = val_find_child(
                   rpydata, NC_MODULE, NCX_EL_RPC_ERROR);
                if (rpcerr != NULL) {
                    val_value_t *errtag = val_find_child(rpcerr,
                                   NC_MODULE, NCX_EL_ERROR_TAG);
                    step->result_error_tag = xml_strdup(VAL_STR(errtag));
                } else {
                    log_error("\nError: no <rpc-error> element found\n");
                }
             }
            break;
        case UT_RPC_DATA:
            record_reply_data (server, context, rpydata);
            break;
        default:
            res = ERR_NCX_INVALID_VALUE;
      }

    context->cur_step_record_done = TRUE;
    return res;

} /* record_step_reply */

/*********************************************************************
 * check_supported_cmd
 *
 * Record a new step for the test.
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   rpc == template for the local RPC
 *   line == input command line from user
 *   len == line length
 *
 * RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
static status_t
   check_supported_cmd  (const xmlChar *rpcname)
{
    if (!xml_strcmp(rpcname, YANGCLI_RECORD_TEST)) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    if (!xml_strcmp(rpcname, YANGCLI_TEST_SUITE)) {
        if (LOGDEBUG2) {
            log_debug2("\nNo recording for this CLI command: '%s'\n",
            rpcname);
        }

        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    return NO_ERR;

} /* check_supported_cmd */

/*********************************************************************
 * new_record_step 
 *
 * Record a new step for the test.
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   rpc == template for the local RPC
 *   line == input command line from user
 *   len == line length
 *
 * RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
extern status_t
    new_record_step (server_cb_t *server,
                     obj_template_t *rpc, 
                     const xmlChar *line)
{
    yangcli_ut_suite_t *cur_record_suite = NULL;
    yangcli_ut_test_t  *cur_record_test = NULL;
    yangcli_ut_context_t *context = NULL;

    if (is_test_recording_on (server) == FALSE) {
       return NO_ERR;
    }

    context = get_record_context(server);
    if (context->cur_step_record_done == TRUE ){
       return NO_ERR;
    }

    if (rpc){
        const xmlChar *rpcname = obj_get_name(rpc);
        if (check_supported_cmd(rpcname) != NO_ERR){
            context->cur_step_record_done = TRUE;
            return NO_ERR;
        }
    } else {
        context->cur_step_record_done = TRUE;
        return NO_ERR;
    }

    cur_record_suite = context->cur_suite;
    if (cur_record_suite != NULL) {
        cur_record_test = (yangcli_ut_test_t *)
        dlq_firstEntry(&cur_record_suite->test_listQ);

        if (cur_record_test == NULL) {
            return ERR_NCX_DEF_NOT_FOUND;
        }
    }

    /* Start to cache the test step. */
    yangcli_ut_step_t *step = m__getObj(yangcli_ut_step_t);
    if (!step) {
        return  ERR_INTERNAL_MEM;
    }

    memset(step, 0x0, sizeof(yangcli_ut_step_t));
    dlq_createSQue(&step->result_error_infoQ);

    const xmlChar *name_step = 
        (const xmlChar *)get_next_step_to_record (server);
    step->name = xml_strdup(name_step);
    if (!step->name) {
        free_step(step);
        return ERR_INTERNAL_MEM;
    }

    session_cb_t *session = server->cur_session_cb;
    step->session_name =  xml_strdup(get_session_name(session));
    if (!step->session_name) {
        free_step(step);
        return ERR_INTERNAL_MEM;
    }   

    step->command =  xml_strdup((const xmlChar *)line);
    if (!step->command) {
        free_step(step);
        return ERR_INTERNAL_MEM;
    }   

    context->cur_step = step;
    /* step->result_error_apptag = TODO;
    if (!step->result_error_apptag) {
        free_step(step);
        return ERR_INTERNAL_MEM;
    }   
    */
  
    dlq_enque(step, &cur_record_test->step_listQ);

    context->cur_step_record_done = TRUE;

    return NO_ERR;

} /* new_record_step */

/********************************************************************
* FUNCTION is_test_recording_on
*
* INPUTS:
*   server == server_control_block
*
* RETURNS:
*   TRUE if recording is on
*   FALSE if recording is off
*********************************************************************/
extern boolean
    is_test_recording_on (server_cb_t *server)
{
    boolean record_on = FALSE; 

    yangcli_ut_context_t *context = get_record_context(server);
    if (context->ut_state == UT_STATE_RECORD_IN_PROGRESS){
        record_on = TRUE;
    } 

    return record_on;

}/* is_test_recording_on */

/********************************************************************
 * FUNCTION do_record_test 
 * 
 * INPUTS:
 *    server == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 * RETURNS:
 *   status
 *********************************************************************/
extern status_t
    do_record_test (server_cb_t *server,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len)
{
    status_t   res = NO_ERR;
    yangcli_ut_suite_t *cur_suite = NULL;
    yangcli_ut_test_t *cur_test = NULL;

    yangcli_ut_context_t *context = 
            get_record_context(server);
    cur_suite = context->cur_suite;
    if (cur_suite != NULL) {
       cur_test  = (yangcli_ut_test_t *)
            dlq_firstEntry(&cur_suite->test_listQ);
    }

    /* Check if there exists an recording test. */
    xmlChar *val_suite_name = NULL;
    xmlChar *val_test_name = NULL;
    xmlChar *cur_suite_name = NULL;
    xmlChar *cur_test_name = NULL;

    /* Checking for any active recording case */
    if (context->ut_state != UT_STATE_NONE) {
       if ((cur_suite == NULL) || (cur_test == NULL)) {
           SET_ERROR(ERR_INTERNAL_PTR); 
           return ERR_NCX_OPERATION_FAILED;
       } else {
           cur_suite_name = xml_strdup(cur_suite->name);
           if (cur_suite_name == NULL) {
               return ERR_INTERNAL_MEM;
           } 
           cur_test_name = xml_strdup(cur_test->test_name);
           if (cur_test_name == NULL) {
               if (cur_suite_name != NULL) {
                   m__free(cur_suite_name);
               }
               return ERR_INTERNAL_MEM;
           }
       }
    } 

   /* 
    * Handle a new recording or
    * paulse, resume, cancel, and finish an existing one 
    */

    val_value_t *val_start = NULL;
    val_value_t *val_cancel = NULL;
    val_value_t* val_finish = NULL;
    val_value_t *val_pause =  NULL;
    val_value_t *val_resume = NULL;
    val_value_t *val_suite = NULL; 
    val_value_t *val_test =  NULL;

    val_value_t *valset = get_valset(server, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
        val_start = val_find_child(valset,
              YANGCLI_MOD, YANGCLI_RECORD_START);
        val_cancel = val_find_child(valset,
                       YANGCLI_MOD, YANGCLI_RECORD_CANCEL);
        val_finish = val_find_child(valset,
                       YANGCLI_MOD, YANGCLI_RECORD_FINISH);
        val_pause = val_find_child(valset,
                       YANGCLI_MOD, YANGCLI_RECORD_PAUSE);
        val_resume = val_find_child(valset,
                       YANGCLI_MOD, YANGCLI_RECORD_RESUME);
        val_suite = val_find_child(valset,
                YANGCLI_MOD, YANGCLI_RECORD_SUITENAME);
        if (val_suite) {
            val_suite_name = VAL_STR(val_suite);
        }
        val_test = val_find_child(valset,
                YANGCLI_MOD, YANGCLI_RECORD_TESTNAME);
        if (val_test) {
           val_test_name = VAL_STR(val_test);
        }
    } 

     if (context->ut_state == UT_STATE_NONE) {
        if (val_start) {
            res = do_start(server, val_suite_name, val_test_name);
        } 
     } else {
        if (val_start) {
            log_error("\nError: One recording is already active: suite=%s test=%s\n",
                     cur_suite_name, cur_test_name);
        } else if (val_cancel) { 
            res = do_cancel(server, cur_suite_name, cur_test_name);
        } else if (val_finish) {
            res = do_finish(server, cur_suite_name, cur_test_name);
        } else if (val_pause) {
            res = do_pause(server, cur_suite_name, cur_test_name);
        } else if (val_resume) {
            res = do_resume(server, cur_suite_name, cur_test_name);
        } else {
            res = ERR_NCX_NOT_FOUND;
        }
    }

    if (valset) {
        val_free_value(valset);
    }

    if (cur_suite_name != NULL) { 
       m__free(cur_suite_name);
    }
   
    if (cur_test_name != NULL) {
        m__free(cur_test_name);
    }

    return res;

}  /* do_record_test */

/********************************************************************
* FUNCTION set_cur_record_step_done
*
* INPUTS:
*    server == server control block to use
*    boolean == TRUE/FALSE
* RETURNS:
*   none
*********************************************************************/
extern void
    set_cur_record_step_done (server_cb_t *server, 
                              boolean done_status)
{
    yangcli_ut_context_t *context = get_record_context(server);
    context->cur_step_record_done = done_status;
    
} /* set_cur_record_step_done */


/********************************************************************
* FUNCTION record_test_init
*
* INPUTS:
*    server == server control block to use
* RETURNS:
*   none
*********************************************************************/
extern void
    record_test_init (server_cb_t *server)
{
    yangcli_ut_context_t *context = get_record_context(server);
    memset(context, 0x0, sizeof(yangcli_ut_context_t));
    dlq_createSQue(&context->suite_listQ);
    context->ut_state = UT_STATE_NONE;
    context->cur_step_record_done = TRUE;

    //context->step_record_cmdline = NULL; 

} /* record_test_init */

/********************************************************************
* FUNCTION record_test_cleanup
*
* Cleanup the test record module
*
* INPUTS:
*   server_cb == server context to use
* RETURNS:
*   none
*********************************************************************/
void
    record_test_cleanup (server_cb_t *server_cb)
{
    yangcli_ut_context_t *context = get_record_context(server_cb);
    free_context_cache(context);

} /* yangcli_ut_cleanup */

/* END yangcli_record_test.c */

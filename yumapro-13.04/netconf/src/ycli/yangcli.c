/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangcli.c

   NETCONF YANG-based CLI Tool

   See ./README for details

For yp-shell mode note the following differences:

   - autotest is set to FALSE and cannot be changed

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
01-jun-08    abb      begun; started from ncxcli.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
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
#include <time.h>
#include <sys/time.h>

/* #define MEMORY_DEBUG 1 */

#ifdef MEMORY_DEBUG
#include <mcheck.h>
#endif

#include "libtecla.h"

#include "procdefs.h"
#include "cli.h"
#include "conf.h"
#include "help.h"
#include "json_wr.h"
#include "log.h"
#include "ncxmod.h"
#include "mgr.h"
#include "mgr_hello.h"
#include "mgr_io.h"
#include "mgr_not.h"
#include "mgr_rpc.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncx_list.h"
#include "ncx_num.h"
#include "ncx_str.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "op.h"
#include "rpc.h"
#include "runstack.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xml_util.h"
#include "xml_wr.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_alias.h"
#include "yangcli_autoconfig.h"
#include "yangcli_autoload.h"
#include "yangcli_autolock.h"
#include "yangcli_autonotif.h"
#include "yangcli_autotest.h"
#include "yangcli_cmd.h"
#include "yangcli_config.h"
#include "yangcli_notif.h"
#include "yangcli_save.h"
#include "yangcli_server.h"
#include "yangcli_sessions.h"
#include "yangcli_tab.h"
#include "yangcli_unit_test.h"
#include "yangcli_record_test.h"
#include "yangcli_uservars.h"
#include "yangcli_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
#define YANGCLI_DEBUG   1
//#define LOGGER_TEST 1
//#define DEBUG_MODLIBQ 1
//#define TIMEOUT_TEST 1
#endif


/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*             F O R W A R D   D E C L A R A T I O N S               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

/*****************  I N T E R N A L   V A R S  ****************/

/* TBD: use multiple server control blocks, stored in this Q */
static dlq_hdr_t      server_cbQ;

/* hack for now instead of lookup functions to get correct
 * server processing context; later search by session ID
 */
static server_cb_t    *cur_server_cb;

/* yangcli.yang file used for quicker lookups */
static ncx_module_t  *yangcli_mod;

/* netconf.yang file used for quicker lookups */
static ncx_module_t  *netconf_mod;

/* yumaworks-test.yang file used for quicker lookups */
static ncx_module_t  *test_suite_mod;

/* need to save CLI parameters: other vars are back-pointers */
static val_value_t   *mgr_cli_valset;

/* true if running a script from the invocation and exiting */
static boolean         batchmode;

/* true if printing program help and exiting */
static boolean         helpmode;
static help_mode_t     helpsubmode;

/* true if printing program version and exiting */
static boolean         versionmode;

/* name of script passed at invocation to auto-run */
static xmlChar        *runscript;

/* TRUE if runscript has been completed */
static boolean         runscriptdone;

/* command string passed at invocation to auto-run */
static xmlChar        *runcommand;

/* TRUE if runscript has been completed */
static boolean         runcommanddone;

/* Q of modtrs that have been loaded with 'mgrload' */
static dlq_hdr_t       mgrloadQ;

/* temporary file control block for the program instance */
static ncxmod_temp_progcb_t  *temp_progcb;

/* Q of ncxmod_search_result_t structs representing all modules
 * and submodules found in the module library path at boot-time
 */
static dlq_hdr_t      modlibQ;

/* Q of alias_cb_t structs representing all command aliases */
static dlq_hdr_t      aliasQ;

/* flag to indicate init never completed OK; used during cleanup */
static boolean       init_done;

/*****************  C O N F I G   V A R S  ****************/

/* TRUE if OK to load aliases automatically
 * FALSE if --autoaliases=false set by user
 * when yangcli starts, this var controls
 * whether the ~/.yumapro/.yangcli_pro_aliases file will be loaded
 * into this application automatically
 */
static boolean         autoaliases;

/* set if the --aliases-file parameter is present */
static const xmlChar *aliases_file;

/* TRUE if OK to load modules automatically
 * FALSE if --autoload=false set by user
 * when server connection is made, and module discovery is done
 * then this var controls whether the matching modules
 * will be loaded into this application automatically
 */
static boolean         autoload;

/* TRUE if OK to check for partial command names and parameter
 * names by the user.  First match (TBD: longest match!!)
 * will be used if no exact match found
 * FALSE if only exact match should be used
 */
static boolean         autocomp;

/* TRUE if OK to load the running config when a session starts
 * and update it as needed;
 * FALSE: auto running config retrieval mode is disabled
 */
static boolean         autoconfig;

/* controls automatic command line history buffer load/save */
static boolean autohistory;

/* controls automatic notification monitoring */
static boolean         autonotif;

/* TRUE if OK to load/save configured sessions
 * FALSE if only manual load/save
 */
static boolean         autosessions;

/* TRUE if OK to load/save configured test-suites
 * FALSE if only manual load/save
 */
static boolean         autotest;

/* TRUE if OK to load user vars automatically
 * FALSE if --autouservars=false set by user
 * when yangcli starts, this var controls
 * whether the ~/.yumapro/.yangcli_pro_uservars file will be loaded
 * into this application automatically
 */
static boolean         autouservars;

/* set if the --uservars-file=filespec parameter is set */
static const xmlChar  *uservars_file;

/* set if the --testsuite-file=filespec parameter is set */
static const xmlChar  *testsuite_file;
static boolean testsuite_file_default;
static boolean testsuite_file_opened;

/* TBD: set if the --sessions=filespec parameter is set */
static const xmlChar  *sessions_file;

/* NCX_BAD_DATA_IGNORE to silently accept invalid input values
 * NCX_BAD_DATA_WARN to warn and accept invalid input values
 * NCX_BAD_DATA_CHECK to prompt user to keep or re-enter value
 * NCX_BAD_DATA_ERROR to prompt user to re-enter value
 */
static ncx_bad_data_t  baddata;

/* global connect param set, copied to server connect parmsets */
static val_value_t   *connect_valset;

/* name of external CLI config file used on invocation */
static xmlChar        *confname;

/* the module to check first when no prefix is given and there
 * is no parent node to check;
 * usually set to module 'netconf'
 */
static xmlChar        *default_module;  

/* 0 for no timeout; N for N seconds message timeout */
static uint32          default_timeout;

/* default value for val_dump_value display mode */
static ncx_display_mode_t   display_mode;

/* FALSE to send PDUs in manager-specified order
 * TRUE to always send in correct canonical order
 */
static boolean         fixorder;

/* FALSE to skip optional nodes in do_fill
 * TRUE to check optional nodes in do_fill
 */
static boolean         optional;

/* default NETCONF test-option value */
static op_testop_t     testoption;

/* default NETCONF error-option value */
static op_errop_t      erroption;

/* default NETCONF default-operation value */
static op_defop_t      defop;

/* default NETCONF with-defaults value */
static ncx_withdefaults_t  withdefaults;

/* default prompt-type value */
static help_mode_t prompt_type;

/* default indent amount */
static int8            defindent;

/* default message indent amount */
static int8       msg_defindent;

/* default echo-notifs */
static boolean echo_notifs;

/* default echo-notif-log-level */
static log_debug_t echo_notif_loglevel;

/* default echo-replies */
static boolean echo_replies;

/* default script-input */
static boolean script_input;

/* default time-rpcs */
static boolean time_rpcs;

/* default time-rpcs-stats */
static boolean time_rpcs_stats;

/* default time-rpcs-stats-file */
static const xmlChar *time_rpcs_stats_file;

/* default match-names */
static ncx_name_match_t match_names;

/* default alt-names */
static boolean alt_names;

/* default force-target */
static const xmlChar *force_target;

/* default use-xmlheader */
static boolean use_xmlheader;

/* default config-edit-mode */
static config_edit_mode_t config_edit_mode;

/* program mode: client or server */
static program_mode_t program_mode;

/* modification time of ALIASES_FILE */
static time_t  yangcli_def_aliases_file_mtime;

/* modification time of USERVARS_FILE */
static time_t  yangcli_def_uservars_file_mtime;

/* modification time of HISTORY_FILE */
static time_t  yangcli_def_history_file_mtime;

/* modification time of SESSIONS_FILE */
static time_t  yangcli_def_session_file_mtime;

/* modification time of TESTSUITE_FILE */
static time_t  yangcli_def_testsuite_file_mtime;

/* modification time of TIME_RPCS_STATS_FILE */
static time_t  yangcli_def_rpcs_stats_file_mtime;

static boolean aliases_change;
static boolean uservars_change;
static boolean history_change;
static boolean session_change;
static boolean testsuite_change;
static boolean rpcs_stats_change;

/* connect_all_in_progress */
static boolean connect_all_in_progress;

/* command counter to cause the EVAL version to exit after N commands */
#ifdef EVAL_VERSION
static uint32 command_count;
#endif

#ifdef DEBUG_MODLIBQ
/********************************************************************
* FUNCTION dump_modlibQ
* 
*  Dump the contents of the module search Q
*
* INPUTS:
*    searchQ == Q to use
*********************************************************************/
static void
    dump_modlibQ (dlq_hdr_t *searchQ)
{
    log_debug("\nModule search Q:");
    ncxmod_search_result_t *sr;
    for (sr = (ncxmod_search_result_t *)dlq_firstEntry(searchQ);
         sr != NULL;
         sr = (ncxmod_search_result_t *)dlq_nextEntry(sr)) {
        log_debug("\nModule: %s, rev: %s, ns: %s", 
                  sr->module ? sr->module : EMPTY_STRING,
                  sr->revision ? sr->module : EMPTY_STRING,
                  sr->namespacestr ? sr->namespacestr : EMPTY_STRING);
    }

}  /* dump_modlibQ */
#endif


/********************************************************************
* FUNCTION get_line_timeout
* 
*  Callback function for libtecla when the inactivity
*  timeout occurs.  
*
* This function checks to see:
*   1) if the session is still active
*   2) if any notifications are pending
*
* INPUTS:
*    gl == line in progress
*    data == server control block passed as cookie
* 
* OUTPUTS:
*    prints/logs notifications pending
*    may generate log output and/or change session state
*
* RETURNS:
*    if session state changed (session lost)
*    then GLTO_ABORT will be returned
*
*    if any text written to STDOUT, then GLTO_REFRESH 
*    will be returned
*
*    if nothing done, then GLTO_CONTINUE will be returned
*********************************************************************/
static GlAfterTimeout
    get_line_timeout (GetLine *gl, 
                      void *data)
{
    (void)gl;

    server_cb_t *server_cb = (server_cb_t *) data;
    session_cb_t *session_cb = (session_cb_t *)server_cb->cur_session_cb;

#ifdef TIMEOUT_TEST
    boolean any_debug_out = FALSE;
    if (LOGDEBUG3) {
        gl_normal_io(server_cb->cli_gl);
    }
#endif

    if (!server_connected(server_cb)) {
#ifdef TIMEOUT_TEST
        if (LOGDEBUG4) {
            log_debug4("\ngl-timeout continue exit: server not connected");
            any_debug_out = TRUE;
        }
#endif
        return GLTO_CONTINUE;
    }        

    ses_cb_t *scb = NULL;
    if (session_cb) {
        session_cb->returncode = MGR_IO_RC_NONE;

        if (session_cb->mysid) {
            scb = mgr_ses_get_scb(session_cb->mysid);
            if (scb == NULL) {
                /* session was dropped */
#ifdef TIMEOUT_TEST
                if (LOGDEBUG3) {
                    log_debug3("\ngl-timeout abort exit: drop null scb (%u)\n",
                               session_cb->mysid);
                    any_debug_out = TRUE;
            }
#endif
                session_cb->returncode = MGR_IO_RC_DROPPED;
                session_cb->state = MGR_IO_ST_IDLE;
                server_drop_session(server_cb, session_cb);
                return GLTO_ABORT;
            }
        }
    }

    ses_id_t sid = (scb) ? scb->sid : 0;
    boolean wantdata = FALSE;
    boolean anystdout = FALSE;
    boolean retval = mgr_io_process_timeout(sid, &wantdata, &anystdout);
    if (session_cb) {
        if (retval) {
#ifdef TIMEOUT_TEST
            if (LOGDEBUG4) {
                log_debug4("\ngl-timeout process retval\n");
                any_debug_out = TRUE;
            }
#endif
            /* this session is probably still alive */
            if (wantdata) {
                session_cb->returncode = MGR_IO_RC_WANTDATA;
            } else {
                session_cb->returncode = MGR_IO_RC_PROCESSED;
            }
        } else {
            /* this session was dropped just now */
#ifdef TIMEOUT_TEST
            if (LOGDEBUG3) {
                log_debug3("\ngl-timeout process drop\n");
                any_debug_out = TRUE;
            }
#endif
            session_cb->returncode = MGR_IO_RC_DROPPED_NOW;
            session_cb->state = MGR_IO_ST_IDLE;
            server_drop_session(server_cb, session_cb);
            return GLTO_ABORT;
        }
    }

#ifdef TIMEOUT_TEST
    if (LOGDEBUG3 && any_debug_out) {
        return GLTO_REFRESH;
    }
#endif

    return (anystdout) ? GLTO_REFRESH : GLTO_CONTINUE;

} /* get_line_timeout */


/********************************************************************
* FUNCTION do_startup_screen
* 
*  Print the startup messages to the log and stdout output
* 
*********************************************************************/
static void
    do_startup_screen (program_mode_t prog_mode)
{
    xmlChar versionbuffer[NCX_VERSION_BUFFSIZE];
    boolean imode = interactive_mode();
    logfn_t logfn = (imode) ? log_stdout : log_write;

    status_t res = ncx_get_version(versionbuffer, NCX_VERSION_BUFFSIZE);
    if (res == NO_ERR) {
        const xmlChar *progname = PROGNAME;
        if (prog_mode == PROG_MODE_SERVER) {
            progname = YP_PROGNAME;
        }
        (*logfn)("\n  %s version %s", progname, versionbuffer);
        if (prog_mode == PROG_MODE_SERVER) {
            if (LOGDEBUG) {
                (*logfn)("\n  ");
                mgr_print_libssh2_version(!imode);
            }
        } else {
            if (LOGINFO) {
                (*logfn)("\n  ");
                mgr_print_libssh2_version(!imode);
            }
        }
    } else {
        SET_ERROR(res);
    }

    (*logfn)("\n\n  ");
    (*logfn)(COPYRIGHT_STRING);
    (*logfn)("  ");
    (*logfn)(COPYRIGHT_STRING2);

    if (!imode) {
        return;
    }

    if (prog_mode == PROG_MODE_SERVER) {
        return;
    }

    (*logfn)("\n  Type 'help' or 'help <command-name>' to get started");
    (*logfn)("\n  Use the <tab> key for command and value completion");
    (*logfn)("\n  Use the <enter> key to accept the default value ");
    (*logfn)("in brackets");

    (*logfn)("\n\n  These escape sequences are available ");
    (*logfn)("when filling parameter values:");
    (*logfn)("\n\n\t?\thelp");
    (*logfn)("\n\t??\tfull help");
    (*logfn)("\n\t?s\tskip current parameter");
    (*logfn)("\n\t?c\tcancel current command");

    (*logfn)("\n\n  These assignment statements are available ");
    (*logfn)("when entering commands:");
    (*logfn)("\n\n\t$<varname> = <expr>\tLocal user variable assignment");
    (*logfn)("\n\t$$<varname> = <expr>\tGlobal user variable assignment");
    (*logfn)("\n\t@<filespec> = <expr>\tFile assignment\n");


}  /* do_startup_screen */


/********************************************************************
* FUNCTION free_autotest_cb
* 
*  Clean and free an autotest control block
* 
* INPUTS:
*    autotest_cb == control block to free
*
*********************************************************************/
static void
    free_autotest_cb (autotest_cb_t *autotest_cb)
{
    if (autotest_cb == NULL) {
        return;
    }

    val_free_value(autotest_cb->editroot);
    m__free(autotest_cb);

}  /* free_autotest_cb */


/********************************************************************
* FUNCTION free_session_cb
* 
*  Clean and free an session control block
* 
* INPUTS:
*    session_cb == control block to free
*                MUST BE REMOVED FROM ANY Q FIRST
*
*********************************************************************/
static void
    free_session_cb (session_cb_t *session_cb)
{
    if (session_cb == NULL) {
        return;
    }

    if (session_cb->session_cfg_new) {
        free_session_cfg(session_cb->session_cfg);
    }

    m__free(session_cb->time_rpcs_stats_file_rpc);

    if (session_cb->time_rpcs_fp) {
        fclose(session_cb->time_rpcs_fp);
    }

    m__free(session_cb->autoconfig_saveline);

    ncx_clean_save_deviationsQ(&session_cb->deviationQ);

    clean_session_cb_conn(session_cb);

    var_clean_varQ(&session_cb->varbindQ);

    m__free(session_cb->config_path);

    val_free_value(session_cb->config_tree);

    val_free_value(session_cb->config_etree);

    while (!dlq_empty(&session_cb->config_editQ)) {
        config_edit_t *edit = (config_edit_t *)
            dlq_deque(&session_cb->config_editQ);
        free_config_edit(edit);
    }

    free_autotest_cb(session_cb->autotest_cb);

    m__free(session_cb);

}  /* free_session_cb */


/********************************************************************
* FUNCTION free_server_cb
* 
*  Clean and free an server control block
* 
* INPUTS:
*    server_cb == control block to free
*                MUST BE REMOVED FROM ANY Q FIRST
*
********************************************************************/
static void
    free_server_cb (server_cb_t *server_cb)
{
    if (server_cb == NULL) {
        return;
    }

    /* save the history buffer if needed */
    if (server_cb->cli_gl != NULL && server_cb->history_auto) {
        int retval = 
            gl_save_history(server_cb->cli_gl,
                            (const char *)server_cb->history_filename,
                            "#",   /* comment prefix */
                            -1);    /* save all entries */
        if (retval) {
            log_error("\nError: could not save command line "
                      "history file '%s'\n", server_cb->history_filename);
        } else {
           status_t  res = NO_ERR;
           xmlChar *fullspec = ncx_get_source(
                      server_cb->history_filename, &res);
           if (res == NO_ERR){
               res = update_def_yangcli_file_mtime (HISTORY_FILE, fullspec);
           }
           if (fullspec) {
               m__free(fullspec);
           }
       }
    }

    if (server_cb->autosessions) {
        /* save file if it was opened or if the user saved any
         * sessions during this program run
         */
        if (server_cb->session_cfg_file_opened ||
            !dlq_empty(&server_cb->session_cfgQ)) {
            status_t res = save_sessions(server_cb, 
                                         server_cb->session_cfg_file);
            if ( (res != NO_ERR) && (res != ERR_NCX_CANCELED) ) {
                log_error("\nError: could not save configured sessions "
                          "file '%s'\n", 
                          (server_cb->session_cfg_file) ?
                          server_cb->session_cfg_file : get_sessions_file());
            }
        }
    }

    m__free(server_cb->name);
    m__free(server_cb->address);
    m__free(server_cb->password);
    val_free_value(server_cb->local_result);
    m__free(server_cb->result_name);
    m__free(server_cb->result_filename);
    m__free(server_cb->history_filename);
    m__free(server_cb->history_line);
    val_free_value(server_cb->connect_valset);

    clean_completion_state(&server_cb->completion_state);

    /* cleanup the user edit buffer */
    if (server_cb->cli_gl) {
        (void)del_GetLine(server_cb->cli_gl);
    }

    var_clean_varQ(&server_cb->varbindQ);

    if (server_cb->runstack_context) {
        runstack_free_context(server_cb->runstack_context);
    }

    clean_session_cbQ(server_cb, TRUE);

    m__free(server_cb->session_cfg_file);

    while (!dlq_empty(&server_cb->session_cfgQ)) {
        session_cfg_t *session_cfg =
            (session_cfg_t *)dlq_deque(&server_cb->session_cfgQ);
        free_session_cfg(session_cfg);
    }

    yangcli_ut_cleanup(server_cb);

    record_test_cleanup(server_cb);

    m__free(server_cb);

}  /* free_server_cb */


/********************************************************************
* FUNCTION new_server_cb
* 
*  Malloc and init a new server control block
* 
* INPUTS:
*    name == name of server record
*
* RETURNS:
*   malloced server_cb struct or NULL of malloc failed
*********************************************************************/
static server_cb_t *
    new_server_cb (const xmlChar *name)
{
    server_cb_t  *server_cb;
    int          retval;

    server_cb = m__getObj(server_cb_t);
    if (server_cb == NULL) {
        return NULL;
    }
    memset(server_cb, 0x0, sizeof(server_cb_t));

    /* context-specific user variables */
    dlq_createSQue(&server_cb->varbindQ);

    /* sessions for this server */
    dlq_createSQue(&server_cb->session_cfgQ);
    dlq_createSQue(&server_cb->session_cbQ);

    yangcli_ut_init(server_cb);
    record_test_init(server_cb);

    server_cb->history_auto = autohistory;
    server_cb->history_size = YANGCLI_HISTLEN;
    server_cb->get_optional = optional;
    server_cb->program_mode = program_mode;

    /* TBD: add user config for this knob */
    server_cb->overwrite_filevars = TRUE;

    server_cb->runstack_context = runstack_new_context();
    if (server_cb->runstack_context == NULL) {
        m__free(server_cb);
        return NULL;
    }

    /* set the default CLI history file (may not get used) */
    server_cb->history_filename = xml_strdup(YANGCLI_DEF_HISTORY_FILE);
    if (server_cb->history_filename == NULL) {
        free_server_cb(server_cb);
        return NULL;
    }

    /* store per-session temp files */
    server_cb->temp_progcb = temp_progcb;

    /* the name is not used yet; needed when multiple
     * server profiles are needed at once instead
     * of 1 session at a time
     */
    server_cb->name = xml_strdup(name);
    if (server_cb->name == NULL) {
        free_server_cb(server_cb);
        return NULL;
    }

    /* get a tecla CLI control block */
    server_cb->cli_gl = new_GetLine(YANGCLI_LINELEN, YANGCLI_HISTLEN);
    if (server_cb->cli_gl == NULL) {
        log_error("\nError: cannot allocate a new GL\n");
        free_server_cb(server_cb);
        return NULL;
    }

    /* first-time initialize the completion state to init backptrQ */
    first_init_completion_state(&server_cb->completion_state);

    /* setup CLI tab line completion */
    retval = gl_customize_completion(server_cb->cli_gl,
                                     &server_cb->completion_state,
                                     yangcli_tab_callback);
    if (retval != 0) {
        log_error("\nError: cannot set GL tab completion\n");
        free_server_cb(server_cb);
        return NULL;
    }

    /* setup CLI context-sensitive help mode */
    status_t res = register_help_action(server_cb);
    if (res != NO_ERR) {
        log_error("\nError: cannot set libtecla help mode key binding\n");
        free_server_cb(server_cb);
        return NULL;
    }

    /* setup the inactivity timeout callback function */
    retval = gl_inactivity_timeout(server_cb->cli_gl, get_line_timeout,
                                   NULL, 1, 0);
    if (retval != 0) {
        log_error("\nError: cannot set GL inactivity timeout\n");
        free_server_cb(server_cb);
        return NULL;
    }

    /* setup the history buffer if needed */
    if (server_cb->history_auto) {
        retval = gl_load_history(server_cb->cli_gl,
                                 (const char *)server_cb->history_filename,
                                 "#");   /* comment prefix */
        if (retval) {
            log_error("\nError: cannot load command line history buffer\n");
            free_server_cb(server_cb);
            return NULL;

         } else {
            res = NO_ERR;
            xmlChar *fullspec = ncx_get_source(
                     server_cb->history_filename, &res);
            if (res == NO_ERR){
                res = update_def_yangcli_file_mtime (HISTORY_FILE, fullspec);
            }
            if (fullspec) {
                m__free(fullspec);
            }
        }
    }

    return server_cb;

}  /* new_server_cb */


/********************************************************************
* FUNCTION new_session_cb
* 
*  Malloc and init a new session control block
*
* INPUTS:
*   server_cb == server control block to use
*   session_cfg == session config to use for this session
* 
* RETURNS:
*   malloced session_cb struct or NULL of malloc failed
*********************************************************************/
static session_cb_t *
    new_session_cb (server_cb_t *server_cb,
                    session_cfg_t *session_cfg)
{
    (void)server_cb;
    session_cb_t  *session_cb = m__getObj(session_cb_t);
    if (session_cb == NULL) {
        return NULL;
    }
    memset(session_cb, 0x0, sizeof(session_cb_t));

    dlq_createSQue(&session_cb->deviationQ);
    dlq_createSQue(&session_cb->modptrQ);
    dlq_createSQue(&session_cb->notificationQ);
    dlq_createSQue(&session_cb->varbindQ);
    dlq_createSQue(&session_cb->searchresultQ);
    dlq_createSQue(&session_cb->config_editQ);

    session_cb->session_cfg = session_cfg;

    /* set up lock control blocks for get-locks */
    session_cb->locks_active = FALSE;
    session_cb->locks_waiting = FALSE;
    session_cb->locks_cur_cfg = NCX_CFGID_RUNNING;
    session_cb->locks_timeout = 120;
    session_cb->locks_retry_interval = 1;
    session_cb->locks_cleanup = FALSE;
    session_cb->locks_start_time = (time_t)0;
    session_cb->lock_cb[NCX_CFGID_RUNNING].config_id = 
        NCX_CFGID_RUNNING;
    session_cb->lock_cb[NCX_CFGID_RUNNING].config_name = 
        NCX_CFG_RUNNING;
    session_cb->lock_cb[NCX_CFGID_RUNNING].lock_state = 
        LOCK_STATE_IDLE;

    session_cb->lock_cb[NCX_CFGID_CANDIDATE].config_id = 
        NCX_CFGID_CANDIDATE;
    session_cb->lock_cb[NCX_CFGID_CANDIDATE].config_name = 
        NCX_CFG_CANDIDATE;
    session_cb->lock_cb[NCX_CFGID_CANDIDATE].lock_state = 
        LOCK_STATE_IDLE;

    session_cb->lock_cb[NCX_CFGID_STARTUP].config_id = 
        NCX_CFGID_STARTUP;
    session_cb->lock_cb[NCX_CFGID_STARTUP].config_name = 
        NCX_CFG_STARTUP;
    session_cb->lock_cb[NCX_CFGID_STARTUP].lock_state = 
        LOCK_STATE_IDLE;

    /* set default server flags to current settings */
    session_cb->state = MGR_IO_ST_INIT;
    session_cb->baddata = baddata;
    session_cb->log_level = log_get_debug_level();
    session_cb->autoload = autoload;
    session_cb->fixorder = fixorder;
    session_cb->testoption = testoption;
    session_cb->erroption = erroption;
    session_cb->defop = defop;
    session_cb->timeout = default_timeout;
    session_cb->display_mode = display_mode;
    session_cb->withdefaults = withdefaults;
    session_cb->command_mode = CMD_MODE_NORMAL;
    session_cb->prompt_type = prompt_type;
    session_cb->defindent = defindent;
    session_cb->echo_notifs = echo_notifs;
    session_cb->echo_notif_loglevel = echo_notif_loglevel;
    session_cb->echo_replies = echo_replies;
    session_cb->script_input = script_input;
    session_cb->time_rpcs = time_rpcs;
    session_cb->time_rpcs_stats = time_rpcs_stats;
    session_cb->time_rpcs_stats_file = time_rpcs_stats_file;
    session_cb->match_names = match_names;
    session_cb->alt_names = alt_names;
    session_cb->autoconfig = autoconfig;
    session_cb->autonotif = autonotif;

    session_cb->use_xmlheader = use_xmlheader;
    session_cb->config_edit_mode = config_edit_mode;

    return session_cb;

}  /* new_session_cb */


/********************************************************************
 * FUNCTION handle_config_assign
 * 
 * handle a user assignment of a config variable
 * consume memory 'newval' if it is non-NULL
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   session_cb == session control block to use
 *   configval == value to set
 *  use 1 of:
 *   newval == value to use for changing 'configval' 
 *   newvalstr == value to use as string form 
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    handle_config_assign (server_cb_t *server_cb,
                          session_cb_t *session_cb,
                          val_value_t *configval,
                          val_value_t *newval,
                          const xmlChar *newvalstr)
{
    val_value_t *testval = NULL;
    obj_template_t *testobj = NULL;
    status_t res = NO_ERR;
    const xmlChar *usestr = NULL;
    ncx_num_t testnum;

    if (newval) {
        if (!typ_is_string(newval->btyp)) {
            val_free_value( newval );
            return ERR_NCX_WRONG_TYPE;
        }
        if (VAL_STR(newval) == NULL) {
            val_free_value( newval );
            return ERR_NCX_INVALID_VALUE;
        }
        usestr = VAL_STR(newval);
    } else if (newvalstr) {
        usestr = newvalstr;
    } else {
        log_error("\nError: NULL value in config assignment\n");
        return ERR_NCX_INVALID_VALUE;
    }

    if (!xml_strcmp(configval->name, YANGCLI_SERVER)) {
        /* should check for valid IP address!!! */
        if (val_need_quotes(usestr)) {
            /* using this dumb test as a placeholder */
            log_error("\nError: invalid hostname\n");
        } else {
            /* save or update the connnect_valset */
            testval = val_find_child(connect_valset, NULL, YANGCLI_SERVER);
            if (testval) {
                res = val_set_simval(testval, testval->typdef, testval->nsid,
                                     testval->name, usestr);
                if (res != NO_ERR) {
                    log_error("\nError: changing 'server' failed\n");
                }
            } else {
                testobj = obj_find_child(connect_valset->obj, NULL, 
                                         YANGCLI_SERVER);
                if (testobj) {
                    testval = val_make_simval_obj(testobj, usestr, &res);
                    if (testval) {
                        val_add_child(testval, connect_valset);
                    }
                }
            }
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOALIASES)) {
        if (ncx_is_true(usestr)) {
            autoaliases = TRUE;
        } else if (ncx_is_false(usestr)) {
            autoaliases = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_ALIASES_FILE)) {
        aliases_file = usestr;
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOCOMP)) {
        if (ncx_is_true(usestr)) {
            autocomp = TRUE;
        } else if (ncx_is_false(usestr)) {
            autocomp = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOCONFIG)) {
        if (ncx_is_true(usestr)) {
            autoconfig = TRUE;
            session_cb->autoconfig = TRUE;
        } else if (ncx_is_false(usestr)) {
            autoconfig = FALSE;
            session_cb->autoconfig = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTONOTIF)) {
        if (ncx_is_true(usestr)) {
            autonotif = TRUE;
            session_cb->autonotif = TRUE;
        } else if (ncx_is_false(usestr)) {
            autonotif = FALSE;
            session_cb->autonotif = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOHISTORY)) {
        if (ncx_is_true(usestr)) {
            autohistory = TRUE;
        } else if (ncx_is_false(usestr)) {
            autohistory = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOLOAD)) {
        if (ncx_is_true(usestr)) {
            autoload = TRUE;
            session_cb->autoload = TRUE;
        } else if (ncx_is_false(usestr)) {
            autoload = FALSE;
            session_cb->autoload = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOSESSIONS)) {
        if (server_cb->program_mode == PROG_MODE_CLIENT) {
            if (ncx_is_true(usestr)) {
                autosessions = TRUE;
                server_cb->autosessions = TRUE;
            } else if (ncx_is_false(usestr)) {
                autosessions = FALSE;
                server_cb->autosessions = FALSE;
            } else {
                log_error("\nError: value must be 'true' or 'false'\n");
                res = ERR_NCX_INVALID_VALUE;
            }
        } else {
            log_error("\nError: not supported in server mode\n");
            res = ERR_NCX_OPERATION_FAILED;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOTEST)) {
        if (program_mode == PROG_MODE_CLIENT) {
            if (ncx_is_true(usestr)) {
                autotest = TRUE;
            } else if (ncx_is_false(usestr)) {
                autotest = FALSE;
            } else {
                log_error("\nError: value must be 'true' or 'false'\n");
                res = ERR_NCX_INVALID_VALUE;
            }
        } else {
            log_error("\nError: not supported in server mode\n");
            res = ERR_NCX_OPERATION_FAILED;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_AUTOUSERVARS)) {
        if (ncx_is_true(usestr)) {
            autouservars = TRUE;
        } else if (ncx_is_false(usestr)) {
            autouservars = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_USERVARS_FILE)) {
        uservars_file = usestr;
    } else if (!xml_strcmp(configval->name, YANGCLI_TEST_SUITE_FILE)) {
        testsuite_file = usestr;
        testsuite_file_default = FALSE;
    } else if (!xml_strcmp(configval->name, YANGCLI_BADDATA)) {
        ncx_bad_data_t testbaddata = ncx_get_baddata_enum(usestr);
        if (testbaddata != NCX_BAD_DATA_NONE) {
            baddata = testbaddata;
            session_cb->baddata = testbaddata;
        } else {
            log_error("\nError: value must be 'ignore', 'warn', "
                      "'check', or 'error'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_DEF_MODULE)) {
        if (!ncx_valid_name2(usestr)) {
            log_error("\nError: must be a valid module name\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            /* save a copy of the string value */
            xmlChar *dupval = xml_strdup(usestr);
            if (dupval == NULL) {
                log_error("\nError: malloc failed\n");
                res = ERR_INTERNAL_MEM;
            } else {
                if (default_module) {
                    m__free(default_module);
                }
                default_module = dupval;
            }
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_DISPLAY_MODE)) {
        ncx_display_mode_t dmode = ncx_get_display_mode_enum(usestr);
        if (dmode != NCX_DISPLAY_MODE_NONE) {
            display_mode = dmode;
            session_cb->display_mode = dmode;
        } else {
            log_error("\nError: value must be 'plain', 'prefix', "
                      "'module', 'xml', 'xml-nons', or 'json'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_ECHO_NOTIFS)) {
        if (ncx_is_true(usestr)) {
            echo_notifs = TRUE;
            session_cb->echo_notifs = TRUE;
        } else if (ncx_is_false(usestr)) {
            echo_notifs = FALSE;
            session_cb->echo_notifs = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_ECHO_NOTIF_LOGLEVEL)) {
        log_debug_t testloglevel =
            log_get_debug_level_enum((const char *)usestr);
        if (testloglevel == LOG_DEBUG_NONE) {
            log_error("\nError: value must be valid log-level:"
                      "\n       (off, error, warn, info, debug, debug2)\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            echo_notif_loglevel = testloglevel;
            session_cb->echo_notif_loglevel = testloglevel;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_ECHO_REPLIES)) {
        if (ncx_is_true(usestr)) {
            echo_replies = TRUE;
            session_cb->echo_replies = TRUE;
        } else if (ncx_is_false(usestr)) {
            echo_replies = FALSE;
            session_cb->echo_replies = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_SCRIPT_INPUT)) {
        if (ncx_is_true(usestr)) {
            script_input = TRUE;
            session_cb->script_input = TRUE;
        } else if (ncx_is_false(usestr)) {
            script_input = FALSE;
            session_cb->script_input = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_TIME_RPCS)) {
        if (ncx_is_true(usestr)) {
            time_rpcs = TRUE;
            session_cb->time_rpcs = TRUE;
        } else if (ncx_is_false(usestr)) {
            time_rpcs = FALSE;
            session_cb->time_rpcs = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_TIME_RPCS_STATS)) {
        if (ncx_is_true(usestr)) {
            time_rpcs_stats = TRUE;
            session_cb->time_rpcs_stats = TRUE;
        } else if (ncx_is_false(usestr)) {
            time_rpcs_stats = FALSE;
            session_cb->time_rpcs_stats = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_TIME_RPCS_STATS_FILE)) {
        if (val_need_quotes(usestr)) {
            log_error("\nError: invalid filespec value '%s'\n", usestr);
            res = ERR_NCX_INVALID_VALUE;
        } else {
            session_cb->time_rpcs_stats_file = usestr;
        }
    } else if (!xml_strcmp(configval->name, NCX_EL_MATCH_NAMES)) {
        ncx_name_match_t match = ncx_get_name_match_enum(usestr);

        if (match == NCX_MATCH_NONE) {
            log_error("\nError: value must be 'exact', "
                      "'exact-nocase', 'one', 'one-nocase', "
                      "'first', or 'first-nocase'\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            match_names = match;
            session_cb->match_names = match;
        }
    } else if (!xml_strcmp(configval->name, NCX_EL_ALT_NAMES)) {
        if (ncx_is_true(usestr)) {
            alt_names = TRUE;
            session_cb->alt_names = TRUE;
        } else if (ncx_is_false(usestr)) {
            alt_names = FALSE;
            session_cb->alt_names = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_USE_XMLHEADER)) {
        if (ncx_is_true(usestr)) {
            use_xmlheader = TRUE;
            session_cb->use_xmlheader = TRUE;
        } else if (ncx_is_false(usestr)) {
            use_xmlheader = FALSE;
            session_cb->use_xmlheader = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_CONFIG_EDIT_MODE)) {
        config_edit_mode_t editmode = cvt_config_edit_mode_str(usestr);
        if (editmode != CFG_EDITMODE_NONE) {
            log_error("\nError: value must be 'line', 'level', 'mode' or "
                      "'manual'\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            config_edit_mode = editmode;
            session_cb->config_edit_mode = editmode;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_USER)) {
        if (!ncx_valid_name2(usestr)) {
            log_error("\nError: must be a valid user name\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            /* save or update the connnect_valset */
            testval = val_find_child(connect_valset, NULL, YANGCLI_USER);
            if (testval) {
                res = val_set_simval(testval, testval->typdef,
                                     testval->nsid, testval->name, usestr);
                if (res != NO_ERR) {
                    log_error("\nError: changing user name failed\n");
                }
            } else {
                testobj = obj_find_child(connect_valset->obj, NULL, 
                                         YANGCLI_USER);
                if (testobj) {
                    testval = val_make_simval_obj(testobj, usestr, &res);
                }
                if (testval) {
                    val_add_child(testval, connect_valset);
                }
            }
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_TEST_OPTION)) {
        op_testop_t testop;
        if (!xml_strcmp(usestr, NCX_EL_NONE)) {
            testop = OP_TESTOP_NONE;
            session_cb->testoption = OP_TESTOP_NONE;
        } else {            
            testop = op_testop_enum(usestr);
            if (testop != OP_TESTOP_NONE) {
                testoption = testop;
                session_cb->testoption = testop;
            } else {
                log_error("\nError: must be a valid 'test-option'"
                          "\n       (none, test-then-set, set, test-only)\n");
                res = ERR_NCX_INVALID_VALUE;
            }
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_ERROR_OPTION)) {
        if (!xml_strcmp(usestr, NCX_EL_NONE)) {
            erroption = OP_ERROP_NONE;
            session_cb->erroption = OP_ERROP_NONE;
        } else {
            op_errop_t errop = op_errop_id(usestr);
            if (errop != OP_ERROP_NONE) {
                erroption = errop;
                session_cb->erroption = errop;
            } else {
                log_error("\nError: must be a valid 'error-option'"
                          "\n       (none, stop-on-error, "
                          "continue-on-error, rollback-on-error)\n");
                res = ERR_NCX_INVALID_VALUE;
            }
        }
    } else if (!xml_strcmp(configval->name, NCX_EL_DEFAULT_OPERATION)) {
        op_defop_t mydefop = op_defop_id2(usestr);
        if (mydefop != OP_DEFOP_NOT_SET) {
            defop = mydefop;
            session_cb->defop = mydefop;
        } else {
            log_error("\nError: must be a valid 'default-operation'"
                      "\n       (none, merge, replace, not-used)\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_TIMEOUT)) {
        ncx_init_num(&testnum);
        res = ncx_decode_num(usestr, NCX_BT_UINT32, &testnum);
        if (res == NO_ERR) {
            default_timeout = testnum.u;
            session_cb->timeout = testnum.u;
        } else {
            log_error("\nError: must be valid uint32 value\n");
        }
        ncx_clean_num(NCX_BT_UINT32, &testnum);
    } else if (!xml_strcmp(configval->name, YANGCLI_OPTIONAL)) {
        if (ncx_is_true(usestr)) {
            optional = TRUE;
            server_cb->get_optional = TRUE;
        } else if (ncx_is_false(usestr)) {
            optional = FALSE;
            server_cb->get_optional = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, NCX_EL_LOGLEVEL)) {
        log_debug_t testloglevel =
            log_get_debug_level_enum((const char *)usestr);
        if (testloglevel == LOG_DEBUG_NONE) {
            log_error("\nError: value must be valid log-level:"
                      "\n       (off, error,"
                      "warn, info, debug, debug2)\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            log_set_debug_level(testloglevel);
            session_cb->log_level = testloglevel;
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_FIXORDER)) {
        if (ncx_is_true(usestr)) {
            fixorder = TRUE;
            session_cb->fixorder = TRUE;
        } else if (ncx_is_false(usestr)) {
            fixorder = FALSE;
            session_cb->fixorder = FALSE;
        } else {
            log_error("\nError: value must be 'true' or 'false'\n");
            res = ERR_NCX_INVALID_VALUE;
        }
    } else if (!xml_strcmp(configval->name, NCX_EL_WITH_DEFAULTS)) {
        if (!xml_strcmp(usestr, NCX_EL_NONE)) {
            withdefaults = NCX_WITHDEF_NONE;
        } else {
            if (ncx_get_withdefaults_enum(usestr) == NCX_WITHDEF_NONE) {
                log_error("\nError: value must be 'none', "
                          "'report-all', 'trim', or 'explicit'\n");
                res = ERR_NCX_INVALID_VALUE;
            } else {
                withdefaults = ncx_get_withdefaults_enum(usestr);
                session_cb->withdefaults = withdefaults;
            }
        }
    } else if (!xml_strcmp(configval->name, YANGCLI_PROMPT_TYPE)) {
        help_mode_t ptype = help_cvt_str(usestr);
        if (ptype == HELP_MODE_NONE) {
            log_error("\nError: value must be 'brief', "
                      "'normal', or 'full'\n");
            res = ERR_NCX_INVALID_VALUE;
        } else {
            prompt_type = ptype;
            session_cb->prompt_type = ptype;
        }
    } else if (!xml_strcmp(configval->name, NCX_EL_INDENT)) {
        ncx_init_num(&testnum);
        res = ncx_decode_num(usestr, NCX_BT_UINT32, &testnum);
        if (res == NO_ERR) {
            if (testnum.u > YANGCLI_MAX_INDENT) {
                log_error("\nError: value must be a uint32 (0..9)\n");
                res = ERR_NCX_INVALID_VALUE;
            } else {
                /* indent value is valid */
                defindent = (int32)testnum.u;
                session_cb->defindent = defindent;
            }
        } else {
            log_error("\nError: must be valid uint32 value\n");
        }
        ncx_clean_num(NCX_BT_INT32, &testnum);
    } else if (!xml_strcmp(configval->name, NCX_EL_MESSAGE_INDENT)) {
        ncx_init_num(&testnum);
        res = ncx_decode_num(usestr, NCX_BT_INT8, &testnum);
        if (res == NO_ERR) {
            if (testnum.i > YANGCLI_MAX_INDENT || testnum.i < -1) {
                log_error("\nError: value must be a int8 (-1..9)\n");
                res = ERR_NCX_INVALID_VALUE;
            } else {
                /* message-indent value is valid */
                msg_defindent = testnum.i;
                ncx_set_message_indent(testnum.i);
            }
        } else {
            log_error("\nError: must be valid int8 value\n");
        }
        ncx_clean_num(NCX_BT_INT8, &testnum);
    } else {
        /* unknown parameter name */
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* update the variable value for user access */
    if (res == NO_ERR) {
        if (newval) {
            res = var_set_move(server_cb->runstack_context,
                               configval->name, 
                               xml_strlen(configval->name),
                               VAR_TYP_CONFIG, 
                               newval);
        } else {
            res = var_set_from_string(server_cb->runstack_context,
                                      configval->name,
                                      newvalstr, 
                                      VAR_TYP_CONFIG);
        }
        if (res != NO_ERR) {
            log_error("\nError: set result for '%s' failed (%s)\n",
                          server_cb->result_name, 
                          get_error_string(res));
        }
    }

    if (res == NO_ERR) {
        log_info("\nSystem variable set\n");
    }
    return res;

} /* handle_config_assign */


/********************************************************************
* FUNCTION handle_delete_result
* 
* Delete the specified file, if it is ASCII and a regular file
*
* INPUTS:
*    server_cb == server control block to use
*
* OUTPUTS:
*    server_cb->result_filename will get deleted if NO_ERR
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    handle_delete_result (server_cb_t *server_cb)
{
    status_t     res;
    struct stat  statbuf;
    int          statresult;

    res = NO_ERR;

    if (LOGDEBUG2) {
        log_debug2("\n*** delete file result '%s'",
                   server_cb->result_filename);
    }

    /* see if file already exists */
    statresult = stat((const char *)server_cb->result_filename,
                      &statbuf);
    if (statresult != 0) {
        log_error("\nError: assignment file '%s' could not be opened\n",
                  server_cb->result_filename);
        res = errno_to_status();
    } else if (!S_ISREG(statbuf.st_mode)) {
        log_error("\nError: assignment file '%s' is not a regular file\n",
                  server_cb->result_filename);
        res = ERR_NCX_OPERATION_FAILED;
    } else {
        statresult = remove((const char *)server_cb->result_filename);
        if (statresult == -1) {
            log_error("\nError: assignment file '%s' could not be deleted\n",
                      server_cb->result_filename);
            res = errno_to_status();
        }
    }

    clear_result(server_cb);

    return res;

}  /* handle_delete_result */


/********************************************************************
* FUNCTION output_file_result
* 
* Check the filespec string for a file assignment statement
* Save it if it is good
*
* INPUTS:
*    server_cb == server control block to use
*    session_cb == session control block to use
*
* use 1 of these 2 parms:
*    resultval == result to output to file
*    resultstr == result to output as string
*
* OUTPUTS:
*    server_cb->result_filename will get set if NO_ERR
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    output_file_result (server_cb_t *server_cb,
                        session_cb_t *session_cb,
                        val_value_t *resultval,
                        const xmlChar *resultstr)
{
    FILE        *fil;
    status_t     res;
    xml_attrs_t  attrs;
    struct stat  statbuf;
    int          statresult;

    res = NO_ERR;

    if (LOGDEBUG2) {
        log_debug2("\n*** output file result to '%s'",
                   server_cb->result_filename);
    }

    /* see if file already exists, unless overwrite_filevars is set */
    if (server_cb->overwrite_filevars) {
        statresult = stat((const char *)server_cb->result_filename,
                          &statbuf);
        if (statresult == 0) {
            log_error("\nError: assignment file '%s' already exists\n",
                      server_cb->result_filename);
            clear_result(server_cb);
            return ERR_NCX_DATA_EXISTS;
        }
    }

    ncx_display_mode_t dispmode;
    if (resultval) {
        result_format_t rf = 
            get_file_result_format(server_cb->result_filename);
        switch (rf) {
        case RF_TEXT:
            /* output in text format to the specified file */
            res = log_alt_open((const char *)
                               server_cb->result_filename);
            if (res != NO_ERR) {
                log_error("\nError: assignment file '%s' could "
                          "not be opened (%s)\n",
                          server_cb->result_filename,
                          get_error_string(res));
            } else {
                val_dump_value_max(resultval,
                                   0,   /* startindent */
                                   session_cb->defindent,
                                   DUMP_VAL_ALT_LOG, /* dumpmode */
                                   session_cb->display_mode,
                                   FALSE,    /* withmeta */
                                   FALSE);   /* configonly */
                log_alt_close();
            }
            break;
        case RF_XML:
            /* output in XML format to the specified file */
            xml_init_attrs(&attrs);
            dispmode = session_cb->display_mode;
            res = xml_wr_file(server_cb->result_filename,
                              resultval, &attrs, XMLMODE, 
                              session_cb->use_xmlheader,
                              (dispmode == NCX_DISPLAY_MODE_XML_NONS) 
                              ? FALSE : TRUE, 0,
                              session_cb->defindent);
            xml_clean_attrs(&attrs);
            break;
        case RF_JSON:
            /* output in JSON format to the specified file */
            res = json_wr_file(server_cb->result_filename,
                               resultval, 0, session_cb->defindent);
            break;
        case RF_NONE:
        default:
            SET_ERROR(ERR_INTERNAL_VAL);
        }
    } else if (resultstr) {
        fil = fopen((const char *)server_cb->result_filename, "w");
        if (fil == NULL) {
            log_error("\nError: assignment file '%s' could "
                      "not be opened\n",
                      server_cb->result_filename);
            res = errno_to_status();
        } else {
            statresult = fputs((const char *)resultstr, fil);
            if (statresult == EOF) {
                log_error("\nError: assignment file '%s' could "
                          "not be written\n",
                          server_cb->result_filename);
                res = errno_to_status();
            } else {
                statresult = fputc('\n', fil);  
                if (statresult == EOF) {
                    log_error("\nError: assignment file '%s' could "
                              "not be written\n",
                              server_cb->result_filename);
                    res = errno_to_status();
                }
            }
        }
        if (fil != NULL) {
            statresult = fclose(fil);
            if (statresult == EOF) {
                log_error("\nError: assignment file '%s' could "
                          "not be closed\n",
                          server_cb->result_filename);
                res = errno_to_status();
            }
        }
    } else {
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    clear_result(server_cb);

    return res;

}  /* output_file_result */


/********************************************************************
* FUNCTION check_assign_statement
* 
* Check if the command line is an assignment statement
* 
* E.g.,
*
*   $foo = $bar
*   $foo = get-config filter=@filter.xml
*   $foo = "quoted string literal"
*   $foo = [<inline><xml/></inline>]
*   $foo = @datafile.xml
*
* INPUTS:
*   server_cb == server control block to use
*   session_cb == session control block to use (may be NULL)
*   line == command line string to expand
*   len  == address of number chars parsed so far in line
*   getrpc == address of return flag to parse and execute an
*            RPC, which will be assigned to the var found
*            in the 'result' module variable
*   fileassign == address of file assign flag
*
* OUTPUTS:
*   *len == number chars parsed in the assignment statement
*   *getrpc == TRUE if the rest of the line represent an
*              RPC command that needs to be evaluated
*   *fileassign == TRUE if the assignment is for a
*                  file output (starts with @)
*                  FALSE if not a file assignment
*
* SIDE EFFECTS:
*    If this is an 'rpc' assignment statement, 
*    (*dorpc == TRUE && *len > 0 && return NO_ERR),
*    then the global result and result_pending variables will
*    be set upon return.  For the other (direct) assignment
*    statement variants, the statement is completely handled
*    here.
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    check_assign_statement (server_cb_t *server_cb,
                            session_cb_t *session_cb,
                            const xmlChar *line,
                            uint32 *len,
                            boolean *getrpc,
                            boolean *fileassign)
{
    *len = 0;
    *getrpc = FALSE;
    *fileassign = FALSE;

    const xmlChar *name = NULL;
    uint32 nlen = 0;
    uint32 tlen = 0;
    status_t res = NO_ERR;

    /* save start point in line */
    const xmlChar *str = line;
    var_type_t vartype = VAR_TYP_NONE;
    val_value_t *curval = NULL;

    /* skip leading whitespace */
    while (*str && isspace(*str)) {
        str++;
    }

    if (session_cb->config_mode && (*str == '@' || *str == '$')) {
        log_error("\nError: assignment statements not allowed "
                  "in config mode\n");
        *len = (uint32)(str - line);
        return ERR_NCX_OPERATION_FAILED;
    }

    if (*str == '@') {
        /* check if valid file assignment is being made */
        *fileassign = TRUE;
        str++;
    }

    if (*str == '$') {
        /* check if a valid variable assignment is being made */
        res = var_check_ref(server_cb->runstack_context,
                            str, ISLEFT, &tlen, &vartype, &name, &nlen);
        if (res != NO_ERR) {
            /* error in the varref */
            return res;
        } else if (tlen == 0) {
            /* should not happen: returned not a varref */
            *getrpc = TRUE;
            return NO_ERR;
        } else if (*fileassign) {
            /* file assignment complex form:
             *
             *    @$foo = bar or @$$foo = bar
             *
             * get the var reference for real because
             * it is supposed to contain the filespec
             * for the output file
             */
            curval = var_get_str(server_cb->runstack_context,
                                 name, nlen, vartype);
            if (curval == NULL) {
                log_error("\nError: file assignment variable "
                          "not found\n");
                return ERR_NCX_VAR_NOT_FOUND;
            }

            /* variable must be a string */
            if (!typ_is_string(curval->btyp)) {
                log_error("\nError: file assignment variable '%s' "
                          "is wrong type '%s'\n",
                          curval->name,
                          tk_get_btype_sym(curval->btyp));
                return ERR_NCX_VAR_NOT_FOUND;
            }
            const xmlChar *filespec = VAL_STR(curval);
            res = check_filespec(server_cb, filespec, curval->name);
            if (res != NO_ERR) {
                return res;
            }
        } else {
            /* variable reference:
             *
             *     $foo or $$foo
             *
             * check for a valid varref, get the data type, which
             * will also indicate if the variable exists yet
             */
            switch (vartype) {
            case VAR_TYP_SYSTEM:
                log_error("\nError: system variables are read-only\n");
                return ERR_NCX_VAR_READ_ONLY;
            case VAR_TYP_GLOBAL:
            case VAR_TYP_CONFIG:
                update_yangcli_param_change_flag (USERVARS_FILE, TRUE);
                curval = var_get_str(server_cb->runstack_context,
                                     name, nlen, vartype);
                break;
            case VAR_TYP_LOCAL:
            case VAR_TYP_SESSION:
                update_yangcli_param_change_flag (USERVARS_FILE, TRUE);
                curval = var_get_local_str(server_cb->runstack_context,
                                           name, nlen);
                break;
            default:
                return SET_ERROR(ERR_INTERNAL_VAL);
            }
        }
        /* move the str pointer past the variable name */
        str += tlen;
    } else if (*fileassign) {
        /* file assignment, simple form:
         *
         *     @foo.txt = bar
         *
         * get the length of the filename 
         */
        name = str;
        while (*str && !isspace(*str) && *str != '=') {
            str++;
        }
        nlen = (uint32)(str-name);

        /* save the filename in a temp string */
        xmlChar *tempstr = xml_strndup(name, nlen);
        if (tempstr == NULL) {
            return ERR_INTERNAL_MEM;
        }

        /* check filespec and save filename for real */
        res = check_filespec(server_cb, tempstr, NULL);

        m__free(tempstr);

        if (res != NO_ERR) {
            return res;
        }
    } else {
        /* not an assignment statement at all */
        *getrpc = TRUE;
        return NO_ERR;
    }

    /* skip any more whitespace, after the LHS term */
    while (*str && xml_isspace(*str)) {
        str++;
    }

    /* check end of string */
    if (!*str) {
        log_error("\nError: truncated assignment statement\n");
        clear_result(server_cb);
        return ERR_NCX_DATA_MISSING;
    }

    /* check for the equals sign assignment char */
    if (*str == NCX_ASSIGN_CH) {
        /* move past assignment char */
        str++;
    } else {
        log_error("\nError: equals sign '=' expected\n");
        clear_result(server_cb);
        return ERR_NCX_WRONG_TKTYPE;
    }

    /* skip any more whitespace */
    while (*str && xml_isspace(*str)) {
        str++;
    }

    /* check EO string == unset command, but only if in a TRUE conditional */
    if (!*str && runstack_get_cond_state(server_cb->runstack_context)) {
        if (*fileassign) {
            /* got file assignment (@foo) =  EOLN
             * treat this as a request to delete the file
             */
            res = handle_delete_result(server_cb);
        } else {
            /* got $foo =  EOLN
             * treat this as a request to unset the variable
             */
            if (vartype == VAR_TYP_SYSTEM ||
                vartype == VAR_TYP_CONFIG) {
                log_error("\nError: cannot remove system variables\n");
                clear_result(server_cb);
                return ERR_NCX_OPERATION_FAILED;
            }

            /* else try to unset this variable */
            res = var_unset(server_cb->runstack_context, name, nlen, vartype);
        }
        *len = (uint32)(str - line);
        return res;
    }

    /* the variable name and equals sign is parsed
     * and the current char is either '$', '"', '<',
     * or a valid first name
     *
     * if *fileassign:
     *
     *      @foo.xml = blah
     *               ^
     *  else:
     *      $foo = blah
     *           ^
     */
    obj_template_t *obj;
    if (*fileassign) {
        obj = NULL;
    } else {
        obj = (curval) ? curval->obj : NULL;
    }

    /* get the script or CLI input as a new val_value_t struct */
    val_value_t *val =
        var_check_script_val(server_cb->runstack_context,
                             obj, str, ISTOP, &res);
    if (val) {
        if (obj == NULL) {
            obj = val->obj;
        }

        /* a script value reference was found */
        if (obj == NULL || obj == ncx_get_gen_string()) {
            /* the generic name needs to be overwritten */
            val_set_name(val, name, nlen);
        }

        if (*fileassign) {
            /* file assignment of a variable value 
             *   @foo.txt=$bar  or @$foo=$bar
             */
            res = output_file_result(server_cb, session_cb, val, NULL);
            val_free_value(val);
        } else {
            /* this is a plain assignment statement
             * first check if the input is VAR_TYP_CONFIG
             */
            if (vartype == VAR_TYP_CONFIG) {
                if (curval==NULL) {
                    val_free_value(val);
                    res = SET_ERROR(ERR_INTERNAL_VAL);
                } else {
                    /* hand off 'val' memory here */
                    res = handle_config_assign(server_cb, session_cb,
                                               curval, val, NULL);
                }
            } else {
                /* val is a malloced struct, pass it over to the
                 * var struct instead of cloning it
                 */
                res = var_set_move(server_cb->runstack_context,
                                   name, nlen, vartype, val);
                if (res != NO_ERR) {
                    val_free_value(val);
                }
            }
        }
    } else if (res==NO_ERR) {
        /* this is as assignment to the results
         * of an RPC function call 
         */
        if (server_cb->result_name) {
            log_error("\nError: result already pending for %s\n",
                     server_cb->result_name);
            m__free(server_cb->result_name);
            server_cb->result_name = NULL;
        }

        if (!*fileassign) {
            /* save the variable result name */
            server_cb->result_name = xml_strndup(name, nlen);
            if (server_cb->result_name == NULL) {
                *len = 0;
                res = ERR_INTERNAL_MEM;
            } else {
                server_cb->result_vartype = vartype;
            }
        }

        if (res == NO_ERR) {
            *len = str - line;
            *getrpc = TRUE;
        }
    } else {
        /* there was some error in the statement processing */
        *len = 0;
        clear_result(server_cb);
    }

    return res;

}  /* check_assign_statement */


/********************************************************************
 * FUNCTION create_system_var
 * 
 * create a read-only system variable
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   varname == variable name
 *   varval  == variable string value (may be NULL)
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    create_system_var (server_cb_t *server_cb,
                       const char *varname,
                       const char *varval)
{
    status_t res = var_set_from_string(server_cb->runstack_context,
                                       (const xmlChar *)varname,
                                       (const xmlChar *)varval,
                                       VAR_TYP_SYSTEM);
    return res;

} /* create_system_var */


/********************************************************************
 * FUNCTION create_config_var
 * 
 * create a read-write system variable
 *
 * INPUTS:
 *   server_cb == server control block
 *   varname == variable name
 *   varval  == variable string value (may be NULL)
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    create_config_var (server_cb_t *server_cb,
                       const xmlChar *varname,
                       const xmlChar *varval)
{
    status_t res = var_set_from_string(server_cb->runstack_context,
                                       varname, varval, VAR_TYP_CONFIG);
    return res;

} /* create_config_var */


/********************************************************************
 * FUNCTION init_system_vars
 * 
 * create the read-only system variables
 *
 * INPUTS:
 *   server_cb == server control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    init_system_vars (server_cb_t *server_cb)
{
    const char *envstr;
    status_t    res;

    envstr = getenv(NCXMOD_PWD);
    res = create_system_var(server_cb, NCXMOD_PWD, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = (const char *)ncxmod_get_home();
    res = create_system_var(server_cb, USER_HOME, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(ENV_HOST);
    res = create_system_var(server_cb, ENV_HOST, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(ENV_SHELL);
    res = create_system_var(server_cb, ENV_SHELL, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(ENV_USER);
    res = create_system_var(server_cb, ENV_USER, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(ENV_LANG);
    res = create_system_var(server_cb, ENV_LANG, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(NCXMOD_HOME);
    res = create_system_var(server_cb, NCXMOD_HOME, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(NCXMOD_MODPATH);
    res = create_system_var(server_cb, NCXMOD_MODPATH, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(NCXMOD_DATAPATH);
    res = create_system_var(server_cb, NCXMOD_DATAPATH, envstr);
    if (res != NO_ERR) {
        return res;
    }

    envstr = getenv(NCXMOD_RUNPATH);
    res = create_system_var(server_cb, NCXMOD_RUNPATH, envstr);
    if (res != NO_ERR) {
        return res;
    }

    return NO_ERR;

} /* init_system_vars */


/********************************************************************
 * FUNCTION init_config_vars
 * 
 * create the read-write global variables
 *
 * INPUTS:
 *   server_cb == server control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    init_config_vars (server_cb_t *server_cb)
{
    val_value_t    *parm;
    const xmlChar  *strval;
    status_t        res;
    xmlChar         numbuff[NCX_MAX_NUMLEN];

    /* $$server = ip-address */
    strval = NULL;
    parm = val_find_child(mgr_cli_valset, NULL, YANGCLI_SERVER);
    if (parm) {
        strval = VAL_STR(parm);
    }
    res = create_config_var(server_cb, YANGCLI_SERVER, strval);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ autoaliases = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTOALIASES, 
                            (autoaliases) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ aliases-file = filespec */
    res = create_config_var(server_cb, YANGCLI_ALIASES_FILE, aliases_file);
    if (res != NO_ERR) {
        return res;
    }

    /* $$autocomp = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTOCOMP, 
                            (autocomp) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$autoconfig = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTOCONFIG, 
                            (autoconfig) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ autohistory = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTOHISTORY, 
                            (autohistory) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ autoload = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTOLOAD, 
                            (autoload) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$autonotif = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTONOTIF, 
                            (autonotif) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ autosessions = boolean */
    if (program_mode == PROG_MODE_CLIENT) {
        res = create_config_var(server_cb, YANGCLI_AUTOSESSIONS, 
                                (autosessions) ? NCX_EL_TRUE : NCX_EL_FALSE);
        if (res != NO_ERR) {
            return res;
        }
    }

    if (program_mode == PROG_MODE_CLIENT) {
        /* $$ autotest = boolean */
        res = create_config_var(server_cb, YANGCLI_AUTOTEST, 
                                (autotest) ? NCX_EL_TRUE : NCX_EL_FALSE);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* $$ autouservars = boolean */
    res = create_config_var(server_cb, YANGCLI_AUTOUSERVARS, 
                            (autouservars) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ uservars-file = filespec */
    res = create_config_var(server_cb, YANGCLI_USERVARS_FILE, uservars_file);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ test-suite-file = filespec */
    res = create_config_var(server_cb, YANGCLI_TEST_SUITE_FILE, 
                            testsuite_file);
    if (res != NO_ERR) {
        return res;
    }

    /* $$baddata = enum */
    res = create_config_var(server_cb, YANGCLI_BADDATA, 
                            ncx_get_baddata_string(baddata));
    if (res != NO_ERR) {
        return res;
    }

    /* $$default-module = string */
    res = create_config_var(server_cb, YANGCLI_DEF_MODULE, default_module);
    if (res != NO_ERR) {
        return res;
    }

    /* $$display-mode = enum */
    res = create_config_var(server_cb, YANGCLI_DISPLAY_MODE, 
                            ncx_get_display_mode_str(display_mode));
    if (res != NO_ERR) {
        return res;
    }

    /* $$echo-notif-loglevel = enum */
    res = create_config_var(server_cb, YANGCLI_ECHO_NOTIF_LOGLEVEL,
                            log_get_debug_level_string(echo_notif_loglevel));
    if (res != NO_ERR) {
        return res;
    }

    /* $$echo-notifs = boolean */
    res = create_config_var(server_cb, YANGCLI_ECHO_NOTIFS, 
                            (echo_notifs) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$echo-replies = boolean */
    res = create_config_var(server_cb, YANGCLI_ECHO_REPLIES, 
                            (echo_replies) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ script-input = boolean */
    res = create_config_var(server_cb, YANGCLI_SCRIPT_INPUT, 
                            (script_input) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ time-rpcs = boolean */
    res = create_config_var(server_cb, YANGCLI_TIME_RPCS, 
                            (time_rpcs) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ time-rpcs-stats = boolean */
    res = create_config_var(server_cb, YANGCLI_TIME_RPCS_STATS, 
                            (time_rpcs_stats) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ time-rpcs-stats-file = filespec */
    res = create_config_var(server_cb, YANGCLI_TIME_RPCS_STATS_FILE, 
                            time_rpcs_stats_file);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ match-names = enum */
    res = create_config_var(server_cb, NCX_EL_MATCH_NAMES, 
                            ncx_get_name_match_string(match_names));
    if (res != NO_ERR) {
        return res;
    }

    /* $$ alt-names = boolean */
    res = create_config_var(server_cb, NCX_EL_ALT_NAMES, 
                            (alt_names) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ use-xmlheader = boolean */
    res = create_config_var(server_cb, YANGCLI_USE_XMLHEADER, 
                            (use_xmlheader) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$ config-edit-mode = enum */
    res = create_config_var(server_cb, YANGCLI_CONFIG_EDIT_MODE, 
                            cvt_config_edit_mode_enum(config_edit_mode));
    if (res != NO_ERR) {
        return res;
    }

    /* $$user = string */
    strval = NULL;
    parm = val_find_child(mgr_cli_valset, NULL, YANGCLI_USER);
    if (parm) {
        strval = VAL_STR(parm);
    } else {
        strval = (const xmlChar *)getenv(ENV_USER);
    }
    res = create_config_var(server_cb, YANGCLI_USER, strval);
    if (res != NO_ERR) {
        return res;
    }

    /* $$test-option = enum */
    res = create_config_var(server_cb, YANGCLI_TEST_OPTION,
                            op_testop_name(testoption));
    if (res != NO_ERR) {
        return res;
    }

    /* $$error-optiona = enum */
    res = create_config_var(server_cb, YANGCLI_ERROR_OPTION,
                            op_errop_name(erroption)); 
    if (res != NO_ERR) {
        return res;
    }

    /* $$default-timeout = uint32 */
    sprintf((char *)numbuff, "%u", default_timeout);
    res = create_config_var(server_cb, YANGCLI_TIMEOUT, numbuff);
    if (res != NO_ERR) {
        return res;
    }

    /* $$indent = int32 */
    sprintf((char *)numbuff, "%d", defindent);
    res = create_config_var(server_cb, NCX_EL_INDENT, numbuff);
    if (res != NO_ERR) {
        return res;
    }

    /* $$message-indent = int8 */
    sprintf((char *)numbuff, "%d", msg_defindent);
    res = create_config_var(server_cb, NCX_EL_MESSAGE_INDENT, numbuff);
    if (res != NO_ERR) {
        return res;
    }

    /* $$optional = boolean */
    res = create_config_var(server_cb, YANGCLI_OPTIONAL, 
                            (server_cb->get_optional) 
                            ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$log-level = enum
     * could have changed during CLI processing; do not cache
     */
    res = create_config_var(server_cb, NCX_EL_LOGLEVEL, 
                            log_get_debug_level_string
                            (log_get_debug_level()));
    if (res != NO_ERR) {
        return res;
    }

    /* $$fixorder = boolean */
    res = create_config_var(server_cb, YANGCLI_FIXORDER, 
                            (fixorder) ? NCX_EL_TRUE : NCX_EL_FALSE);
    if (res != NO_ERR) {
        return res;
    }

    /* $$with-defaults = enum */
    res = create_config_var(server_cb, YANGCLI_WITH_DEFAULTS,
                            ncx_get_withdefaults_string(withdefaults)); 
    if (res != NO_ERR) {
        return res;
    }

    /* $$prompt-type = enum */
    res = create_config_var(server_cb, YANGCLI_PROMPT_TYPE,
                            help_cvt_enum(prompt_type)); 
    if (res != NO_ERR) {
        return res;
    }

    /* $$default-operation = enum */
    res = create_config_var(server_cb, NCX_EL_DEFAULT_OPERATION,
                            op_defop_name(defop)); 
    if (res != NO_ERR) {
        return res;
    }

    return NO_ERR;

} /* init_config_vars */


/********************************************************************
* FUNCTION process_cli_input
*
* Process the param line parameters against the hardwired
* parmset for the ncxmgr program
*
* INPUTS:
*    server_cb == server control block to use
*    argc == argument count
*    argv == array of command line argument strings
*
* OUTPUTS:
*    global vars that are present are filled in, with parms 
*    gathered or defaults
*
* RETURNS:
*    NO_ERR if all goes well
*********************************************************************/
static status_t
    process_cli_input (server_cb_t *server_cb,
                       int argc,
                       char *argv[])
{
    boolean defs_done = FALSE;
    status_t res = NO_ERR;

    mgr_cli_valset = NULL;

    /* find the parmset definition in the registry */
    obj_template_t *obj = ncx_find_object(yangcli_mod, YANGCLI_BOOT);
    if (obj == NULL) {
        res = ERR_NCX_NOT_FOUND;
    }

    if (res == NO_ERR) {
        /* check no command line parms */
        if (argc <= 1) {
            mgr_cli_valset = val_new_value();
            if (mgr_cli_valset == NULL) {
                res = ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(mgr_cli_valset, obj);
            }
        } else {
            /* parse the command line against the object template */    
            mgr_cli_valset = cli_parse(server_cb->runstack_context,
                                       argc, 
                                       argv, 
                                       obj,
                                       FULLTEST, 
                                       PLAINMODE,
                                       autocomp,
                                       CLI_MODE_PROGRAM,
                                       &res);
	    defs_done = TRUE;
        }
    }

    if (res != NO_ERR) {
        if (mgr_cli_valset) {
            val_free_value(mgr_cli_valset);
            mgr_cli_valset = NULL;
        }
        return res;
    }

    /* next get any params from the conf file
     * check choice config or no-config
     */
    confname = get_strparm(mgr_cli_valset, YANGCLI_MOD, YANGCLI_CONFIG);
    if (confname != NULL) {
        res = conf_parse_val_from_filespec(confname, mgr_cli_valset,
                                           TRUE, TRUE);
    } else if (val_find_child(mgr_cli_valset, YANGCLI_MOD, 
                              NCX_EL_NO_CONFIG)) {
        log_info("\nSkipping default config file because "
                 "--no-config specified");
    } else {
        res = conf_parse_val_from_filespec(YANGCLI_DEF_CONF_FILE,
                                           mgr_cli_valset,
                                           TRUE, FALSE);
    }

    if (res == NO_ERR && defs_done == FALSE) {
        res = val_add_defaults(mgr_cli_valset, NULL, NULL, FALSE);
    }

    if (res != NO_ERR) {
        return res;
    }

    /****************************************************
     * go through the yangcli params in order,
     * after setting up the logging parameters
     ****************************************************/

    /* set the --home and --yumapro-home parameters */
    val_set_home_parms(mgr_cli_valset);

    /* set the logging control parameters */
    val_set_logging_parms(mgr_cli_valset);

    /* set the file search path parms */
    val_set_path_parms(mgr_cli_valset);

    /* set the warning control parameters */
    val_set_warning_parms(mgr_cli_valset);

    /* set the subdirs parm */
    val_set_subdirs_parm(mgr_cli_valset);

    /* set the protocols parm */
    res = val_set_protocols_parm(mgr_cli_valset);
    if (res != NO_ERR) {
        return res;
    }

    /* check the message-indent parameter */
    res = val_set_message_indent_parm(mgr_cli_valset);
    if (res != NO_ERR) {
        return res;
    }

    /* set the feature code generation parameters */
    val_set_feature_parms(mgr_cli_valset);

    /* get the server parameter */
    val_value_t *parm =
        val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_SERVER);
    if (parm && parm->res == NO_ERR) {
        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the autoaliases parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOALIASES);
    if (parm && parm->res == NO_ERR) {
        autoaliases = VAL_BOOL(parm);
    }

    /* get the aliases-file parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_ALIASES_FILE);
    if (parm && parm->res == NO_ERR) {
        aliases_file = VAL_STR(parm);
    }

    /* get the autocomp parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOCOMP);
    if (parm && parm->res == NO_ERR) {
        autocomp = VAL_BOOL(parm);
    }

    /* get the autoconfig parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOCONFIG);
    if (parm && parm->res == NO_ERR) {
        autoconfig = VAL_BOOL(parm);
    }

    /* get the autohistory parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOHISTORY);
    if (parm && parm->res == NO_ERR) {
        autohistory = VAL_BOOL(parm);
    }

    /* get the autoload parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOLOAD);
    if (parm && parm->res == NO_ERR) {
        autoload = VAL_BOOL(parm);
    }

    /* get the autonotif parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTONOTIF);
    if (parm && parm->res == NO_ERR) {
        autonotif = VAL_BOOL(parm);
    }

    /* get the autosessions parameter */
    if (program_mode == PROG_MODE_CLIENT) {
        parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, 
                              YANGCLI_AUTOSESSIONS);
        if (parm && parm->res == NO_ERR) {
            autosessions = VAL_BOOL(parm);
            server_cb->autosessions = VAL_BOOL(parm);
        }
    }

    /* get the autotest parameter */
    if (program_mode == PROG_MODE_CLIENT) {
        parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOTEST);
        if (parm && parm->res == NO_ERR) {
            autotest = VAL_BOOL(parm);
        }
    }

    /* get the autouservars parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_AUTOUSERVARS);
    if (parm && parm->res == NO_ERR) {
        autouservars = VAL_BOOL(parm);
    }

    /* get the baddata parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_BADDATA);
    if (parm && parm->res == NO_ERR) {
        baddata = ncx_get_baddata_enum(VAL_ENUM_NAME(parm));
        if (baddata == NCX_BAD_DATA_NONE) {
            SET_ERROR(ERR_INTERNAL_VAL);
            baddata = YANGCLI_DEF_BAD_DATA;
        }
    }

    /* get the batch-mode parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_BATCHMODE);
    if (parm && parm->res == NO_ERR) {
        batchmode = TRUE;
    }

    /* get the default module for unqualified module addesses */
    default_module = get_strparm(mgr_cli_valset, YANGCLI_MOD, 
                                 YANGCLI_DEF_MODULE);

    /* get the display-mode parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_DISPLAY_MODE);
    if (parm && parm->res == NO_ERR) {
        ncx_display_mode_t dmode =
            ncx_get_display_mode_enum(VAL_ENUM_NAME(parm));
        if (dmode != NCX_DISPLAY_MODE_NONE) {
            display_mode = dmode;
        } else {
            display_mode = YANGCLI_DEF_DISPLAY_MODE;
        }
    }

    /* get the echo-notif-loglevel parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD,
                          YANGCLI_ECHO_NOTIF_LOGLEVEL);
    if (parm && parm->res == NO_ERR) {
        echo_notif_loglevel =
            log_get_debug_level_enum((const char *)VAL_ENUM_NAME(parm));
    }

    /* get the echo-notifs parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_ECHO_NOTIFS);
    if (parm && parm->res == NO_ERR) {
        if (!val_set_by_default(parm)) {
            echo_notifs = VAL_BOOL(parm);
        }
    }

    /* get the echo-replies parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_ECHO_REPLIES);
    if (parm && parm->res == NO_ERR) {
        if (!val_set_by_default(parm)) {
            echo_replies = VAL_BOOL(parm);
        }
    }

    /* get the script-input parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_SCRIPT_INPUT);
    if (parm && parm->res == NO_ERR) {
        script_input = VAL_BOOL(parm);
    }

    /* get the time-rpcs parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_TIME_RPCS);
    if (parm && parm->res == NO_ERR) {
        time_rpcs = VAL_BOOL(parm);
    }

    /* get the time-rpcs-stats parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, 
                          YANGCLI_TIME_RPCS_STATS);
    if (parm && parm->res == NO_ERR) {
        time_rpcs_stats = VAL_BOOL(parm);
    }

    /* get the time-rpcs-stats-file parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, 
                          YANGCLI_TIME_RPCS_STATS_FILE);
    if (parm && parm->res == NO_ERR) {
        time_rpcs_stats_file = VAL_STR(parm);
    }

    /* get the time-rpcs-stats-file parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, 
                          YANGCLI_TIME_RPCS_STATS_FILE);
    if (parm && parm->res == NO_ERR) {
        time_rpcs_stats_file = VAL_STR(parm);
    }

    /* get the fixorder parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_FIXORDER);
    if (parm && parm->res == NO_ERR) {
        fixorder = VAL_BOOL(parm);
    }

    /* get the help flag */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_HELP);
    if (parm && parm->res == NO_ERR) {
        helpmode = TRUE;
    }

    /* help submode parameter (brief/normal/full) */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_BRIEF);
    if (parm) {
        helpsubmode = HELP_MODE_BRIEF;
    } else {
        /* full parameter */
        parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_FULL);
        if (parm) {
            helpsubmode = HELP_MODE_FULL;
        } else {
            helpsubmode = HELP_MODE_NORMAL;
        }
    }

    /* get indent param */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_INDENT);
    if (parm && parm->res == NO_ERR) {
        defindent = (int32)VAL_UINT(parm);
    }

    /* get message-indent param */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_MESSAGE_INDENT);
    if (parm && parm->res == NO_ERR) {
        msg_defindent = VAL_INT8(parm);
    }

    /* get the password parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_PASSWORD);
    if (parm && parm->res == NO_ERR) {
        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the --ncport parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_NCPORT);
    if (parm && parm->res == NO_ERR) {
        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the --private-key parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_PRIVATE_KEY);
    if (parm && parm->res == NO_ERR) {
        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the --public-key parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_PUBLIC_KEY);
    if (parm && parm->res == NO_ERR) {
        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the --transport parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_TRANSPORT);
    if (parm && parm->res == NO_ERR) {
        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the run-script parameter */
    runscript = get_strparm(mgr_cli_valset, YANGCLI_MOD, YANGCLI_RUN_SCRIPT);

    /* get the run-command parameter */
    runcommand = get_strparm(mgr_cli_valset, YANGCLI_MOD, YANGCLI_RUN_COMMAND);

    /* get the timeout parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
        default_timeout = VAL_UINT(parm);

        /* save to the connect_valset parmset */
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the user name */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_USER);
    if (parm && parm->res == NO_ERR) {
        res = add_clone_parm(parm, connect_valset);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* get the version parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_VERSION);
    if (parm && parm->res == NO_ERR) {
        versionmode = TRUE;
    }

    /* get the match-names parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_MATCH_NAMES);
    if (parm && parm->res == NO_ERR) {
        match_names = ncx_get_name_match_enum(VAL_ENUM_NAME(parm));
        if (match_names == NCX_MATCH_NONE) {
            return ERR_NCX_INVALID_VALUE;
        }
    }

    /* get the alt-names parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_ALT_NAMES);
    if (parm && parm->res == NO_ERR) {
        alt_names = VAL_BOOL(parm);
    }

    /* get the force-target parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_FORCE_TARGET);
    if (parm && parm->res == NO_ERR) {
        force_target = VAL_ENUM_NAME(parm);
    }

    /* get the use-xmlheader parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_USE_XMLHEADER);
    if (parm && parm->res == NO_ERR) {
        use_xmlheader = VAL_BOOL(parm);
    }

    /* get the config-edit-mode parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD,
                          YANGCLI_CONFIG_EDIT_MODE);
    if (parm && parm->res == NO_ERR) {
        config_edit_mode = cvt_config_edit_mode_str(VAL_ENUM_NAME(parm));
    }

    /* get the uservars-file parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, YANGCLI_USERVARS_FILE);
    if (parm && parm->res == NO_ERR) {
        uservars_file = VAL_STR(parm);
    }

    /* get the test-suite-file parameter */
    parm = val_find_child(mgr_cli_valset, YANGCLI_MOD, 
                          YANGCLI_TEST_SUITE_FILE);
    if (parm && parm->res == NO_ERR) {
        testsuite_file = VAL_STR(parm);
        testsuite_file_default = val_set_by_default(parm);
    }

    return NO_ERR;

} /* process_cli_input */


/********************************************************************
 * FUNCTION load_base_schema 
 * 
 * Load the following YANG modules:
 *   yangcli
 *   yuma-netconf
 *
 * RETURNS:
 *     status
 *********************************************************************/
static status_t
    load_base_schema (void)
{
    status_t   res;

    log_debug2("\nyangcli-pro: Loading YumaPro CLI Parameters");

    /* load in the server boot parameter definition file */
    res = ncxmod_load_module(YANGCLI_MOD, NULL, NULL, &yangcli_mod);
    if (res != NO_ERR) {
        return res;
    }

    /* load in the NETCONF data types and RPC methods */
    res = ncxmod_load_module(NCXMOD_NETCONF, NULL, NULL, &netconf_mod);
    if (res != NO_ERR) {
        return res;
    }

    /* load the netconf-state module to use
     * the <get-schema> operation 
     */
    res = ncxmod_load_module(NCXMOD_IETF_NETCONF_STATE, NULL, NULL, NULL);
    if (res != NO_ERR) {
        return res;
    }

    /* load the unit-test module to use
     * the <run-test-suite> operation 
     */
    res = ncxmod_load_module(YANGCLI_TEST_MOD, NULL, NULL, 
                             &test_suite_mod);
    if (res != NO_ERR) {
        return res;
    }

    /* load the notifications module to use
     * the <notification> container definition
     */
    res = ncxmod_load_module(NCN_MODULE, NULL, NULL, NULL);
    if (res != NO_ERR) {
        return res;
    }

    /* load the 2nd notifications module to use
     * the <replayComplete> and <notificationComplete>
     * event definition (TBD: not used yet!)
     */
    res = ncxmod_load_module(NCN2_MODULE, NULL, NULL, NULL);
    if (res != NO_ERR) {
        log_error("\nError: continuing init without finding "
                  "nc-notifications.yang\n");
    }

    return NO_ERR;

}  /* load_base_schema */


/********************************************************************
 * FUNCTION load_core_schema 
 * 
 * Load the following YANG modules:
 *   yuma-xsd
 *   yuma-types
 *
 * RETURNS:
 *     status
 *********************************************************************/
static status_t
    load_core_schema (void)
{
    status_t   res;

    log_debug2("\nNcxmgr: Loading NETCONF Module");

    /* load in the XSD data types */
    res = ncxmod_load_module(XSDMOD, NULL, NULL, NULL);
    if (res != NO_ERR) {
        return res;
    }

    /* load in the NCX data types */
    res = ncxmod_load_module(NCXDTMOD, NULL, NULL, NULL);
    if (res != NO_ERR) {
        return res;
    }

    return NO_ERR;

}  /* load_core_schema */


/********************************************************************
* FUNCTION check_module_capabilities
* 
* Check the modules reported by the server
* If autoload is TRUE then load any missing modules
* otherwise just warn which modules are missing
* Also check for wrong module module and version errors
*
* The server_cb->searchresultQ is filled with
* records for each module or deviation specified in
* the module capability URIs.
*
* After determining all the files that the server has,
* the <get-schema> operation is used (if :schema-retrieval
* advertised by the device and --autoload=true)
*
* All the files are copied into the session work directory
* to make sure the correct versions are used when compiling
* these files and applying features and deviations
*
* All files are compiled against the versions of the imports
* advertised in the capabilities, to make sure that imports
* without revision-date statements will still select the
* same revision as the server (INSTEAD OF ALWAYS SELECTING 
* THE LATEST VERSION).
*
* If the device advertises an incomplete set of modules,
* then searches for any missing imports will be done
* using the normal search path, including YUMA_MODPATH.
*
* INPUTS:
*  server_cb == server control block to use
*  session_cb == session control block to use
*  scb == session control block
*
*********************************************************************/
static void
    check_module_capabilities (server_cb_t *server_cb,
                               session_cb_t *session_cb,
                               ses_cb_t *scb)
{
    mgr_scb_t *mscb = (mgr_scb_t *)scb->mgrcb;
    status_t res = NO_ERR;

    log_debug("\n\nChecking Server Modules...\n");

    if (!cap_std_set(&mscb->caplist, CAP_STDID_V1)) {
        log_warn("\nWarning: NETCONF v1 capability not found");
    }

#ifdef DEBUG_MODLIBQ
    if (LOGDEBUG2) {
        dump_modlibQ(&modlibQ);
    }
#endif

    boolean retrieval_supported = cap_set(&mscb->caplist,
                                          CAP_SCHEMA_RETRIEVAL);

    if (!session_cb->autoload) {
        /* --autoload=false */
        if (LOGINFO) {
            log_info("\nAutoload: Skipping module capabilities, "
                     "autoload disabled");
        }
    } else {
        /* check all the YANG modules;
         * build a list of modules that
         * the server needs to get somehow
         * or proceed without them
         * save the results in the server_cb->searchresultQ
         */
        cap_rec_t *cap = cap_first_modcap(&mscb->caplist);
        while (cap) {
            const xmlChar *module = NULL;
            const xmlChar *revision = NULL;
            const xmlChar *namespacestr = NULL;
            ncxmod_search_result_t *libresult = NULL;
            ncxmod_search_result_t *searchresult = NULL;

            /* get the parsed fields out of the cap URI string */
            cap_split_modcap(cap, &module, &revision, &namespacestr);

            if (namespacestr == NULL) {
                /* try the entire base part of the URI if there was
                 * no module capability parsed
                 */
                namespacestr = cap->cap_uri;
            }

            if (namespacestr == NULL) {
                if (ncx_warning_enabled(ERR_NCX_RCV_INVALID_MODCAP)) {
                    log_warn("\nWarning: skipping enterprise capability "
                             "for URI '%s'", cap->cap_uri);
                }
                cap = cap_next_modcap(cap);
                continue;
            }

            /* check if there is a module in the modlibQ that
             * matches this capability URI */
            libresult = ncxmod_find_search_result(&modlibQ, module,
                                                  revision, namespacestr);
            if (libresult) {
                searchresult = ncxmod_clone_search_result(libresult);
                if (searchresult == NULL) {
                    log_error("\nError: cannot load file, "
                              "malloc failed\n");
                    cap = cap_next_modcap(cap);
                    continue;
                }

                /* can use the local system file found */
                searchresult->capmatch = TRUE;
                searchresult->cap = cap;
                dlq_enque(searchresult, &session_cb->searchresultQ);
                cap = cap_next_modcap(cap);
                continue;
            }

            /* module was not found in the module search path
             * of this instance of yangcli
             * try to auto-load the module if enabled
             */
            if (module == NULL) {
                if (LOGINFO) {
                    log_info("\nAutoload skipping cap; No module name"
                             "found for ns '%s'\n", cap->cap_uri);
                }
            } else if (!retrieval_supported) {
                /* libresult was NULL, so there was no searchresult
                 * ncxmod did not find any YANG module with this namespace
                 * and module, revision values;
                 * no <get-schema> so SOL, do without this module 
                 * !!! need warning number 
                 */
                if (revision != NULL) {
                    log_warn("\nWarning: module '%s' "
                             "revision '%s' not available",
                             module, revision);
                } else {
                    log_warn("\nWarning: module '%s' "
                             "(no revision) not available", module);
                }
            } else {
                /* setup a blank searchresult so auto-load
                 * will attempt to retrieve it
                 */
                searchresult = ncxmod_new_search_result_str(module, revision);
                if (searchresult) {
                    searchresult->cap = cap;
                    dlq_enque(searchresult, &session_cb->searchresultQ);
                    if (LOGDEBUG) {
                        log_debug("\nyangcli_autoload: Module '%s' "
                                  "not available, will try "
                                  "<get-schema>", module);
                    }
                } else {
                    log_error("\nError: cannot load module '%s', "
                              "malloc failed\n", module);
                }
            }

            /* move on to the next module */
            cap = cap_next_modcap(cap);
        }  // while (cap)
    }

    /* get all the advertised YANG data model modules into the
     * session temp work directory that are local to the system
     */
    res = autoload_setup_tempdir(server_cb, session_cb, scb);
    if (res != NO_ERR) {
        log_error("\nError: autoload setup temp files failed (%s)\n",
                  get_error_string(res));
    }

    /* go through all the search results (if any)
     * and see if <get-schema> is needed to pre-load
     * the session work directory YANG files
     */
    if (res == NO_ERR && retrieval_supported && session_cb->autoload) {
        /* compile phase will be delayed until autoload
         * get-schema operations are done
         */
        res = autoload_start_get_modules(session_cb);
        if (res != NO_ERR) {
            log_error("\nError: autoload get modules failed (%s)\n",
                      get_error_string(res));
        }
    }

    /* check autoload state did not start or was not requested */
    if (res == NO_ERR && session_cb->command_mode != CMD_MODE_AUTOLOAD) {
        /* parse and hold the modules with the correct deviations,
         * features and revisions.  The returned modules
         * from yang_parse.c will not be stored in the local module
         * directory -- just used for this one session then deleted
         */
        res = autoload_compile_modules(server_cb, session_cb, scb);
        if (res != NO_ERR) {
            log_error("\nError: autoload compile modules failed (%s)\n",
                      get_error_string(res));
        }
    }

} /* check_module_capabilities */


/********************************************************************
* message_timed_out
*
* Check if the request in progress (!) has timed out
* TBD: check for a specific message if N requests per
* session are ever supported
*
* INPUTS:
*   scb == session control block to use
*
* RETURNS:
*   TRUE if any messages have timed out
*   FALSE if no messages have timed out
*********************************************************************/
static boolean
    message_timed_out (ses_cb_t *scb)
{
    mgr_scb_t    *mscb;
    uint32        deletecount;

    mscb = (mgr_scb_t *)scb->mgrcb;

    if (mscb) {
        deletecount = mgr_rpc_timeout_requestQ(&mscb->reqQ);
        if (deletecount) {
            log_error("\nError: request to server timed out\n");
        }
        return (deletecount) ? TRUE : FALSE;
    }

    /* else mgr_shutdown could have been issued via control-C
     * and the session control block has been deleted
     */
    return FALSE;

}  /* message_timed_out */


/********************************************************************
* FUNCTION get_lock_worked
* 
* Check if the get-locks function ended up with
* all its locks or not
* 
* INPUTS:
*  session_cb == session control block to use
*
*********************************************************************/
static boolean
    get_lock_worked (session_cb_t *session_cb)
{
    ncx_cfg_t  cfgid;

    for (cfgid = NCX_CFGID_RUNNING;
         cfgid <= NCX_CFGID_STARTUP;
         cfgid++) {
        if (session_cb->lock_cb[cfgid].lock_used &&
            session_cb->lock_cb[cfgid].lock_state !=
            LOCK_STATE_ACTIVE) {
            return FALSE;
        }
    }

    return TRUE;

}  /* get_lock_worked */


/********************************************************************
* FUNCTION get_input_line
 *
 * get a line of input text from the appropriate source
 *
 * INPUTS:
 *   server_sb == server control block to use
 *   autotestmode == TRUE if autotest mode; FALSE for normal mode
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status
 *
 * RETURNS:
 *    pointer to the line to use; do not free this memory
*********************************************************************/
static xmlChar *
    get_input_line (server_cb_t *server_cb,
                    boolean autotestmode,
                    status_t *res)
{
    /* get a line of user input
     * this is really a const pointer
     */
    *res = NO_ERR;

    xmlChar *line = NULL;
    if (autotestmode) {
        line = yangcli_ut_getline(server_cb, res);
        if (*res != ERR_NCX_SKIPPED) {
            log_debug("\nAutotest input: %s", line);
            return line;
        } else {
            *res = NO_ERR;
        }
    }

    boolean done = FALSE;
    while (!done && *res == NO_ERR) {
        /* figure out where to get the next input line */
        runstack_src_t  rsrc = 
            runstack_get_source(server_cb->runstack_context);

        switch (rsrc) {
        case RUNSTACK_SRC_NONE:
            *res = SET_ERROR(ERR_INTERNAL_VAL);
            break;
        case RUNSTACK_SRC_USER:
            /* block until user enters some input */
            line = get_cmd_line(server_cb, res);
            break;
        case RUNSTACK_SRC_SCRIPT:
            /* get one line of script text */
            line = runstack_get_cmd(server_cb->runstack_context, res);
            break;
        case RUNSTACK_SRC_LOOP:
            /* get one line of script text */
            line = runstack_get_loop_cmd(server_cb->runstack_context,
                                         res);
            if (line == NULL || *res != NO_ERR) {
                if (*res == ERR_NCX_LOOP_ENDED) {
                    /* did not get a loop line, try again */
                    *res = NO_ERR;
                    continue;
                }
                return NULL;
            }
            break;
        default:
            *res = SET_ERROR(ERR_INTERNAL_VAL);
        }

        if (*res == NO_ERR) {
            *res = runstack_save_line(server_cb->runstack_context,
                                      line);
        }
        /* only 1 time through loop in normal conditions */
        done = TRUE;
    }
    return line;

}  /* get_input_line */


/********************************************************************
 * FUNCTION do_bang_recall (local !string)
 * 
 * Do Command line history support operations
 *
 * !sget-config target=running
 * !42
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    line == CLI input in progress
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_bang_recall (server_cb_t *server_cb,
                    const xmlChar *line)
{
    status_t            res;

    res = NO_ERR;

    if (line && 
        *line == YANGCLI_RECALL_CHAR && 
        line[1] != 0) {
        if (isdigit(line[1])) {
            /* assume the form is !42 to recall by line number */
            ncx_num_t num;
            ncx_init_num(&num);
            res = ncx_decode_num(&line[1], NCX_BT_UINT64, &num);
            if (res != NO_ERR) {
                log_error("\nError: invalid number '%s'\n", 
                          get_error_string(res));
            } else {
                res = do_line_recall(server_cb, num.ul);
            }
            ncx_clean_num(NCX_BT_UINT64, &num);
        } else {
            /* assume form is !command string and recall by
             * most recent match of the partial command line given
             */
            res = do_line_recall_string(server_cb, &line[1]);
        }
    } else {
        res = ERR_NCX_MISSING_PARM;
        log_error("\nError: missing recall string\n");
    }

    return res;

}  /* do_bang_recall */

/********************************************************************
* yangcli_session_closed_handler
*
* INPUT: agtsid
* RETURNS: void
*********************************************************************/
static void
    yangcli_session_closed_handler (uint32 agtsid)
{
    server_cb_t *server_cb = cur_server_cb;
    session_cb_t *ses_cb =
         (session_cb_t *)dlq_firstEntry(&server_cb->session_cbQ);

    for (; ses_cb != NULL;
         ses_cb = (session_cb_t *)dlq_nextEntry(ses_cb)) {
         if (ses_cb->mysid == agtsid ) {
             clear_server_cb_session(server_cb, ses_cb);
             break;
         }
    }
} /* yangcli_session_closed_handler */


/********************************************************************
* yangcli_stdin_handler
*
* Temp: Calling readline which will block other IO while the user
*       is editing the line.  This is okay for this CLI application
*       but not multi-session applications;
* Need to prevent replies from popping up on the screen
* while new commands are being edited anyway
*
* RETURNS:
*   new program state
*********************************************************************/
static mgr_io_state_t
    yangcli_stdin_handler (void)
{
    /* TBD: support more than 1 application server */
    server_cb_t *server_cb = cur_server_cb;

    set_cur_record_step_done (server_cb, FALSE);

    /* the current session can change during commands invokded during
     * this function.  Be very careful about assuming the session_cb
     * pointer is still valid.  Note that the current session is recheked
     * after operations are performed
     */
    session_cb_t *session_cb = server_cb->cur_session_cb;

    if (session_cb->returncode == MGR_IO_RC_WANTDATA) {
        /* this keep-alive is not actually used */
        if (LOGDEBUG2) {
            log_debug2("\nyangcli-pro: sending dummy <get> keepalive");
        }
        (void)send_keepalive_get(server_cb);
        session_cb->returncode = MGR_IO_RC_NONE;        
    }

    if (session_cb->command_mode == CMD_MODE_NORMAL) {
        if (session_cb->config_mode) {
            /* make sure help key handler is registered */
            init_config_completion_state(&server_cb->completion_state);
        } else if (server_cb->cli_fn == NULL && !server_cb->climore) {
            /* init the tab completion state except when command in progress */
            init_completion_state(&server_cb->completion_state,
                                  server_cb, CMD_STATE_FULL);
        }
    }

    if (mgr_shutdown_requested()) {
        session_cb->state = MGR_IO_ST_SHUT;
    }

    status_t res = NO_ERR;
    ses_cb_t *scb = NULL;
    mgr_scb_t *mscb = NULL;

    switch (session_cb->state) {
    case MGR_IO_ST_INIT:
        return session_cb->state;
    case MGR_IO_ST_IDLE:
        break;
    case MGR_IO_ST_CONN_IDLE:
        /* check if session was dropped by remote peer */
        scb = mgr_ses_get_scb(session_cb->mysid);
        if (scb==NULL || scb->state == SES_ST_SHUTDOWN_REQ) {
            if (scb) {
                (void)mgr_ses_free_session(session_cb->mysid);
            }
            clear_server_cb_session(server_cb, session_cb);
            if (program_mode == PROG_MODE_SERVER) {
                mgr_request_shutdown();
            }
        } else  {
            res = NO_ERR;
            mscb = (mgr_scb_t *)scb->mgrcb;
            ncx_set_temp_modQ(&mscb->temp_modQ);

            /* check locks timeout */
            if (session_cb->command_mode == CMD_MODE_AUTOLOCK ||
                session_cb->command_mode == CMD_MODE_AUTOUNLOCK ||
                session_cb->command_mode == CMD_MODE_AUTODISCARD) {

                if (check_locks_timeout(session_cb)) {
                    res = ERR_NCX_TIMEOUT;

                    if (runstack_level(server_cb->runstack_context)) {
                        runstack_cancel(server_cb->runstack_context);
                    }

                    switch (session_cb->command_mode) {
                    case CMD_MODE_AUTODISCARD:
                    case CMD_MODE_AUTOLOCK:
                        handle_locks_cleanup(server_cb, session_cb);
                        if (session_cb->command_mode != CMD_MODE_NORMAL) {
                            return session_cb->state;
                        }
                        break;
                    case CMD_MODE_AUTOUNLOCK:
                        clear_lock_cbs(session_cb);
                        break;
                    default:
                        SET_ERROR(ERR_INTERNAL_VAL);
                    }
                } else if (session_cb->command_mode == CMD_MODE_AUTOLOCK) {
                    if (session_cb->locks_waiting) {
                        session_cb->command_mode = CMD_MODE_AUTOLOCK;
                        boolean done = FALSE;
                        res = handle_get_locks_request_to_server(server_cb,
                                                                 session_cb,
                                                                 FALSE, &done);
                        if (done) {
                            /* check if the locks are all good */
                            if (get_lock_worked(session_cb)) {
                                log_info("\nget-locks finished OK");
                                session_cb->command_mode = CMD_MODE_NORMAL;
                                session_cb->locks_waiting = FALSE;
                            } else {
                                log_error("\nError: get-locks failed, "
                                          "starting cleanup\n");
                                handle_locks_cleanup(server_cb, session_cb);
                            }
                        }
                    }
                }
            }
        }
        break;
    case MGR_IO_ST_CONN_START:
        /* waiting until <hello> processing complete */
        scb = mgr_ses_get_scb(session_cb->mysid);
        if (scb == NULL) {
            /* session startup failed */
            session_cb->state = MGR_IO_ST_IDLE;
            if (batchmode) {
                mgr_request_shutdown();
                return session_cb->state;
            }
        } else if (scb->state == SES_ST_IDLE && dlq_empty(&scb->outQ)) {
            /* incoming hello OK and outgoing hello is sent */
            session_cb->state = MGR_IO_ST_CONN_IDLE;
            report_capabilities(server_cb, session_cb, scb, TRUE, 
                                HELP_MODE_NONE);
            check_module_capabilities(server_cb, session_cb, scb);
            mscb = (mgr_scb_t *)scb->mgrcb;
            ncx_set_temp_modQ(&mscb->temp_modQ);

            /* Check if connect_all sessions in progress */
            check_connect_all_sessions (server_cb);

        } else {
            /* check timeout */
            if (message_timed_out(scb)) {
                session_cb->state = MGR_IO_ST_IDLE;
                if (batchmode) {
                    mgr_request_shutdown();
                    return session_cb->state;
                }
                break;
            } /* else still setting up session */
            return session_cb->state;
        }
        break;
    case MGR_IO_ST_CONN_RPYWAIT:
        /* check if session was dropped by remote peer */
        scb = mgr_ses_get_scb(session_cb->mysid);
        if (scb==NULL || scb->state == SES_ST_SHUTDOWN_REQ) {
            if (scb) {
                (void)mgr_ses_free_session(session_cb->mysid);
            }
            clear_server_cb_session(server_cb, session_cb);
            if (program_mode == PROG_MODE_SERVER) {
                mgr_request_shutdown();
            }
        } else  {
            /* session is active; init state check command mode */
            res = NO_ERR;
            mscb = (mgr_scb_t *)scb->mgrcb;
            ncx_set_temp_modQ(&mscb->temp_modQ);

            /* check timeout */
            if (message_timed_out(scb)) {
                res = ERR_NCX_TIMEOUT;
            } else if (session_cb->command_mode == CMD_MODE_AUTOLOCK ||
                session_cb->command_mode == CMD_MODE_AUTOUNLOCK ||
                session_cb->command_mode == CMD_MODE_AUTODISCARD) {
                if (check_locks_timeout(session_cb)) {
                    res = ERR_NCX_TIMEOUT;
                }
            }

            if (res != NO_ERR) {
                if (runstack_level(server_cb->runstack_context)) {
                    runstack_cancel(server_cb->runstack_context);
                }
                session_cb->state = MGR_IO_ST_CONN_IDLE;

                switch (session_cb->command_mode) {
                case CMD_MODE_NORMAL:
                    break;
                case CMD_MODE_AUTOLOAD:
                    /* even though a timeout occurred
                     * attempt to compile the modules
                     * that are already present
                     */
                    autoload_compile_modules(server_cb, session_cb, scb);
                    break;
                case CMD_MODE_AUTODISCARD:
                    session_cb->command_mode = CMD_MODE_AUTOLOCK;
                    /* fall through */
                case CMD_MODE_AUTOLOCK:
                    handle_locks_cleanup(server_cb, session_cb);
                    if (session_cb->command_mode != CMD_MODE_NORMAL) {
                        return session_cb->state;
                    }
                    break;
                case CMD_MODE_AUTOUNLOCK:
                    clear_lock_cbs(session_cb);
                    break;
                case CMD_MODE_AUTOCONFIG:
                    log_error("\nError: autoconfig <get-config> failed\n");
                    session_cb->command_mode = CMD_MODE_NORMAL;
                    break;
                case CMD_MODE_AUTONOTIF:
                    log_error("\nError: autonotif <create-subscription> "
                              "failed\n");
                    session_cb->command_mode = CMD_MODE_NORMAL;
                    break;
                case CMD_MODE_AUTOTEST:
                    log_error("\nError: auto-test mode <edit-config> failed\n");
                    autotest_cancel(session_cb);
                    break;
                case CMD_MODE_CONF_APPLY:
                    log_error("\nError: config mode <edit-config> failed\n");
                    session_cb->command_mode = CMD_MODE_NORMAL;
                    break;
                case CMD_MODE_SAVE:
                    session_cb->command_mode = CMD_MODE_NORMAL;
                    break;
                default:
                    SET_ERROR(ERR_INTERNAL_VAL);
                }
            } else {
                /* keep waiting for reply */
                return session_cb->state;
            }
        }
        break;
    case MGR_IO_ST_CONNECT:
    case MGR_IO_ST_SHUT:
    case MGR_IO_ST_CONN_CANCELWAIT:
    case MGR_IO_ST_CONN_SHUT:
    case MGR_IO_ST_CONN_CLOSEWAIT:
        /* check timeout */
        scb = mgr_ses_get_scb(session_cb->mysid);
        if (scb==NULL || scb->state == SES_ST_SHUTDOWN_REQ) {
            if (scb) {
                (void)mgr_ses_free_session(session_cb->mysid);
            }
            clear_server_cb_session(server_cb, session_cb);
        } else if (message_timed_out(scb)) {
            clear_server_cb_session(server_cb, session_cb);
        }

        /* do not accept chars in these states */
        return session_cb->state;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return session_cb->state;
    }

    /* check if some sort of auto-mode, and the
     * CLI is skipped until that mode is done.
     */
    if (session_cb->command_mode != CMD_MODE_NORMAL) {
        return session_cb->state;
    } else if (!batchmode && !session_cb->autoconfig_done &&
               session_cb->state == MGR_IO_ST_CONN_IDLE) {
        /* want to stop here as soon as a session has finished
         * all its <hello> and <get-schema> messages, except in
         * batchmode which will not use the config_tree at all   */
        session_cb->autoconfig_done = TRUE;
        if (session_cb->autoconfig) {
            res = start_autoconfig_retrieval(server_cb, session_cb);
            if (res != NO_ERR) {
                log_error("\nError: start autoconfig mode failed, "
                          "instance tab completion disabled for this "
                          "session\n");
                res = NO_ERR;
            } else {
                return session_cb->state;
            }
        }
    } else if (!batchmode && !session_cb->autonotif_done &&
               session_cb->state == MGR_IO_ST_CONN_IDLE) {
        /* want to stop here as soon as a session has finished
         * all its <hello>, <get-schema> and autoconfig messages,
         * except in batchmode which will not use notifications    */
        session_cb->autonotif_done = TRUE;
        if (session_cb->autonotif) {
            // set stream to monitor; only default NETCONF supported now
            const xmlChar *stream = NULL;

            res = start_autonotif_mode(server_cb, session_cb, stream);
            if (res == ERR_NCX_SKIPPED) {
                /* notifications not supported, keep going */
                res = NO_ERR;
            } else if (res != NO_ERR) {
                /* some other error starting notifications, keep going */
                log_error("\nError: start autonotif mode failed, "
                          "Shadow config update disabled for this session\n");
                res = NO_ERR;
            } else {
                /* started AUTONOTIF mode OK */
                return session_cb->state;
            }
        }
    }

    /* check if autoconfig is active and the config is dirty */
    if (session_cb->autoconfig && session_cb->config_tree_dirty) {
        res = start_update_config(server_cb, session_cb, NCX_CFGID_RUNNING);
        if (res == NO_ERR) {
            return session_cb->state;
        }
        res = NO_ERR;
    }

    /* if autotest is active then get the input from the
     * test-suite config; 
     */
    boolean isautotest = yangcli_ut_active(server_cb);

    /* check the run-script parameters */
    if (runscript) {
        if (!runscriptdone) {
            runscriptdone = TRUE;
            (void)do_startup_script(server_cb, runscript);
            /* drop through and get input line from runstack */
            session_cb = server_cb->cur_session_cb;
        }
    } else if (runcommand) {
        if (!runcommanddone) {
            runcommanddone = TRUE;
            (void)do_startup_command(server_cb, runcommand);
            /* exit now in case there was a session started up and a remote
             * command sent as part of run-command.  
             * May need to let write_sessions() run in mgr_io.c.
             * If not, 2nd loop through this fn will hit get_input_line
             */
            session_cb = server_cb->cur_session_cb;
            return session_cb->state;            
        } else if (batchmode && !isautotest) {
            /* run command is done at this point */
            mgr_request_shutdown();
            session_cb = server_cb->cur_session_cb;
            return session_cb->state;
        }
    }

    /* get a line of user input
     * 'line' is really a const pointer
     */
    res = NO_ERR;

    xmlChar *line = NULL;
    if (session_cb->autoconfig_saveline) {
        line = session_cb->autoconfig_saveline;
        session_cb->autoconfig_saveline = NULL;
    } else {
        line = get_input_line(server_cb, isautotest, &res);
    }

    /* need to get the current session over here because get_prompt
     * may have changed it to the default session
     * !!!!! NEED TO CHECK THIS -- SHOULD REMOVE !!!!
     */
    if (session_cb != server_cb->cur_session_cb) {
        log_debug3("\nChanging current session to server_cb current!");
        session_cb = server_cb->cur_session_cb;
    }

    /* need to check if the current session was dropped while waiting
     * for input
     */
    scb = mgr_ses_get_scb(session_cb->mysid);
    if (!scb) {
        clean_session_cb_conn(session_cb);
        if (server_cb->program_mode == PROG_MODE_SERVER) {
            mgr_request_shutdown();
        }
    }

    /* check config_tree dirty while waiting for command line input */
    if (scb && session_cb->autoconfig && session_cb->config_tree_dirty) {
        if (LOGDEBUG) {
            log_debug("\nConfig has changed on server; "
                      "Retrieving new config now!\n");
        }

        /* update the config cahce right now */
        res = start_update_config(server_cb, session_cb, NCX_CFGID_RUNNING);
        if (res == NO_ERR) {
            session_cb->autoconfig_saveline = line;
            return session_cb->state;
        } else {
            log_error("\nError: cannot start autoconfig update!"
                      "\nConfig cache is out of date on server!\n");
        }
    }

    /* may need to invoke commands in a different session
     * than the current session for autotest mode
     *
     * !!! THIS IS NOT WORKING 100% BECAUSE WE DO NOT WANT
     * !!! TO USE THE CUR_SESSION BEFORE get_prompt
     * !!! IF THAT SESSION FAILED TO START
     * !!! TARGET WILL ALWAYS EQUAL CURRENT FOR NOW
     */
    session_cb_t *target_session_cb = session_cb;

    if (isautotest) {
        const xmlChar *target_name = 
            yangcli_ut_get_target_session(server_cb);
        if (target_name) {
            target_session_cb = find_session_cb(server_cb, target_name);
            if (target_session_cb == NULL) {
                const xmlChar *msg = 
                    (const xmlChar *)"test-suite session not found";
                /* session has died or some other error */
                yangcli_ut_stop(server_cb, ERR_NCX_NOT_FOUND, msg);
                return session_cb->state;            
            }
        } // else NULL == use current session
    }

    /* figure out how to process the input line
     * based on the input source;
     */
    runstack_src_t rsrc = runstack_get_source(server_cb->runstack_context);

    runstack_clear_cancel(server_cb->runstack_context);

    /* check exit conditions for batchmode, where 1 script is
     * executed in non-interactive mode and the program exits
     * when the script is done.
     */
    switch (rsrc) {
    case RUNSTACK_SRC_NONE:
        res = SET_ERROR(ERR_INTERNAL_VAL);
        break;
    case RUNSTACK_SRC_USER:
        if (line == NULL) {
            /* could have just transitioned from script mode to
             * user mode; check batch-mode exit;
             */
            if (batchmode) {
                mgr_request_shutdown();
            }
            if (isautotest) {
                yangcli_ut_stop(server_cb, res, NULL);
            }
            return session_cb->state;
        }
        break;
    case RUNSTACK_SRC_SCRIPT:
    case RUNSTACK_SRC_LOOP:
        if (line == NULL || res != NO_ERR) {
            if (batchmode && res != NO_ERR) {
                mgr_request_shutdown();
            }
            if (isautotest) {
                yangcli_ut_stop(server_cb, res, NULL);
            }
            return session_cb->state;
        }
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        if (isautotest) {
            yangcli_ut_stop(server_cb, res, NULL);
        }
        return session_cb->state;
    }        

    /* check bang recall statement */
    if (*line == YANGCLI_RECALL_CHAR) {
        res = do_bang_recall(server_cb, line);
        if (!isautotest) {
            return session_cb->state;
        } else if (res != NO_ERR) {
            /* autotest mode error exit */
            return session_cb->state;
        } // else use recalled line as-is for autotest mode
    }

    /* starting here, the target session needs to be used */
    session_cb_t *save_session_cb = NULL;
    if (target_session_cb != session_cb) {
        save_session_cb = session_cb;
        set_cur_session_cb(server_cb, target_session_cb);
        session_cb = target_session_cb;
    }

    /* check if this is an assignment statement */
    boolean getrpc = FALSE;
    boolean fileassign = FALSE;
    uint32 len = 0;
    res = check_assign_statement(server_cb, session_cb, line, 
                                 &len, &getrpc, &fileassign);
    if (res != NO_ERR) {
        if (!session_cb->config_mode) {
            log_error("\nyangcli-pro: Variable assignment failed (%s) (%s)",
                      line, get_error_string(res));

            if (isautotest) {
                yangcli_ut_handle_return(server_cb, res);
            }

            if (runstack_level(server_cb->runstack_context)) {
                runstack_cancel(server_cb->runstack_context);
            }

            switch (session_cb->command_mode) {
            case CMD_MODE_AUTODISCARD:
            case CMD_MODE_AUTOLOCK:
                handle_locks_cleanup(server_cb, session_cb);
                break;
            case CMD_MODE_AUTOUNLOCK:
                clear_lock_cbs(session_cb);
                break;
            case CMD_MODE_AUTOLOAD:
            case CMD_MODE_AUTOCONFIG:
            case CMD_MODE_AUTONOTIF:
            case CMD_MODE_NORMAL:
            case CMD_MODE_SAVE:
                break;
            case CMD_MODE_AUTOTEST:
                autotest_cancel(session_cb);
                break;
            default:
                SET_ERROR(ERR_INTERNAL_VAL);
            }
        } // else config-mode error already printed
    } else if (getrpc) {
        res = NO_ERR;
        switch (session_cb->state) {
        case MGR_IO_ST_IDLE:
            /* waiting for top-level commands */
            res = top_command(server_cb, &line[len], FALSE);
            break;
        case MGR_IO_ST_CONN_IDLE:
            if (session_cb->config_mode) {
                /* waiting for a config node name to edit */
                res = handle_config_input(server_cb, session_cb, &line[len]);
            } else {
                /* waiting for session commands */
                res = conn_command(server_cb, &line[len], FALSE, FALSE);
                if (res == ERR_NCX_SKIPPED &&
                    server_cb->program_mode == PROG_MODE_SERVER) {
                    log_error("\nError: unknown command\n");
                    res = ERR_NCX_INVALID_VALUE;
                }
            }
            break;
        case MGR_IO_ST_CONN_RPYWAIT:
            /* waiting for RPC reply while more input typed */
            break;
        case MGR_IO_ST_CONN_CANCELWAIT:
            break;
        default:
            break;
        }

        if (isautotest) {
            yangcli_ut_handle_return(server_cb, res);
        }

        if (res != NO_ERR) {
            if (runstack_level(server_cb->runstack_context)) {
                runstack_cancel(server_cb->runstack_context);
            }
        }

        /* the current session could have switched!!
         * TBD: determine if finish_result_assign needs
         * old session vars or new session vars
         */
        session_cb = server_cb->cur_session_cb;

        switch (session_cb->state) {
        case MGR_IO_ST_IDLE:
        case MGR_IO_ST_CONN_IDLE:
            /* check assignment statement active */
            if (!session_cb->config_mode &&
                (server_cb->result_name != NULL || 
                 server_cb->result_filename != NULL)) {

                if (server_cb->local_result != NULL) {
                    val_value_t  *resval = server_cb->local_result;
                    server_cb->local_result = NULL;
                    /* pass off malloced local_result here */
                    res = finish_result_assign(server_cb, session_cb,
                                               resval, NULL);
                    /* clear_result already called */
                } else {
                    /* save the filled in value */
                    const xmlChar *resultstr = (res == NO_ERR) ? 
                        (const xmlChar *)"ok" :
                        (const xmlChar *)get_error_string(res);
                    res = finish_result_assign(server_cb, session_cb,
                                               NULL, resultstr);
                    /* clear_result already called */
                }
            } else {
                clear_result(server_cb);
            }
            break;
        default:
            ;
        }
    } else {
        log_info("\nOK\n");
    }

    if (save_session_cb) {
        session_cb = save_session_cb;
    } else {
        session_cb = server_cb->cur_session_cb;
    }
    return session_cb->state;

} /* yangcli_stdin_handler */


/********************************************************************
 * FUNCTION get_rpc_error_tag
 * 
 *  Determine why the RPC operation failed
 *
 * INPUTS:
 *   replyval == <rpc-reply> to use to look for <rpc-error>s
 *
 * RETURNS:
 *   the RPC error code for the <rpc-error> that was found
 *********************************************************************/
static rpc_err_t
    get_rpc_error_tag (val_value_t *replyval)
{
    val_value_t *errval = 
        val_find_child(replyval, NC_MODULE, NCX_EL_RPC_ERROR);
    if (errval == NULL) {
        log_error("\nError: No <rpc-error> elements found\n");
        return RPC_ERR_NONE;
    }

    val_value_t *tagval = 
        val_find_child(errval, NC_MODULE, NCX_EL_ERROR_TAG);
    if (tagval == NULL) {
        log_error("\nError: <rpc-error> did not contain an <error-tag>\n");
        return RPC_ERR_NONE;
    }

    return rpc_err_get_errtag_enum(VAL_ENUM_NAME(tagval));

}  /* get_rpc_error_tag */


/********************************************************************
 * FUNCTION handle_rpc_timing
 * 
 * Get the roundtrip time and print the roundtrip time
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   req == original message request
 *
 *********************************************************************/
static void
    handle_rpc_timing (server_cb_t *server_cb,
                       mgr_rpc_req_t *req)
{
    session_cb_t *session_cb = server_cb->cur_session_cb;
    struct timeval      now;
    long int            sec, usec;
    xmlChar             numbuff[NCX_MAX_NUMLEN];

    if (!session_cb->time_rpcs) {
        return;
    }

    gettimeofday(&now, NULL);

    /* get the resulting delta */
    if (now.tv_usec < req->perfstarttime.tv_usec) {
        now.tv_usec += 1000000;
        now.tv_sec--;
    }

    sec = now.tv_sec - req->perfstarttime.tv_sec;
    usec = now.tv_usec - req->perfstarttime.tv_usec;

    sprintf((char *)numbuff, "%ld.%06ld", sec, usec);

    if (session_cb->time_rpcs_stats && session_cb->time_rpcs_fp) {
        /* send a line to the output stats file */
        sprintf((char *)numbuff, "%ld.%06ld\n", sec, usec);
        int ret = fputs((char *)numbuff, session_cb->time_rpcs_fp);
        if (ret == EOF) {
            log_error("\nError: write to rpc stats file failed\n");
        }
    } else if (interactive_mode()) {
        log_stdout("\nRoundtrip time: %s seconds\n", numbuff);
        if (log_get_logfile() != NULL) {
            log_write("\nRoundtrip time: %s seconds\n", numbuff);
        }
    } else {
        log_write("\nRoundtrip time: %s seconds\n", numbuff);
    }


}  /* handle_rpc_timing */


/********************************************************************
 * FUNCTION print_server_errormsg
 * 
 * Print all <rpc-error> elements in a simple format
 *
 * INPUTS:
 *   rpymsg == reply message with errors
 *
 *********************************************************************/
static void
    print_server_errormsg (val_value_t *rpymsg)
{
    val_value_t *errval =
        val_find_child(rpymsg, NC_MODULE, NCX_EL_RPC_ERROR);
    for (; errval; errval = val_next_child_match(rpymsg, errval, errval)) {

        val_value_t *errtag =
            val_find_child(errval, NC_MODULE, NCX_EL_ERROR_TAG);
        if (errtag == NULL) {
            log_error("\nError: operation failed\n");
            return;
        }

        val_value_t *errapptag =
            val_find_child(errval, NC_MODULE, NCX_EL_ERROR_APP_TAG);

        val_value_t *errmsg =
            val_find_child(errval, NC_MODULE, NCX_EL_ERROR_MESSAGE);

        val_value_t *errinfo =
            val_find_child(errval, NC_MODULE, NCX_EL_ERROR_INFO);

        val_value_t *errnum = NULL;
        if (errinfo) {
            errnum = val_find_child(errinfo, NULL, NCX_EL_ERROR_NUMBER);
        }

        log_error("\nError: %s", VAL_ENUM_NAME(errtag));
        if (errapptag) {
            log_error_append(" [%s]", VAL_STR(errapptag));
        }
        if (errmsg) {
            log_error_append(" - %s", VAL_STR(errmsg));
        }
        if (errnum) {
            log_error_append(" (%u)", VAL_UINT(errmsg));
        }
        log_error_append("\n");
    }

}  /* print_server_errormsg */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION get_autocomp
* 
*  Get the autocomp parameter value
* 
* RETURNS:
*    autocomp boolean value
*********************************************************************/
boolean
    get_autocomp (void)
{
    return autocomp;
}  /* get_autocomp */


/********************************************************************
* FUNCTION get_autoload
* 
*  Get the autoload parameter value
* 
* RETURNS:
*    autoload boolean value
*********************************************************************/
boolean
    get_autoload (void)
{
    return autoload;
}  /* get_autoload */


/********************************************************************
* FUNCTION get_batchmode
* 
*  Get the batchmode parameter value
* 
* RETURNS:
*    batchmode boolean value
*********************************************************************/
boolean
    get_batchmode (void)
{
    return batchmode;
}  /* get_batchmode */


/********************************************************************
* FUNCTION get_default_module
* 
*  Get the default module
* 
* RETURNS:
*    default module value
*********************************************************************/
const xmlChar *
    get_default_module (void)
{
    return default_module;
}  /* get_default_module */


/********************************************************************
* FUNCTION get_runscript
* 
*  Get the runscript variable
* 
* RETURNS:
*    runscript value
*********************************************************************/
const xmlChar *
    get_runscript (void)
{
    return runscript;

}  /* get_runscript */


/********************************************************************
* FUNCTION get_baddata
* 
*  Get the baddata parameter
* 
* RETURNS:
*    baddata enum value
*********************************************************************/
ncx_bad_data_t
    get_baddata (void)
{
    return baddata;
}  /* get_baddata */


/********************************************************************
* FUNCTION get_defindent
* 
*  Get the default indentation 'defindent parameter'
* 
* RETURNS:
*    defindent value
*********************************************************************/
int32
    get_defindent (void)
{
    return defindent;
}  /* get_defindent */


/********************************************************************
* FUNCTION get_yangcli_mod
* 
*  Get the yangcli module
* 
* RETURNS:
*    yangcli module
*********************************************************************/
ncx_module_t *
    get_yangcli_mod (void)
{
    return yangcli_mod;
}  /* get_yangcli_mod */


/********************************************************************
* FUNCTION get_unit_test_mod
* 
*  Get the unit-test module
* 
* RETURNS:
*    yangcli module
*********************************************************************/
ncx_module_t *
    get_unit_test_mod (void)
{
    return test_suite_mod;
}  /* get_unit_test_mod */


/********************************************************************
* FUNCTION get_mgr_cli_valset
* 
*  Get the CLI value set
* 
* RETURNS:
*    mgr_cli_valset variable
*********************************************************************/
val_value_t *
    get_mgr_cli_valset (void)
{
    return mgr_cli_valset;
}  /* get_mgr_cli_valset */


/********************************************************************
* FUNCTION get_connect_valset
* 
*  Get the connect value set
* 
* RETURNS:
*    connect_valset variable
*********************************************************************/
val_value_t *
    get_connect_valset (void)
{
    return connect_valset;
}  /* get_connect_valset */


/********************************************************************
* FUNCTION get_aliases_file
* 
*  Get the aliases-file value
* 
* RETURNS:
*    aliases_file variable
*********************************************************************/
const xmlChar *
    get_aliases_file (void)
{
    return aliases_file;
}  /* get_aliases_file */


/********************************************************************
* FUNCTION get_uservars_file
* 
*  Get the uservars-file value
* 
* RETURNS:
*    uservars_file variable
*********************************************************************/
const xmlChar *
    get_uservars_file (void)
{
    return uservars_file;
}  /* get_uservars_file */


/********************************************************************
* FUNCTION get_sessions_file
* 
*  Get the sessions-file value
* 
* RETURNS:
*    sessions_file variable
*********************************************************************/
const xmlChar *
    get_sessions_file (void)
{
    return sessions_file;

}  /* get_sessions_file */


/********************************************************************
* FUNCTION get_test_suite_file
* 
*  Get the test-suite-file value
* 
* RETURNS:
*    test_suite_file variable
*********************************************************************/
const xmlChar *
    get_test_suite_file (void)
{
    return testsuite_file;

}  /* get_test_suite_file */


/********************************************************************
* FUNCTION replace_connect_valset
* 
*  Replace the current connect value set with a clone
* of the specified connect valset
* 
* INPUTS:
*    valset == value node to clone that matches the object type
*              of the input section of the connect operation
*
* RETURNS:
*    status
*********************************************************************/
status_t
    replace_connect_valset (const val_value_t *valset)
{
    val_value_t   *findval, *replaceval;

#ifdef DEBUG
    if (valset == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (connect_valset == NULL) {
        log_debug3("\nCreating new connect_valset");
        connect_valset = val_clone(valset);
        if (connect_valset == NULL) {
            return ERR_INTERNAL_MEM;
        } else {
            return NO_ERR;
        }
    }

    /* go through the connect value set passed in and compare
     * to the existing connect_valset; only replace the
     * parameters that are not already set
     */
    for (findval = val_get_first_child(valset);
         findval != NULL;
         findval = val_get_next_child(findval)) {

        replaceval = val_find_child(connect_valset,
                                    val_get_mod_name(findval),
                                    findval->name);
        if (replaceval == NULL) {
            log_debug3("\nCreating new connect_valset '%s' parameter",
                       findval->name);
            replaceval = val_clone(findval);
            if (replaceval == NULL) {
                return ERR_INTERNAL_MEM;
            }
            val_add_child(replaceval, connect_valset);
        } else {
            log_debug("\nReplacing connect_valset '%s' parameter",
                      findval->name);
            val_value_t *swapval = val_clone(findval);
            if (swapval == NULL) {
                return ERR_INTERNAL_MEM;
            }
            val_swap_child(swapval, replaceval);
            val_free_value(replaceval);
        }
    }

    return NO_ERR;

}  /* replace_connect_valset */


/********************************************************************
* FUNCTION get_mgrloadQ
* 
*  Get the mgrloadQ value pointer
* 
* RETURNS:
*    mgrloadQ variable
*********************************************************************/
dlq_hdr_t *
    get_mgrloadQ (void)
{
    return &mgrloadQ;
}  /* get_mgrloadQ */


/********************************************************************
* FUNCTION get_aliasQ
* 
*  Get the aliasQ value pointer
* 
* RETURNS:
*    aliasQ variable
*********************************************************************/
dlq_hdr_t *
    get_aliasQ (void)
{
    return &aliasQ;
}  /* get_aliasQ */


/********************************************************************
* FUNCTION get_time_rpcs
* 
*  Get the time-rpcs value
* 
* RETURNS:
*    time_rpcs variable
*********************************************************************/
boolean
    get_time_rpcs (void)
{
    return time_rpcs;
}  /* get_time_rpcs */


/********************************************************************
* FUNCTION get_time_rpcs_stats
* 
*  Get the time-rpcs-stats value
* 
* RETURNS:
*    time_rpcs_stats variable
*********************************************************************/
boolean
    get_time_rpcs_stats (void)
{
    return time_rpcs_stats;
}  /* get_time_rpcs_stats */


/********************************************************************
* FUNCTION get_echo_replies
* 
*  Get the echo-replies value
* 
* RETURNS:
*    echo_replies variable
*********************************************************************/
boolean
    get_echo_replies (void)
{
    return echo_replies;
}  /* get_echo_replies */


/********************************************************************
* FUNCTION get_yangcli_file_saved_mtime
*
* Get the last saved modification time of the yangcli file.
*
* INPUTS:
*   yangcli_file_t: yangcli default file.
*
* RETURNS:
*   the last modification time.
*********************************************************************/
extern time_t
    get_yangcli_file_saved_mtime (yangcli_file_t whichfile)
{
    switch (whichfile) {
    case HISTORY_FILE:
        return yangcli_def_history_file_mtime;
    case ALIASES_FILE:
        return yangcli_def_aliases_file_mtime;
    case USERVARS_FILE:
        return yangcli_def_uservars_file_mtime;
    case SESSIONS_FILE:
        return yangcli_def_session_file_mtime;
    case TESTSUITE_FILE:
        return yangcli_def_testsuite_file_mtime;
    case TIME_RPCS_STATS_FILE:
        return yangcli_def_rpcs_stats_file_mtime;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return 0;
    }

}  /* get_yangcli_file_mtime */

/********************************************************************
* FUNCTION get_yangcli_def_file
*
* INPUTS:
*   yangcli_file_t: yangcli default file.
*
* RETURNS:
*    xmlChar * yangcli_default_file
*********************************************************************/
extern const xmlChar *
    get_yangcli_def_file (yangcli_file_t which_flag, status_t* res)
{
    *res = NO_ERR;

    switch (which_flag) {
    case ALIASES_FILE:
        return aliases_file;
    case USERVARS_FILE:
        return uservars_file;
    case SESSIONS_FILE:
       return sessions_file;
    case TESTSUITE_FILE:
        return testsuite_file;
    // TODO: case TIME_RPCS_STATS_FILE:
    case HISTORY_FILE:
        return YANGCLI_DEF_HISTORY_FILE;
    default:
        *res = ERR_INTERNAL_VAL;
        SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }
}  /* get_yangcli_def_file  */

/********************************************************************
* FUNCTION check_def_file_mtime_changed
*
* Check if default yangcli file has been modified.
*
* INPUTS:
*   yangcli_file_t:which yangcli file.
*
* RETURNS: boolean changed or not
*********************************************************************/
extern boolean
    check_def_file_mtime_changed (yangcli_file_t whichfile)
{
    boolean change_flag = FALSE;
    status_t  res = NO_ERR;
    const xmlChar *def_fspec;
    xmlChar   *def_fullspec;
    time_t    saved_mtime = get_yangcli_file_saved_mtime(whichfile);
    time_t    mtime = 0;
    double    timediff = 0;

#define NO_CHANGE (double)0
    def_fspec = get_yangcli_def_file (whichfile, &res);
    def_fullspec = ncx_get_source(def_fspec, &res);
    res = ncxmod_get_file_mtime (def_fullspec, &mtime);

    timediff = difftime(mtime, saved_mtime);
    if (saved_mtime && (timediff > NO_CHANGE)) {
        change_flag = TRUE;
    }

    m__free(def_fullspec);
    return change_flag;

} /* check_def_file_mtime_changed */

/********************************************************************
* FUNCTION update_def_yangcli_file_mtime
*
* Update the modification time of the specified yangcli file.
*
* INPUTS:
*   yangcli_def_file_t: yangcli default file.
*   fullspec: yangcli file.
* RETURNS: none
*********************************************************************/
extern status_t
    update_def_yangcli_file_mtime (yangcli_file_t whichfile,
                                   const xmlChar *fullspec )
{
    time_t    mtime;
    time_t * p_mtime;
    status_t  res = NO_ERR;

    const xmlChar *def_fspec;
    xmlChar   *def_fullspec;

    def_fspec = get_yangcli_def_file (whichfile, &res);

    switch (whichfile) {
        case ALIASES_FILE:
           p_mtime = &yangcli_def_aliases_file_mtime;
           break;
        case HISTORY_FILE:
           p_mtime = &yangcli_def_history_file_mtime;
           break;
        case USERVARS_FILE:
           p_mtime = &yangcli_def_uservars_file_mtime;
           break;
        case SESSIONS_FILE:
           p_mtime = &yangcli_def_session_file_mtime;
           break;
        case TESTSUITE_FILE:
            p_mtime = &yangcli_def_testsuite_file_mtime;
            break;
        case TIME_RPCS_STATS_FILE:
            p_mtime = &yangcli_def_rpcs_stats_file_mtime;
            break;
        default:
            return ERR_INTERNAL_VAL;
    }

    /*
     * No mtime update if it is not a default file.
     */
    def_fullspec = ncx_get_source(def_fspec, &res);
    if (xml_strcmp(def_fullspec, fullspec)) {
       m__free(def_fullspec);
       return NO_ERR;
    }

    m__free(def_fullspec);

    /* Update default file's mtime. */
    res = ncxmod_get_file_mtime (fullspec, &mtime);
    *p_mtime = mtime;

    return res;

} /* update_def_yangcli_file_mtime */

/********************************************************************
* FUNCTION check_for_saving_def_yangcli_file
*
* Check if the default files are to be saved.
*
* INPUTS:
*   yangcli_file_t:which yangcli file.
*   filespec == full file spec to check
*
* RETURNS: status
*********************************************************************/
extern status_t
    check_for_saving_def_yangcli_file (yangcli_file_t whichfile,
                                       const xmlChar *fullspec)
{
    status_t  res = NO_ERR;
    const xmlChar *def_fspec;
    xmlChar   *def_fullspec;
    const xmlChar *fileinfo = NULL;

#define NO_CHANGE (double)0

    def_fspec = get_yangcli_def_file (whichfile, &res);
    time_t    saved_mtime = get_yangcli_file_saved_mtime(whichfile);
    time_t    mtime = 0;

    /* Find the last mtime of the file */
    res = ncxmod_get_file_mtime (fullspec, &mtime);
    if (res == NO_ERR || res == ERR_NCX_NOT_FOUND) {
        res = NO_ERR;
    } else {
        return res;
    }
    switch (whichfile) {
        case ALIASES_FILE:
            fileinfo = (const xmlChar *)"Aliases";
            break;
        case HISTORY_FILE:
            fileinfo = (const xmlChar *)"History";
            break;
        case USERVARS_FILE:
           fileinfo = (const xmlChar *)"Uservars";
            break;
        case SESSIONS_FILE:
            fileinfo = (const xmlChar *)"Sessions";
            break;
        case TESTSUITE_FILE:
            fileinfo = (const xmlChar *)"Tests";
            break;
        case TIME_RPCS_STATS_FILE:
            fileinfo = (const xmlChar *)"RPC stats";
            break;
        default:
           return ERR_INTERNAL_VAL;
           break;
    }

    def_fullspec = ncx_get_source(def_fspec, &res);

    /* Allow change to be saved in user specified non-default file */
    if (xml_strcmp(def_fullspec, fullspec)) {
       res = NO_ERR;
    } else {
        double  timediff = 0;
        if (saved_mtime) {
            timediff = difftime(mtime, saved_mtime);
        }
    
        if (saved_mtime && (timediff > NO_CHANGE)) {
            res = ERR_NCX_CANCELED;
            if (LOGDEBUG2) {
                log_warn("\nWarning: %s are not saved in %s. Another client "
                         "has modified this file.\n", fileinfo, def_fspec);
            }
        } else if (get_yangcli_param_change_flag (whichfile) == TRUE) {
            update_yangcli_param_change_flag (whichfile, FALSE);
            if (LOGDEBUG4) {
                log_info ("\n%s are changed. save %s.\n",
                                   fileinfo, def_fspec);
            }
        } 
    }

    m__free(def_fullspec);
    return res;

}  /* check_for_saving_def_yangcli_file */


/********************************************************************
* FUNCTION get_yangcli_param_change_flag
*
* get_yangcli_param_change_flag.
*
* INPUTS:
*   yangcli_file_t: yangcli default file.
*
* RETURNS:
*   the change flag: TRUE or FALSE
*********************************************************************/
extern boolean
    get_yangcli_param_change_flag (yangcli_file_t which_flag)
{
    switch (which_flag) {
    case HISTORY_FILE:
        return history_change;
    case ALIASES_FILE:
        return aliases_change;
    case USERVARS_FILE:
        return uservars_change;
    case SESSIONS_FILE:
        return session_change;
    case TESTSUITE_FILE:
        return testsuite_change;
    case TIME_RPCS_STATS_FILE:
        return rpcs_stats_change;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return FALSE;
    }

}  /* get_yangcli_param_change_flag */

/*********************************************************************
* FUNCTION update_yangcli_file_param_change_flag
*
* Update param change flag for the specified file.
*
* INPUTS:
*   yangcli_def_file_t: yangcli default file.
*   boolean: change_flag: TRUE or FALSE.
*
* RETURNS: none
*********************************************************************/
extern status_t
    update_yangcli_param_change_flag (yangcli_file_t which_flag,
                                      boolean change_flag )
{
    status_t  res = NO_ERR;

    switch (which_flag) {
    case HISTORY_FILE:
        history_change = change_flag;
        break;
    case ALIASES_FILE:
        aliases_change = change_flag;
        break;
    case USERVARS_FILE:
        uservars_change = change_flag;
        break;
    case SESSIONS_FILE:
        session_change = change_flag;
        break;
    case TESTSUITE_FILE:
        testsuite_change = change_flag;
        break;
    case TIME_RPCS_STATS_FILE:
        rpcs_stats_change = change_flag;
        break;
    default:
       res = ERR_INTERNAL_VAL;
       break;
    }

    return res;
} /* update_yangcli_param_change */

/*********************************************************************
* FUNCTION update the status of connection_all_in_progress.
*
* INPUTS:
*   boolean: change_stat: TRUE or FALSE.
* RETURNS: none
*********************************************************************/
extern void
    update_connect_all_in_progress (boolean in_progress)
{
    connect_all_in_progress = in_progress;

} /* update_connect_all_in_progress */

/********************************************************************
* FUNCTION connect_all_in_progress
*
* INPUTS: NONE
* RETURNS:
*   the change flag: TRUE or FALSE
*********************************************************************/
extern boolean
    check_connect_all_in_progress (void)
{
    return connect_all_in_progress;

} /* check_connect_all_in_progress */



/********************************************************************
 * FUNCTION yangcli_reply_handler
 * 
 *  handle incoming <rpc-reply> messages
 * 
 * INPUTS:
 *   scb == session receiving RPC reply
 *   req == original request returned for freeing or reusing
 *   rpy == reply received from the server (for checking then freeing)
 *
 * RETURNS:
 *   none
 *********************************************************************/
void
    yangcli_reply_handler (ses_cb_t *scb,
                           mgr_rpc_req_t *req,
                           mgr_rpc_rpy_t *rpy)
{
#ifdef DEBUG
    if (!scb || !req || !rpy) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    lock_cb_t *lockcb = NULL;
    status_t res = NO_ERR;
    mgr_scb_t *mgrcb = scb->mgrcb;
    uint32 usesid = 0;
    server_cb_t *server_cb = cur_server_cb;
    session_cb_t *save_session_cb = server_cb->cur_session_cb;
    session_cb_t *session_cb = NULL;

    if (mgrcb) {
        usesid = mgrcb->agtsid;
        session_cb = mgrcb->session_cb;
        ncx_set_temp_modQ(&mgrcb->temp_modQ);
    }

    if (session_cb == NULL) {
        log_error("\nError: session no longer valid; dropping reply\n");
        mgr_rpc_free_request(req);
        if (rpy) {
            mgr_rpc_free_reply(rpy);
        }
        return;
    }

    const xmlChar *sesname = get_session_name(session_cb);
    if (session_cb != save_session_cb) {
        set_cur_session_cb(server_cb, session_cb);
    }

    boolean anyout = FALSE;
    boolean anyerrors = FALSE;
    boolean servermode = (program_mode == PROG_MODE_SERVER);

    response_type_t resp_type = UT_RPC_DATA;

    /* check the contents of the reply */
    if (rpy && rpy->reply) {
        if (val_find_child(rpy->reply, NC_MODULE, NCX_EL_RPC_ERROR)) {
            if (session_cb->command_mode == CMD_MODE_NORMAL || LOGDEBUG) {
                if (session_cb->echo_replies) {
                    if (servermode) {
                        print_server_errormsg(rpy->reply);
                    } else {
                        log_error("\nRPC Error Reply %s for session %u [%s]:\n",
                                  rpy->msg_id, usesid, sesname);
                        val_dump_value_max(rpy->reply, 0, session_cb->defindent,
                                           DUMP_VAL_LOG, 
                                           session_cb->display_mode,
                                           FALSE, FALSE);
                        log_error_append("\n");
                        anyout = TRUE;
                    }
                }
            }
            anyerrors = TRUE;
            resp_type = UT_RPC_ERROR;
        } else if (val_find_child(rpy->reply, NC_MODULE, NCX_EL_OK)) {
            if (session_cb->command_mode == CMD_MODE_NORMAL || LOGDEBUG2) {
                if (session_cb->echo_replies) {
                    if (session_cb->config_mode) {
                        if (LOGDEBUG) {
                            log_debug("\nRPC OK Reply %s for "
                                      "session %u [%s]:\n",
                                      rpy->msg_id, usesid, sesname);
                            anyout = TRUE;
                        }
                    } else {
                        if (LOGINFO) {
                            log_info("\nRPC OK Reply %s for session %u [%s]:\n",
                                     rpy->msg_id, usesid, sesname);
                            anyout = TRUE;
                        }
                    }
                }
            }
            resp_type = UT_RPC_OK;
        } else if ((session_cb->command_mode == CMD_MODE_NORMAL && LOGINFO) ||
                   (session_cb->command_mode != CMD_MODE_NORMAL && LOGDEBUG2)) {
            if (session_cb->echo_replies) {
                if (session_cb->config_mode) {
                    if (LOGDEBUG) {
                        log_debug("\nRPC Data Reply %s for session %u [%s]: ",
                                  rpy->msg_id, usesid, sesname);
                        log_debug_append("\n");
                        val_dump_value_max(rpy->reply, 0, session_cb->defindent,
                                           DUMP_VAL_LOG, 
                                           session_cb->display_mode,
                                           FALSE, FALSE);
                        log_debug_append("\n");
                        anyout = TRUE;
                    }
                } else if (LOGINFO) {
                    log_info("\nRPC Data Reply %s for session %u [%s]: ",
                             rpy->msg_id, usesid, sesname);
                    log_info_append("\n");
                    val_dump_value_max(rpy->reply, 0, session_cb->defindent,
                                       DUMP_VAL_LOG, session_cb->display_mode,
                                       FALSE, FALSE);
                    log_info_append("\n");
                    anyout = TRUE;
                }
            }
        }

        if (session_cb->time_rpcs) {
            handle_rpc_timing(server_cb, req);
            anyout = TRUE;
        }

        /* handle autotest mode */
        boolean isautotest = yangcli_ut_active(server_cb);
        if (isautotest) {
            yangcli_ut_handle_reply(server_cb, resp_type,
                                    req->msg_id, req->data, 
                                    rpy->reply);
        }

        if (is_test_recording_on (server_cb) == TRUE) {
            record_step_reply (server_cb, resp_type,
                               req->msg_id, req->data,
                               rpy->reply);
        }
        
        /* output data even if there were errors
         * TBD: use a CLI switch to control whether
         * to save if <rpc-errors> received
         */
        if (server_cb->result_name || server_cb->result_filename) {
            /* save the data element if it exists */
            val_value_t *val = 
                val_first_child_name(rpy->reply, NCX_EL_DATA);
            if (val) {
                val_remove_child(val);
            } else {
                if (val_child_cnt(rpy->reply) == 1) {
                    val = val_get_first_child(rpy->reply);
                    val_remove_child(val);
                } else {
                    /* not 1 child node, so save the entire reply
                     * need a single top-level element to be a
                     * valid XML document
                     */
                    val = rpy->reply;
                    rpy->reply = NULL;
                }
            }

            /* hand off the malloced 'val' node here */
            res = finish_result_assign(server_cb, session_cb, val, NULL);
        }  else if (LOGINFO && !anyout && !anyerrors && 
                    session_cb->command_mode == CMD_MODE_NORMAL &&
                    interactive_mode()) {
            if (session_cb->config_mode) {
                log_debug("\nOK\n");
            } else {
                log_info("\nOK\n");
            }
        }
    } else {
        log_error("\nError: yangcli-pro: no reply parsed\n");
    }

    /* check if a script is running */
    if ((anyerrors || res != NO_ERR) && 
        runstack_level(server_cb->runstack_context)) {
        runstack_cancel(server_cb->runstack_context);
    }

    rpc_err_t rpcerrtyp;
    if (anyerrors && rpy->reply) {
        rpcerrtyp = get_rpc_error_tag(rpy->reply);
    } else {
        /* default error: may not get used */
        rpcerrtyp = RPC_ERR_OPERATION_FAILED;
    }

    val_value_t *saveval = NULL;
    boolean done = FALSE;

    switch (session_cb->state) {
    case MGR_IO_ST_CONN_CLOSEWAIT:
        if (mgrcb->closed) {
            mgr_ses_free_session(session_cb->mysid);
            session_cb->mysid = 0;
            session_cb->state = MGR_IO_ST_IDLE;
        } /* else keep waiting */
        break;
    case MGR_IO_ST_CONN_RPYWAIT:
        session_cb->state = MGR_IO_ST_CONN_IDLE;
        switch (session_cb->command_mode) {
        case CMD_MODE_NORMAL:
            break;
        case CMD_MODE_AUTOLOAD:
            (void)autoload_handle_rpc_reply(server_cb, session_cb,
                                            scb, rpy->reply, anyerrors);
            break;
        case CMD_MODE_AUTOLOCK:
            done = FALSE;
            lockcb = &session_cb->lock_cb[session_cb->locks_cur_cfg];
            if (anyerrors) {
                if (rpcerrtyp == RPC_ERR_LOCK_DENIED) {
                    lockcb->lock_state = LOCK_STATE_TEMP_ERROR;
                } else if (lockcb->config_id == NCX_CFGID_CANDIDATE) {
                    res = send_discard_changes_pdu_to_server(server_cb,
                                                             session_cb);
                    if (res != NO_ERR) {
                        handle_locks_cleanup(server_cb, session_cb);
                    }
                    done = TRUE;
                } else {
                    lockcb->lock_state = LOCK_STATE_FATAL_ERROR;
                    done = TRUE;
                }
            } else {
                lockcb->lock_state = LOCK_STATE_ACTIVE;
            }

            if (!done) {
                res = handle_get_locks_request_to_server(server_cb,
                                                         session_cb,
                                                        FALSE, &done);
                if (done) {
                    /* check if the locks are all good */
                    if (get_lock_worked(session_cb)) {
                        log_info("\nget-locks finished OK");
                        session_cb->command_mode = CMD_MODE_NORMAL;
                        session_cb->locks_waiting = FALSE;
                    } else {
                        log_error("\nError: get-locks failed, "
                                  "starting cleanup\n");
                        handle_locks_cleanup(server_cb, session_cb);
                    }
                } else if (res != NO_ERR) {
                    log_error("\nError: get-locks failed, no cleanup\n");
                }
            }
            break;
        case CMD_MODE_AUTOUNLOCK:
            lockcb = &session_cb->lock_cb[session_cb->locks_cur_cfg];
            if (anyerrors) {
                lockcb->lock_state = LOCK_STATE_FATAL_ERROR;
            } else {
                lockcb->lock_state = LOCK_STATE_RELEASED;
            }

            (void)handle_release_locks_request_to_server(server_cb,
                                                         session_cb,
                                                         FALSE, &done);
            if (done) {
                clear_lock_cbs(session_cb);
            }
            break;
        case CMD_MODE_AUTODISCARD:
            lockcb = &session_cb->lock_cb[session_cb->locks_cur_cfg];
            session_cb->command_mode = CMD_MODE_AUTOLOCK;
            if (anyerrors) {
                handle_locks_cleanup(server_cb, session_cb);
            } else {
                res = handle_get_locks_request_to_server(server_cb,
                                                         session_cb,
                                                         FALSE, &done);
                if (done) {
                    /* check if the locks are all good */
                    if (get_lock_worked(session_cb)) {
                        log_info("\nget-locks finished OK");
                        session_cb->command_mode = CMD_MODE_NORMAL;
                        session_cb->locks_waiting = FALSE;
                    } else {
                        log_error("\nError: get-locks failed, "
                                  "starting cleanup\n");
                        handle_locks_cleanup(server_cb, session_cb);
                    }
                } else if (res != NO_ERR) {
                    log_error("\nError: get-locks failed, no cleanup\n");
                }
            }            
            break;
        case CMD_MODE_AUTOCONFIG:
            saveval = val_find_child(rpy->reply, NULL, NCX_EL_DATA);
            if (session_cb->config_tree) {
                if (saveval) {
                    config_check_transfer(session_cb, saveval);
                }
                log_debug3("\nDeleting old autoconfig config_tree");
                val_free_value(session_cb->config_tree);
                session_cb->config_tree = NULL;
            }
            log_debug3("\nSetting autoconfig config_tree");
                session_cb->config_tree = saveval;
                val_find_child(rpy->reply, NULL, NCX_EL_DATA);
            if (session_cb->config_tree) {
                val_remove_child(session_cb->config_tree);
                session_cb->config_tree_dirty = FALSE;
            } else {
                log_error("\nError: No <data> element found in "
                          "<get-config> reply\n");
            }
            session_cb->command_mode = CMD_MODE_NORMAL;
            break;
        case CMD_MODE_AUTONOTIF:
            session_cb->command_mode = CMD_MODE_NORMAL;
            if (anyerrors) {
                log_warn("\nWarning: notification subscription failed; "
                         "shadow config update disabled for this session\n");
            }
            break;
        case CMD_MODE_AUTOTEST:
            res = autotest_handle_rpc_reply(server_cb, session_cb,
                                            rpy->reply, anyerrors);
            break;
        case CMD_MODE_CONF_APPLY:
            session_cb->command_mode = CMD_MODE_NORMAL;
            if (anyerrors) {
                log_warn("\nWarning: Config mode 'save' canceled because "
                         "<edit-config> failed");
            } else {
                /* edit-config for config mode saved right away */
                res = do_save(server_cb);
                if (res != NO_ERR) {
                    log_warn("\nWarning: Config mode 'save' failed (%s)",
                             get_error_string(res));
                } // else command_mode may be changed to CMD_MODE_SAVE
            }
            break;
        case CMD_MODE_SAVE:
            if (anyerrors) {
                log_warn("\nWarning: Final <copy-config> canceled because "
                         "<commit> failed");
            } else {
                (void)finish_save(server_cb, session_cb);
            }
            session_cb->command_mode = session_cb->return_command_mode;
            if (session_cb->command_mode == CMD_MODE_AUTOTEST) {
                res = autotest_handle_rpc_reply(server_cb, session_cb,
                                                rpy->reply, anyerrors);
            }
            break;
        default:
            SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    default:
        break;
    }

    if (session_cb != save_session_cb) {
        set_cur_session_cb(server_cb, save_session_cb);
    }

    /* free the request and reply */
    mgr_rpc_free_request(req);
    if (rpy) {
        mgr_rpc_free_reply(rpy);
    }

}  /* yangcli_reply_handler */


/********************************************************************
 * FUNCTION finish_result_assign
 * 
 * finish the assignment to result_name or result_filename
 * use 1 of these 2 parms:
 *    resultval == result to output to file
 *    !!! This is a live var that is freed by this function
 *
 *    resultstr == result to output as string
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   session_cb == session control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    finish_result_assign (server_cb_t *server_cb,
                          session_cb_t *session_cb,
                          val_value_t *resultvar,
                          const xmlChar *resultstr)
{


#ifdef DEBUG
    if (server_cb == NULL || session_cb == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    status_t res = NO_ERR;

    if (resultvar != NULL) {
        /* force the name strings to be
         * malloced instead of backptrs to
         * the object name
         */
        res = val_force_dname(resultvar);
        if (res != NO_ERR) {
            val_free_value(resultvar);
        }
    }

    if (res != NO_ERR) {
        ;
    } else if (server_cb->result_filename != NULL) {
        res = output_file_result(server_cb, session_cb,
                                 resultvar, resultstr);
        if (resultvar) {
            val_free_value(resultvar);
        }
    } else if (server_cb->result_name != NULL) {
        if (server_cb->result_vartype == VAR_TYP_CONFIG) {
            val_value_t *configvar = 
                var_get(server_cb->runstack_context, server_cb->result_name,
                        VAR_TYP_CONFIG);
            if (configvar==NULL) {
                res = SET_ERROR(ERR_INTERNAL_VAL);
            } else {
                res = handle_config_assign(server_cb, session_cb,
                                           configvar, resultvar, resultstr);
                if (res == NO_ERR) {
                    log_info("\nOK\n");
                }                    
            }
        } else if (resultvar != NULL) {
            /* save the filled in value
             * hand off the malloced 'resultvar' here
             */
            if (res == NO_ERR) {
                res = var_set_move(server_cb->runstack_context,
                                   server_cb->result_name, 
                                   xml_strlen(server_cb->result_name),
                                   server_cb->result_vartype,
                                   resultvar);
            }
            if (res != NO_ERR) {
                /* resultvar is freed even if error returned */
                log_error("\nError: set result for '%s' failed (%s)\n",
                          server_cb->result_name, 
                          get_error_string(res));
            } else {
                log_info("\nOK\n");
            }
        } else {
            /* this is just a string assignment */
            res = var_set_from_string(server_cb->runstack_context,
                                      server_cb->result_name,
                                      resultstr, 
                                      server_cb->result_vartype);
            if (res != NO_ERR) {
                log_error("\nyangcli-pro: Error setting variable %s (%s)",
                          server_cb->result_name, 
                          get_error_string(res));
            } else {
                log_info("\nOK\n");
            }
        }
    }

    clear_result(server_cb);

    return res;

}  /* finish_result_assign */


/********************************************************************
* FUNCTION report_capabilities
* 
* Generate a start session report, listing the capabilities
* of the NETCONF server
* 
* INPUTS:
*  server_cb == server control block to use
*  session_cb == session control block to use
*  scb == session control block
*  isfirst == TRUE if first call when session established
*             FALSE if this is from show session command
*  mode == help mode; ignored unless first == FALSE
*********************************************************************/
void
    report_capabilities (server_cb_t *server_cb,
                         session_cb_t *session_cb,
                         const ses_cb_t *scb,
                         boolean isfirst,
                         help_mode_t mode)
{
    if (!LOGINFO) {
        /* skip unless log level is INFO or higher */
        return;
    }

    const mgr_scb_t *mscb = (const mgr_scb_t *)scb->mgrcb;
    const xmlChar *server = NULL;
    boolean clientmode = (get_program_mode() == PROG_MODE_CLIENT);

    if (session_cb->session_cfg) {
        server = session_cb->session_cfg->server_addr;
    } else {
        val_value_t *parm = 
            val_find_child(server_cb->connect_valset, 
                           YANGCLI_MOD, YANGCLI_SERVER);
        if (parm && parm->res == NO_ERR) {
            server = VAL_STR(parm);
        } else {
            server = (const xmlChar *)"--";
        }
    }

    if (clientmode) {
        log_write("\n\nNETCONF session established for %s on %s",
                  scb->username, mscb->target ? mscb->target : server);
        log_write("\n\nClient Session Id: %u", scb->sid);
        log_write("\nServer Session Id: %u", mscb->agtsid);

        if (isfirst || mode > HELP_MODE_BRIEF) {
            log_write("\n\nServer Protocol Capabilities");
            cap_dump_stdcaps(&mscb->caplist);

            log_write("\n\nServer Module Capabilities");
            cap_dump_modcaps(&mscb->caplist);

            log_write("\n\nServer Enterprise Capabilities");
            cap_dump_entcaps(&mscb->caplist);
            log_write("\n");
        }

        log_write("\nProtocol version set to: ");
        switch (ses_get_protocol(scb)) {
        case NCX_PROTO_NETCONF10:
            log_write_append("RFC 4741 (base:1.0)");
            break;
        case NCX_PROTO_NETCONF11:
            log_write_append("RFC 6241 (base:1.1)");
            break;
        default:
            log_write_append("unknown");
        }
    }

    if (!isfirst && (mode <= HELP_MODE_BRIEF)) {
        return;
    }

    if (clientmode) {
        log_write("\nDefault target set to: ");
    }

    switch (mscb->targtyp) {
    case NCX_AGT_TARG_NONE:
        if (isfirst) {
            session_cb->default_target = NULL;
            session_cb->default_target_id = 0;
        }
        if (clientmode) {
            log_write_append("none");
        }
        break;
    case NCX_AGT_TARG_CANDIDATE:
        if (isfirst) {
            session_cb->default_target = NCX_EL_CANDIDATE;
            session_cb->default_target_id = NCX_CFGID_CANDIDATE;
        }
        if (clientmode) {
            log_write_append("<candidate>");
        }
        break;
    case NCX_AGT_TARG_RUNNING:
        if (isfirst) {
            session_cb->default_target = NCX_EL_RUNNING;
            session_cb->default_target_id = NCX_CFGID_RUNNING;
        }
        if (clientmode) {
            log_write_append("<running>");
        }
        break;
    case NCX_AGT_TARG_CAND_RUNNING:
        if (force_target != NULL &&
            !xml_strcmp(force_target, NCX_EL_RUNNING)) {
            /* set to running */
            if (isfirst) {
                session_cb->default_target = NCX_EL_RUNNING;
                session_cb->default_target_id = NCX_CFGID_RUNNING;
            }
            if (clientmode) {
                log_write_append("<running> (<candidate> also supported)");
            }
        } else {
            /* set to candidate */
            if (isfirst) {
                session_cb->default_target = NCX_EL_CANDIDATE;
                session_cb->default_target_id = NCX_CFGID_CANDIDATE;
            }
            if (clientmode) {
                log_write_append("<candidate> (<running> also supported)");
            }
        }
        break;
    case NCX_AGT_TARG_LOCAL:
        if (isfirst) {
            session_cb->default_target = NULL;
            session_cb->default_target_id = 0;
        }
        if (clientmode) {
            log_write_append("none -- local file");
        }
        break;
    case NCX_AGT_TARG_REMOTE:
        if (isfirst) {
            session_cb->default_target = NULL;
            session_cb->default_target_id = 0;
        }
        if (clientmode) {
            log_write_append("none -- remote file");
        }
        break;
    default:
        if (isfirst) {
            session_cb->default_target = NULL;
            session_cb->default_target_id = 0;
        }
        if (clientmode) {
            log_write_append("none -- unknown (%d)", mscb->targtyp);
        }
        SET_ERROR(ERR_INTERNAL_VAL);
        break;
    }

    if (!clientmode) {
        return;
    }

    log_write("\nSave operation mapped to: ");
    switch (mscb->targtyp) {
    case NCX_AGT_TARG_NONE:
        log_write_append("none");
        break;
    case NCX_AGT_TARG_CANDIDATE:
    case NCX_AGT_TARG_CAND_RUNNING:
        if (!xml_strcmp(session_cb->default_target, NCX_EL_CANDIDATE)) {
            log_write_append("commit");
            if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
                log_write_append(" + copy-config <running> <startup>");
            }
        } else {
            if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
                log_write_append("copy-config <running> <startup>");
            } else {
                log_write_append("none");
            }
        }
        break;
    case NCX_AGT_TARG_RUNNING:
        if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
            log_write_append("copy-config <running> <startup>");
        } else {
            log_write_append("none");
        }           
        break;
    case NCX_AGT_TARG_LOCAL:
    case NCX_AGT_TARG_REMOTE:
        /* no way to assign these enums from the capabilities alone! */
        if (cap_std_set(&mscb->caplist, CAP_STDID_URL)) {
            log_write_append("copy-config <running> <url>");
        } else {
            log_write_append("none");
        }           
        break;
    default:
        log_write_append("none");
        SET_ERROR(ERR_INTERNAL_VAL);
        break;
    }

    log_write("\nDefault with-defaults behavior: ");
    if (mscb->caplist.cap_defstyle) {
        log_write_append("%s", mscb->caplist.cap_defstyle);
    } else {
        log_write_append("unknown");
    }

    log_write("\nAdditional with-defaults behavior: ");
    if (mscb->caplist.cap_supported) {
        log_write_append("%s", mscb->caplist.cap_supported);
    } else {
        log_write_append("unknown");
    }

    log_write_append("\n");
    
} /* report_capabilities */


/********************************************************************
 * FUNCTION clean_session_cbQ
 * 
 * Delete all sessions for a server context
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    all == TRUE for program cleanup, will include the
 *           default session and 
 *********************************************************************/
void clean_session_cbQ (server_cb_t *server_cb,
                        boolean all)
{
    session_cb_t *session_cb =
        (session_cb_t *)dlq_firstEntry(&server_cb->session_cbQ);
    while (session_cb) {
        session_cb_t *next_session_cb =
            (session_cb_t *)dlq_nextEntry(session_cb);
        if (session_cb->def_session && !all) {
            ;
        } else {
            dlq_remove(session_cb);
            free_session_cb(session_cb);
        }
        session_cb = next_session_cb;
    }

}  /* clean_session_cbQ */


/********************************************************************
 * FUNCTION delete_one_session_cb
 *
 * Delete one session for a server context
 *
 * INPUTS:
 *    session_cb_t *session_cb
 *********************************************************************/
void delete_one_session_cb (server_cb_t *server_cb,
                            session_cb_t *session_cb)
{
    if (session_cb->def_session) {
        return; /* Not to delete def_session_cb */
    }

    /*** If this deleted session is a cur_session_cb ***/
    /*** force current session back to the default session ***/
    if (server_cb->cur_session_cb == session_cb ) {
            server_cb->cur_session_cb =
              find_session_cb(server_cb, NCX_EL_DEFAULT);
    }

    dlq_remove(session_cb);
    free_session_cb(session_cb);

}  /* delete_one_session_cb */


/********************************************************************
 * FUNCTION server_connected
 * 
 * Return TRUE if any sessions are connected
 *
 * INPUTS:
 *    server_cb == server control block to use
 *********************************************************************/
boolean 
    server_connected (server_cb_t *server_cb)
{
    session_cb_t *session_cb =
        (session_cb_t *)dlq_firstEntry(&server_cb->session_cbQ);
    for (; session_cb != NULL;
         session_cb = (session_cb_t *)dlq_nextEntry(session_cb)) {

        if (session_connected(session_cb)) {
            return TRUE;
        }
    }
    return FALSE;

}  /* server_connected */


/********************************************************************
 * FUNCTION session_connected_count
 * 
 * Return number of open sessions
 *
 * INPUTS:
 *    server_cb == server control block to use
 * RETURNS:
 *    number of sessions in some sort of connection state
 *********************************************************************/
uint32
    session_connected_count (server_cb_t *server_cb)
{
    uint32 count = 0;
    session_cb_t *session_cb =
        (session_cb_t *)dlq_firstEntry(&server_cb->session_cbQ);
    for (; session_cb != NULL;
         session_cb = (session_cb_t *)dlq_nextEntry(session_cb)) {

        if (session_connected(session_cb)) {
            count++;
        }
    }
    return count;

}  /* session_connected_count */


/********************************************************************
 * FUNCTION session_connected
 * 
 * Return TRUE if the specified session is connected
 *
 * INPUTS:
 *    session_cb == session control block to use
 *********************************************************************/
boolean 
    session_connected (session_cb_t *session_cb)
{
    switch (session_cb->state) {
    case MGR_IO_ST_INIT:
    case MGR_IO_ST_IDLE:
    case MGR_IO_ST_SHUT:
    case MGR_IO_ST_CONN_SHUT:
        break;
    case MGR_IO_ST_CONNECT:
    case MGR_IO_ST_CONN_START:
    case MGR_IO_ST_CONN_IDLE:
    case MGR_IO_ST_CONN_RPYWAIT:
    case MGR_IO_ST_CONN_CANCELWAIT:
    case MGR_IO_ST_CONN_CLOSEWAIT:
        return TRUE;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }
    return FALSE;

}  /* session_connected */


/********************************************************************
 * FUNCTION find_session_cb
 * 
 * Return the specified entry or NULL if not found
 *
 * INPUTS:
 *    server_cb == server context to use
 *    name == name of active session struct to find
 *********************************************************************/
session_cb_t *find_session_cb (server_cb_t *server_cb,
                               const xmlChar *name)
{
    boolean isdefault = !xml_strcmp(name, NCX_EL_DEFAULT);

    session_cb_t *session_cb =
        (session_cb_t *)dlq_firstEntry(&server_cb->session_cbQ);
    for (; session_cb != NULL;
         session_cb = (session_cb_t *)dlq_nextEntry(session_cb)) {
    
        if (session_cb->session_cfg) {
            if (!xml_strcmp(name, session_cb->session_cfg->name)) {
                return session_cb;
            }
        } else if (isdefault && session_cb->def_session) {
            return session_cb;
        }
    }
    return NULL;

} /* find_session_cb */


/********************************************************************
 * FUNCTION find_session_cfg
 * 
 * Return the specified entry or NULL if not found
 *
 * INPUTS:
 *    server_cb == server context to use
 *    name == name of saved session config struct to find
 *********************************************************************/
session_cfg_t *find_session_cfg (server_cb_t *server_cb,
                                 const xmlChar *name)
{
    session_cfg_t *session_cfg =
        (session_cfg_t *)dlq_firstEntry(&server_cb->session_cfgQ);
    for (; session_cfg != NULL && name != NULL;
         session_cfg = (session_cfg_t *)dlq_nextEntry(session_cfg)) {
    
        if (!xml_strcmp(name, session_cfg->name)) {
            return session_cfg;
        }
    }
    return NULL;

} /* find_session_cfg */


/********************************************************************
 * FUNCTION add_session_cb
 * 
 * Create a new session_cb and add it to the server_cb
 *
 * INPUTS:
 *    server_cb == server context to use
 *    session_cfg == session_cfg to use
 * RETURNS:
 * pointer to session_cbcreated -- do not free -- Q in server_cb
 *  NULL id malloc failed
 *********************************************************************/
session_cb_t * add_session_cb (server_cb_t *server_cb,
                               session_cfg_t *session_cfg)
{
    session_cb_t *session_cb = new_session_cb(server_cb, session_cfg);
    if (session_cb) {
        session_cb->session_cfg_new = FALSE;
        dlq_enque(session_cb, &server_cb->session_cbQ);
    }
    return session_cb;

} /* add_session_cb */


/********************************************************************
 * FUNCTION set_cur_session_cb
 * 
 * Set the current session for a server context
 *
 * INPUTS:
 *    server_cb == server context to use
 *    session_cb == session_cb to make the current context
 *********************************************************************/
void
    set_cur_session_cb (server_cb_t *server_cb,
                        session_cb_t *session_cb)
{
    server_cb->cur_session_cb = session_cb;

    ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (scb) {
        mgr_scb_t *mscb = (mgr_scb_t *)scb->mgrcb;
        if (mscb) {
            ncx_set_session_modQ(&mscb->temp_modQ);
            ncx_set_temp_modQ(&mscb->temp_modQ);
        }
    }

} /* set_cur_session_cb */


/********************************************************************
 * FUNCTION get_session_name
 * 
 * Get the name for the specified session context
 *
 * INPUTS:
 *    session_cb == session_cb to get name
 * RETURNS:
 *    name of session (from session_cfg or 'default')
 *********************************************************************/
const xmlChar *
    get_session_name (session_cb_t *session_cb)
{
    if (session_cb->session_cfg) {
        return session_cb->session_cfg->name;
    }
    return NCX_EL_DEFAULT;
}  /* get_session_name */


/********************************************************************
* FUNCTION clean_session_cb_conn
* 
*  Clean a session control block after a connection
*  to the server is terminated
* 
* INPUTS:
*    session_cb == control block to clean
*
*********************************************************************/
void
    clean_session_cb_conn (session_cb_t *session_cb)
{
    if (session_cb == NULL) {
        return;
    }

    while (!dlq_empty(&session_cb->modptrQ)) {
        modptr_t *modptr = (modptr_t *)dlq_deque(&session_cb->modptrQ);
        free_modptr(modptr);
    }

    while (!dlq_empty(&session_cb->notificationQ)) {
        mgr_not_msg_t *notif =
            (mgr_not_msg_t *)dlq_deque(&session_cb->notificationQ);
        mgr_not_free_msg(notif);
    }

    ncxmod_clean_search_result_queue(&session_cb->searchresultQ);

    session_cb->command_mode = CMD_MODE_NORMAL;
    session_cb->locks_active = FALSE;
    session_cb->locks_waiting = FALSE;
    session_cb->locks_cleanup = FALSE;
    session_cb->mysid = 0;
    session_cb->state = MGR_IO_ST_IDLE;
    session_cb->returncode = MGR_IO_RC_NONE;

}  /* clean_session_cb_conn */


/********************************************************************
 * FUNCTION yangcli_init
 * 
 * Init the NCX CLI application
 * 
 * INPUTS:
 *   argc == number of strings in argv array
 *   argv == array of command line strings
 *   progmode == program mode
 *   quickexit == address of return quick exit flag
 *
 * OUTPUTS:
 *   *quickexit == TRUE if quick exit; FALSE == normal run
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t 
    yangcli_init (int argc,
                  char *argv[],
                  program_mode_t progmode,
                  boolean *quickexit)
{
    val_value_t *modval = NULL;

    *quickexit = FALSE;

    switch (progmode) {
    case PROG_MODE_CLIENT:
    case PROG_MODE_SERVER:
        program_mode = progmode;
        break;
    case PROG_MODE_NONE:
    default:
        return SET_ERROR(ERR_INTERNAL_VAL);
    }
        
    /* set the default debug output level/mode */
    log_init_logfn_va();

    log_debug_t log_level = LOG_DEBUG_INFO;

    log_set_debug_app(LOG_DEBUG_APP_YANGCLI);

    dlq_hdr_t savedevQ;
    dlq_createSQue(&savedevQ);

    /* init the module static vars in order */
    dlq_createSQue(&server_cbQ);
    cur_server_cb = NULL;
    yangcli_mod = NULL;
    netconf_mod = NULL;
    test_suite_mod = NULL;
    mgr_cli_valset = NULL;
    batchmode = FALSE;
    helpmode = FALSE;
    helpsubmode = HELP_MODE_NONE;
    versionmode = FALSE;
    runscript = NULL;
    runscriptdone = FALSE;
    runcommand = NULL;
    runcommanddone = FALSE;
    dlq_createSQue(&mgrloadQ);
    temp_progcb = NULL;
    dlq_createSQue(&modlibQ);
    dlq_createSQue(&aliasQ);
    init_done = FALSE;
    autoaliases = TRUE;
    aliases_file = YANGCLI_DEF_ALIASES_FILE;
    autoload = TRUE;
    autocomp = TRUE;
    autoconfig = TRUE;
    autohistory = TRUE;
    autonotif = TRUE;
    autosessions = (program_mode == PROG_MODE_CLIENT) ? TRUE : FALSE;
    autotest = (program_mode == PROG_MODE_CLIENT) ? TRUE : FALSE;
    autouservars = TRUE;
    uservars_file = YANGCLI_DEF_USERVARS_FILE;
    testsuite_file = YANGCLI_DEF_TESTSUITE_FILE;
    testsuite_file_default = TRUE;
    testsuite_file_opened = FALSE;
    sessions_file = YANGCLI_DEF_SESSIONS_FILE;
    baddata = YANGCLI_DEF_BAD_DATA;
    connect_valset = NULL;
    confname = NULL;
    default_module = NULL;
    default_timeout = 30;
    display_mode = NCX_DISPLAY_MODE_PLAIN;
    fixorder = TRUE;
    optional = FALSE;
    testoption = YANGCLI_DEF_TEST_OPTION;
    erroption = YANGCLI_DEF_ERROR_OPTION;
    defop = YANGCLI_DEF_DEFAULT_OPERATION;
    withdefaults = YANGCLI_DEF_WITH_DEFAULTS;
    prompt_type = HELP_MODE_NORMAL;
    defindent = NCX_DEF_INDENT;
    msg_defindent = NCX_DEF_MSG_INDENT;
    echo_notif_loglevel = LOG_DEBUG_DEBUG;
    echo_notifs = (program_mode == PROG_MODE_CLIENT);
    echo_replies = (program_mode == PROG_MODE_CLIENT);
    script_input = TRUE;
    time_rpcs = FALSE;
    time_rpcs_stats = FALSE;
    time_rpcs_stats_file = YANGCLI_DEF_TIME_RPCS_STATS_FILE;
    match_names = NCX_MATCH_EXACT;
    alt_names = TRUE;
    force_target = NULL;
    use_xmlheader = TRUE;
    config_edit_mode = CFG_EDITMODE_LEVEL;
    yangcli_def_aliases_file_mtime = 0;
    yangcli_def_uservars_file_mtime = 0;
    yangcli_def_history_file_mtime = 0;
    yangcli_def_session_file_mtime = 0;
    yangcli_def_testsuite_file_mtime = 0;
    yangcli_def_rpcs_stats_file_mtime = 0;

    aliases_change = FALSE;
    uservars_change = FALSE;
    history_change = TRUE;
    session_change = FALSE;
    testsuite_change = FALSE;
    rpcs_stats_change = FALSE;

    connect_all_in_progress = FALSE;

#ifdef EVAL_VERSION
    command_count = 0;
#endif

    /* set the character set LOCALE to the user default */
    setlocale(LC_CTYPE, "");

    /* initialize the NCX Library first to allow NCX modules
     * to be processed.  No module can get its internal config
     * until the NCX module parser and definition registry is up
     */
    status_t res = ncx_init(NCX_SAVESTR, log_level,
                            TRUE, /* logtstamps */
                            TRUE, /* collapse_submods */
                            FALSE, /* xpath_backptrs */
                            NULL, /* startmsg */
                            argc, argv);
    if (res != NO_ERR) {
        return res;
    }

#ifdef YANGCLI_DEBUG
    if (argc>1 && LOGDEBUG2) {
        log_debug2("\nCommand line parameters:");
        int   i;
        for (i=0; i<argc; i++) {
            log_debug2("\n   arg%d: %s", i, argv[i]);
        }
    }
#endif

    /* init the notification callback handler module */
    yangcli_notif_init();

    /* initialize the autoconfig module; must be after notif_init */
    res = autoconfig_init();
    if (res != NO_ERR) {
        log_error("\nError: autoconfig module init failed\n");
        return res;
    }

    /* make sure the Yuma directory
     * exists for saving per session data
     */
    res = ncxmod_setup_yumadir();
    if (res != NO_ERR) {
        log_error("\nError: could not setup yuma dir '%s'\n",
                  ncxmod_get_yumadir());
        return res;
    }

    /* make sure the Yuma temp directory
     * exists for saving per session data
     */
    res = ncxmod_setup_tempdir();
    if (res != NO_ERR) {
        log_error("\nError: could not setup temp dir '%s/tmp'\n",
                  ncxmod_get_yumadir());
        return res;
    }

    /* make sure the Yuma testrecord directory
     * exists for saving rpydata for data validation.
     */
    res = ncxmod_setup_recordtest_dir();
    if (res != NO_ERR) {
        log_error("\nError: could not setup recordtest dir '%s/recordtest'\n",
                  ncxmod_get_yumadir());
        return res;
    }


    /* at this point, modules that need to read config
     * params can be initialized
     */

#ifdef LOGGER_TEST
    log_backtrace(LOG_DEBUG_DEV1,
		  "\n%s: DEV1: Post ncx_init() backtrace ...", __FUNCTION__);
#endif

    /* Load the yangcli base module */
    res = load_base_schema();
    if (res != NO_ERR) {
        return res;
    }

    /* Initialize the Netconf Manager Library */
    res = mgr_init();
    if (res != NO_ERR) {
        return res;
    }

    /* set up handler for incoming notifications */
    mgr_not_set_callback_fn(yangcli_notification_handler);

    /* init the connect parmset object template;
     * find the connect RPC method
     * !!! MUST BE AFTER load_base_schema !!!
     */
    obj_template_t *obj = ncx_find_object(yangcli_mod, YANGCLI_CONNECT);
    if (obj==NULL) {
        return ERR_NCX_DEF_NOT_FOUND;
    }

    /* set the parmset object to the input node of the RPC */
    obj = obj_find_child(obj, NULL, YANG_K_INPUT);
    if (obj==NULL) {
        return ERR_NCX_DEF_NOT_FOUND;
    }

    /* treat the connect-to-server parmset special
     * it is saved for auto-start plus restart parameters
     * Setup an empty parmset to hold the connect parameters
     */
    connect_valset = val_new_value();
    if (connect_valset==NULL) {
        return ERR_INTERNAL_MEM;
    } else {
        val_init_from_template(connect_valset, obj);
    }

    /* create the program instance temporary directory */
    temp_progcb = ncxmod_new_program_tempdir(&res);
    if (temp_progcb == NULL || res != NO_ERR) {
        return res;
    }

    /* set the CLI handler */
    mgr_io_set_stdin_handler(yangcli_stdin_handler);

    /* set the session close handler */
    mgr_set_ses_closed_handler (yangcli_session_closed_handler);

    /* create a default server control block */
    server_cb_t *server_cb = new_server_cb(YANGCLI_DEF_SERVER);
    if (server_cb == NULL) {
        return ERR_INTERNAL_MEM;
    }
    dlq_enque(server_cb, &server_cbQ);
    cur_server_cb = server_cb;

    /* create a default session control block */
    session_cb_t *session_cb = new_session_cb(server_cb, NULL);
    if (session_cb == NULL) {
        return ERR_INTERNAL_MEM;
    }
    session_cb->def_session = TRUE;
    dlq_enque(session_cb, &server_cb->session_cbQ);
    server_cb->cur_session_cb = session_cb;
    
    /* Get any command line and conf file parameters */
    res = process_cli_input(server_cb, argc, argv);
    if (res != NO_ERR) {
        return res;
    }

    /* check print version */
    if (versionmode || helpmode) {
        xmlChar versionbuffer[NCX_VERSION_BUFFSIZE];
        res = ncx_get_version(versionbuffer, NCX_VERSION_BUFFSIZE);
        if (res == NO_ERR) {
            log_stdout("\n%s version %s\n", PROGNAME, versionbuffer);
        } else {
            SET_ERROR(res);
        }
    }

    /* check print help and exit */
    if (helpmode) {
        help_program_module(YANGCLI_MOD, YANGCLI_BOOT, helpsubmode);
    }

    /* check quick exit */
    if (helpmode || versionmode) {
        *quickexit = TRUE;
        return NO_ERR;
    }

    /* set any server control block defaults which were supplied
     * in the CLI or conf file
     */
    update_session_cb_vars(session_cb);

    /* Load the NETCONF, XSD, SMI and other core modules */
    if (autoload) {
        res = load_core_schema();
        if (res != NO_ERR) {
            return res;
        }
    }

    /* check if there are any deviation parameters to load first */
    for (modval = val_find_child(mgr_cli_valset, YANGCLI_MOD,
                                 NCX_EL_DEVIATION);
         modval != NULL && res == NO_ERR;
         modval = val_find_next_child(mgr_cli_valset, YANGCLI_MOD,
                                      NCX_EL_DEVIATION,
                                      modval)) {

        res = ncxmod_load_deviation(VAL_STR(modval), &savedevQ);
        if (res != NO_ERR) {
            log_error("\n load deviation failed (%s)", 
                      get_error_string(res));
        } else {
            log_info("\n load OK");
        }
    }

    if (res == NO_ERR) {
        /* check if any explicitly listed modules should be loaded */
        modval = val_find_child(mgr_cli_valset, YANGCLI_MOD, NCX_EL_MODULE);
        while (modval != NULL && res == NO_ERR) {
            log_info("\nyangcli-pro: Loading requested module %s", 
                     VAL_STR(modval));

            xmlChar *revision = NULL;
            xmlChar *savestr = NULL;
            uint32   modlen = 0;

            if (yang_split_filename(VAL_STR(modval), &modlen)) {
                savestr = &(VAL_STR(modval)[modlen]);
                *savestr = '\0';
                revision = savestr + 1;
            }

            res = ncxmod_load_module(VAL_STR(modval), revision,
                                     &savedevQ, NULL);
            if (res != NO_ERR) {
                log_error("\n load module failed (%s)", 
                          get_error_string(res));
            } else {
                log_info("\n load OK");
            }

            modval = val_find_next_child(mgr_cli_valset, YANGCLI_MOD,
                                         NCX_EL_MODULE, modval);
        }
    }

    /* discard any deviations loaded from the CLI or conf file */
    ncx_clean_save_deviationsQ(&savedevQ);
    if (res != NO_ERR) {
        return res;
    }

    /* load the system (read-only) variables */
    res = init_system_vars(server_cb);
    if (res != NO_ERR) {
        return res;
    }

    /* load the system config variables */
    res = init_config_vars(server_cb);
    if (res != NO_ERR) {
        return res;
    }

    /* initialize the module library search result queue */
    {
        log_debug_t dbglevel = log_get_debug_level();
        if (LOGDEBUG3) {
            ; 
        } else {
            log_set_debug_level(LOG_DEBUG_NONE);
        }
        res = ncxmod_find_all_modules(&modlibQ);
        log_set_debug_level(dbglevel);
        if (res != NO_ERR) {
            return res;
        }
    }

    boolean file_error = FALSE;

    /* load the user aliases */
    if (autoaliases) {
        file_error = xml_strcmp(aliases_file, YANGCLI_DEF_ALIASES_FILE);

        /* don't cause an error for the default aliases file */
        res = load_aliases(aliases_file, file_error);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* load the user variables */
    if (autouservars) {
        file_error = xml_strcmp(uservars_file, YANGCLI_DEF_USERVARS_FILE);

        res = load_uservars(server_cb, uservars_file, file_error);
        if (res != NO_ERR) {
            /* keep going! do not exit if an error in the uservars */
            log_error("\nError: load uservars file failed (%s)\n",
                      get_error_string(res));
        }
    }

    /* load the user configured sessions */
    if (autosessions) {
        boolean fil_err = (server_cb->session_cfg_file != NULL);
        res = load_sessions(server_cb, server_cb->session_cfg_file, fil_err);
        if ( (res != NO_ERR) && (res != ERR_NCX_CANCELED) ) {
            /* keep going! do not exit if an error in the session-cfg */
            log_error("\nError: load configured sessions file failed (%s) \n",
                      get_error_string(res));
            res = NO_ERR;
        }
    }

    /* load the test suite config */
    if (autotest) {
        res = yangcli_ut_load(server_cb, testsuite_file,
                              !testsuite_file_default);
        if (res != NO_ERR) {
            /* keep going! do not exit if an error in the test-suite */
            log_error("\nError: load test-suite file failed (%s) \n",
                      get_error_string(res));
            res = NO_ERR;
        } else {
            testsuite_file_opened = TRUE;
        }
    }

    /* if this is SERVER mode then automatically start the
     * session session directly over the <ncx-connect> socket
     */
    if (server_cb->program_mode == PROG_MODE_SERVER) {
        res = yangcli_server_connect(server_cb);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* make sure the startup screen is generated
     * before the auto-connect sequence starts
     */
    do_startup_screen(server_cb->program_mode);

    /* check to see if a session should be auto-started
     * --> if the server parameter is set a connect will
     * --> be attempted
     *
     * The yangcli_stdin_handler will call the finish_start_session
     * function when the user enters a line of keyboard text
     */
    if (program_mode == PROG_MODE_CLIENT) {
        session_cb->state = MGR_IO_ST_IDLE;
        val_value_t *parm = 
            val_find_child(connect_valset, YANGCLI_MOD, YANGCLI_SERVER);
        if (parm && parm->res == NO_ERR) {
            res = do_connect(server_cb, NULL, NULL, 0, TRUE);
            if (res != NO_ERR) {
                if (!batchmode) {
                    res = NO_ERR;
                }
            }
        }
    }

    int retval = gl_inactivity_timeout(server_cb->cli_gl,
                                       get_line_timeout,
                                       cur_server_cb, 1, 0);
    if (retval != 0) {
        log_error("\nError: cannot set GL inactivity timeout\n");
        res = ERR_INTERNAL_VAL;
    }

    init_done = (res == NO_ERR) ? TRUE : FALSE;

    return res;

}  /* yangcli_init */


/********************************************************************
 * FUNCTION yangcli_cleanup
 * 
 * 
 * 
 *********************************************************************/
void
    yangcli_cleanup (void)
{
    server_cb_t  *server_cb;
    modptr_t     *modptr;
    status_t      res = NO_ERR;

    log_debug2("\nShutting down yangcli-pro\n");

    /* save the user variables */
    if (autouservars && init_done) {
         /* If uservars are changed, try to save them to the default file. */
        if ( get_yangcli_param_change_flag (USERVARS_FILE) == TRUE ) {
            /* If uservars are changed, try to save them to the default file. */
            if (cur_server_cb) {
                res = save_uservars(cur_server_cb, uservars_file);
            } else {
                res = ERR_NCX_MISSING_PARM;
            }
            if ( (res != NO_ERR) && (res != ERR_NCX_CANCELED) ) {
                log_error("\nError: yangcli-pro user variables could "
                      "not be saved (%s)\n", get_error_string(res));
            }
        }else {
            /*
             * If there is no change of user vars, and other client modified
             * the default file, the warning will be displayed at this exit.
             */
            if ( check_def_file_mtime_changed(USERVARS_FILE) == TRUE ) {
                if (LOGDEBUG4) {
                    log_warn("\nWarning: Uservars are not saved in %s. "
                        "Another client "
                        "has modified this file.\n", uservars_file);
                }
            }
        }
    }

    /* save the user aliases */
    if (autoaliases && init_done) {
        if ( get_yangcli_param_change_flag (ALIASES_FILE) == TRUE ) {
           /* If alieases are changed, try to save them to the default file. */
           res = save_aliases(aliases_file);
           if ( (res != NO_ERR) && (res != ERR_NCX_CANCELED) ) {
               log_error("\nError: yangcli-pro command aliases could "
                         "not be saved (%s)\n", get_error_string(res));
           }
        }else {
           /*
            * If there are no aliases change, but other client modified
            * the default file, the warning will be displayed at this
            * client's exit.
            */
           if ( check_def_file_mtime_changed(ALIASES_FILE) == TRUE ) {
               if (LOGDEBUG4) {
                   log_warn("\nWarning: Aliases are not saved in %s. "
                        "Another client "
                        "has modified this file.\n", aliases_file);
               }
           }
        }
    }

    /* save the test suites */
    if (autotest && init_done && testsuite_file_opened) {
        if (get_yangcli_param_change_flag (TESTSUITE_FILE) == TRUE ) {
            if (cur_server_cb) {
                res = yangcli_ut_save(cur_server_cb, testsuite_file);
             } else {
                res = ERR_NCX_MISSING_PARM;
             }

             if ( (res != NO_ERR) && (res != ERR_NCX_CANCELED) ) {
                 log_error("\nError: yangcli-pro test suites could "
                      "not be saved (%s)\n", get_error_string(res));
             }
         } else {
             /*
              * If there are no change, but other client modified
              * the default file, the warning will be displayed at this
              * client's exit.
              */
              if ( check_def_file_mtime_changed(TESTSUITE_FILE) == TRUE ) {
                  if (LOGDEBUG4) {
                       log_warn("\nWarning: TESTSUITES are not saved in %s. "
                           "Another client "
                           "has modified this file.\n", aliases_file);
                  }
              }
         }
    }

    while (!dlq_empty(&mgrloadQ)) {
        modptr = (modptr_t *)dlq_deque(&mgrloadQ);
        free_modptr(modptr);
    }

    while (!dlq_empty(&server_cbQ)) {
        server_cb = (server_cb_t *)dlq_deque(&server_cbQ);
        free_server_cb(server_cb);
    }

    val_free_value(mgr_cli_valset);
    mgr_cli_valset = NULL;
    val_free_value(connect_valset);
    connect_valset = NULL;
    m__free(default_module);
    default_module = NULL;
    m__free(confname);
    confname = NULL;
    m__free(runscript);
    runscript = NULL;
    m__free(runcommand);
    runcommand = NULL;

    /* cleanup the autoconfig module */
    autoconfig_cleanup();

    /* cleanup the notification callback handler module */
    yangcli_notif_cleanup();

    /* Cleanup the Netconf Server Library */
    mgr_cleanup();

    /* free this after the mgr_cleanup in case any session is active
     * and needs to be cleaned up before this main temp_progcb is freed
     */
    if (temp_progcb) {
        ncxmod_free_program_tempdir(temp_progcb);
        temp_progcb = NULL;
    }

    /* cleanup the module library search results */
    ncxmod_clean_search_result_queue(&modlibQ);

    free_aliases();
    ncx_clear_temp_modQ();

    ncx_reset_modQ();

    /* cleanup the NCX engine and registries */
    ncx_cleanup();

}  /* yangcli_cleanup */


/********************************************************************
* FUNCTION update_session_cb_vars
* 
*  Update the session user preference vars
* 
* INPUTS:
*    session_cb == session control block to use
*
* OUTPUTS: 
*     session_cb->foo is updated if it is a shadow of a global var
*
*********************************************************************/
void
    update_session_cb_vars (session_cb_t *session_cb)
{
    session_cb->baddata = baddata;
    session_cb->log_level = log_get_debug_level();
    session_cb->autoload = autoload;
    session_cb->fixorder = fixorder;
    session_cb->testoption = testoption;
    session_cb->erroption = erroption;
    session_cb->defop = defop;
    session_cb->timeout = default_timeout;
    session_cb->display_mode = display_mode;
    session_cb->withdefaults = withdefaults;
    session_cb->prompt_type = prompt_type;
    session_cb->defindent = defindent;
    session_cb->echo_notif_loglevel = echo_notif_loglevel;
    session_cb->echo_notifs = echo_notifs;
    session_cb->echo_replies = echo_replies;
    session_cb->script_input = script_input;
    session_cb->time_rpcs = time_rpcs;
    session_cb->time_rpcs_stats = time_rpcs_stats;
    session_cb->time_rpcs_stats_file = time_rpcs_stats_file;
    session_cb->match_names = match_names;
    session_cb->alt_names = alt_names;
    session_cb->use_xmlheader = use_xmlheader;
    session_cb->config_edit_mode = config_edit_mode;
    session_cb->autoconfig = autoconfig;
    session_cb->autonotif = autonotif;

}  /* update_session_cb_vars */


/********************************************************************
* FUNCTION clean_session_cb_vars
* 
*  Clean the session state variables
* 
* INPUTS:
*    session_cb == session control block to use
*
*********************************************************************/
void
    clean_session_cb_vars (session_cb_t *session_cb)
{
    session_cb->autoconfig_done = FALSE;
    session_cb->autonotif_done = FALSE;
    val_free_value(session_cb->config_tree);
    session_cb->config_tree = NULL;
    val_free_value(session_cb->config_etree);
    session_cb->config_etree = NULL;
    session_cb->config_tstamp[0] = 0;
    session_cb->config_full = TRUE;
    session_cb->config_mode = FALSE;
    session_cb->config_edit_dirty = FALSE;
    m__free(session_cb->config_path);
    session_cb->config_path = NULL;

} /* clean_session_cb_vars */


/********************************************************************
* FUNCTION get_cur_server_cb
* 
*  Get the current server control block
* 
* RETURNS:
*  pointer to current server control block or NULL if none
*********************************************************************/
server_cb_t *
    get_cur_server_cb (void)
{
    return cur_server_cb;
}

/********************************************************************
 * FUNCTION new_config_edit
 *
 * Create a new config_edit struct
 *
 * INPUTS:
 *   editop == editop to use
 *   etree == edit tree to use (will be consumed!!!)
 *
 * RETURNS:
 *   pointer to new edit
 *********************************************************************/
config_edit_t *
    new_config_edit (op_editop_t editop,
                     val_value_t *etree)
{
    config_edit_t *edit = m__getObj(config_edit_t);
    if (edit == NULL) {
        return NULL;
    }
    memset(edit, 0x0, sizeof(config_edit_t));
    edit->edit_op = editop;
    edit->edit_payload = etree;
    return edit;

} /* new_config_edit */


/********************************************************************
 * FUNCTION free_config_edit
 *
 * Free a config_edit struct
 *
 * INPUTS:
 *   edit == struct to free
 *
 *********************************************************************/
void
    free_config_edit (config_edit_t *edit)
{
    if (edit == NULL) {
        return;
    }
    val_free_value(edit->edit_payload);
    m__free(edit);

} /* free_config_edit */


/********************************************************************
 * FUNCTION get_program_mode
* 
*  Get the program_mode value
* 
* RETURNS:
*    program_mode value
*********************************************************************/
program_mode_t
    get_program_mode (void)
{
    return program_mode;
}  /* get_program_mode */


/********************************************************************
 * FUNCTION register_help_action
* 
*  Register the help mode action handler
* 
* INPUTS:
*  server_cb == server control block to use
* RETURNS:
*    status
*********************************************************************/
status_t register_help_action (server_cb_t *server_cb)
{
    /* setup CLI context-sensitive help mode */
    int retval = gl_register_action(server_cb->cli_gl,
                                    &server_cb->completion_state,
                                    yangcli_help_callback,
                                    HELPMODE_KEY_NAME,
                                    HELPMODE_KEY_BINDING);
    if (retval != 0) {
        log_error("\nError: cannot set libtecla help mode key binding\n");
        return ERR_NCX_OPERATION_FAILED;
    }
    return NO_ERR;

}  /* register_help_action */


/********************************************************************
 * FUNCTION unregister_help_action
* 
*  Unregister the help mode action handler
* 
* INPUTS:
*  server_cb == server control block to use
* RETURNS:
*    status
*********************************************************************/
status_t unregister_help_action (server_cb_t *server_cb)
{
    /* setup CLI context-sensitive help mode */
    int retval = gl_unregister_action(server_cb->cli_gl,
                                      HELPMODE_KEY_NAME,
                                      HELPMODE_KEY_BINDING);
    if (retval != 0) {
        log_error("\nError: cannot unregister libtecla help "
                  "mode key binding\n");
        return ERR_NCX_OPERATION_FAILED;
    }
    return NO_ERR;

}  /* unregister_help_action */

#ifdef EVAL_VERSION
/********************************************************************
 * FUNCTION check_eval_command_limit
* 
*  Check if the command limit is reached.
* 
* RETURNS:
*    status
*********************************************************************/
status_t check_eval_command_limit (void)
{
    if (command_count++ > YANGCLI_EVAL_COMMAND_LIMIT) {
        log_error("\nError: yangcli-pro evaluation command limit "
                  "exceeded. Use 'quit' to exit.\n");
        return ERR_NCX_RESOURCE_DENIED;
    } else {
        return NO_ERR;
    }

}  /* check_eval_command_limit */
#endif  // EVAL_VERSION


/* END yangcli.c */

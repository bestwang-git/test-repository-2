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
#ifndef _H_yangcli
#define _H_yangcli

/*  FILE: yangcli.h
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
27-mar-09    abb      Begun; moved from yangcli.c

*/

#include <time.h>
//#include <sys/time.h>
#include <xmlstring.h>
#include "libtecla.h"

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_mgr_io
#include "mgr_io.h"
#endif

#ifndef _H_mgr_rpc
#include "mgr_rpc.h"
#endif

#ifndef _H_runstack
#include "runstack.h"
#endif

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/
#define PROGNAME   (const xmlChar *)"yangcli-pro"

#define YP_PROGNAME (const xmlChar *)"yp-shell"

#define DEF_PROMPT     (const xmlChar *)"> "
#define DEF_FALSE_PROMPT  (const xmlChar *)"[F]> "
#define MORE_PROMPT    (const xmlChar *)"   more> "
#define FALSE_PROMPT   (const xmlChar *)"[F]"

/* command name for libtecla file to override binding */
#define HELPMODE_KEY_NAME      "helpmode"

/* help mode key binding is question mark */
#define HELPMODE_KEY_BINDING   "?"

#define MAX_PROMPT_LEN 72

#define YANGCLI_MAX_NEST  16

#define YANGCLI_MAX_RUNPARMS 9

#define YANGCLI_LINELEN   4095

/* 8K CLI buffer per server session */
#define YANGCLI_BUFFLEN  8192

#define YANGCLI_HISTLEN  4095

#define YANGCLI_MOD  (const xmlChar *)"yangcli-pro"

#define YANGCLI_NUM_TIMERS    16

/* look in yangcli.c:yangcli_init for defaults not listed here */
#define YANGCLI_DEF_HISTORY_FILE  \
    (const xmlChar *)"~/.yumapro/.yangcli_pro_history"

#define YANGCLI_DEF_ALIASES_FILE  \
    (const xmlChar *)"~/.yumapro/.yangcli_pro_aliases"

#define YANGCLI_DEF_USERVARS_FILE \
    (const xmlChar *)"~/.yumapro/yangcli_pro_uservars.xml"

#define YANGCLI_DEF_SESSIONS_FILE \
    (const xmlChar *)"~/.yumapro/.yangcli_pro_sessions.conf"

#define YANGCLI_DEF_TESTSUITE_FILE \
    (const xmlChar *)"~/.yumapro/yangcli_pro_tests.conf"

#define YANGCLI_DEF_TIME_RPCS_STATS_FILE \
    (const xmlChar *)"~/yangcli_pro_rpc_stats.txt"

#define YANGCLI_DEF_PUBLIC_KEY "$HOME/.ssh/id_rsa.pub"

#define YANGCLI_DEF_PRIVATE_KEY "$HOME/.ssh/id_rsa"

#define YANGCLI_DEF_TIMEOUT   30

#define YANGCLI_DEF_SERVER (const xmlChar *)"default"

#define YANGCLI_DEF_DISPLAY_MODE   NCX_DISPLAY_MODE_PLAIN

#define YANGCLI_DEF_FIXORDER   TRUE

#define YANGCLI_DEF_CONF_FILE \
    (const xmlChar *)"/etc/yumapro/yangcli-pro.conf"

#define YANGCLI_DEF_SERVER (const xmlChar *)"default"

#define YANGCLI_DEF_TEST_OPTION OP_TESTOP_SET

#define YANGCLI_DEF_ERROR_OPTION OP_ERROP_NONE

#define YANGCLI_DEF_DEFAULT_OPERATION OP_DEFOP_MERGE

#define YANGCLI_DEF_WITH_DEFAULTS  NCX_WITHDEF_NONE

#define YANGCLI_DEF_INDENT    2

#define YANGCLI_DEF_BAD_DATA NCX_BAD_DATA_CHECK

#define YANGCLI_DEF_MAXLOOPS  65535

#define YANGCLI_MAX_SESSIONS 1000

#define YANGCLI_DEF_HISTORY_LINES  25

#define YANGCLI_RECALL_CHAR  '!'

#define YANGCLI_DEF_CONFIG_MATCH_NAMES NCX_MATCH_FIRST_NOCASE

#define YANGCLI_DEF_CONFIG_ALT_NAMES   TRUE

#ifdef MACOSX
#define ENV_HOST        (const char *)"HOST"
#else
#define ENV_HOST        (const char *)"HOSTNAME"
#endif

#define ENV_SHELL       (const char *)"SHELL"
#define ENV_USER        (const char *)"USER"
#define ENV_LANG        (const char *)"LANG"

/* CLI parmset for the ncxcli application */
#define YANGCLI_BOOT YANGCLI_MOD

/* core modules auto-loaded at startup */
#define NCXDTMOD       (const xmlChar *)"yuma-types"
#define XSDMOD         (const xmlChar *)"yuma-xsd"

#define YESNO_NODEF  0
#define YESNO_CANCEL 0
#define YESNO_YES    1
#define YESNO_NO     2

/* YANGCLI boot and operation parameter names 
 * matches parm clauses in yangcli container in yangcli-pro.yang
 */

#define YANGCLI_ALIASES_FILE  (const xmlChar *)"aliases-file"
#define YANGCLI_APPEND      (const xmlChar *)"append"
#define YANGCLI_AUTOALIASES (const xmlChar *)"autoaliases"
#define YANGCLI_AUTOCOMP    (const xmlChar *)"autocomp"
#define YANGCLI_AUTOCONFIG  (const xmlChar *)"autoconfig"
#define YANGCLI_AUTOHISTORY (const xmlChar *)"autohistory"
#define YANGCLI_AUTOLOAD    (const xmlChar *)"autoload"
#define YANGCLI_AUTONOTIF   (const xmlChar *)"autonotif"
#define YANGCLI_AUTOSESSIONS (const xmlChar *)"autosessions"
#define YANGCLI_AUTOTEST    (const xmlChar *)"autotest"
#define YANGCLI_AUTO_TEST    (const xmlChar *)"auto-test"
#define YANGCLI_AUTOUSERVARS (const xmlChar *)"autouservars"
#define YANGCLI_BADDATA     (const xmlChar *)"bad-data"
#define YANGCLI_BATCHMODE   (const xmlChar *)"batch-mode"
#define YANGCLI_BRIEF       (const xmlChar *)"brief"
#define YANGCLI_CLEANUP     (const xmlChar *)"cleanup"
#define YANGCLI_CLEAR       (const xmlChar *)"clear"
#define YANGCLI_CLI         (const xmlChar *)"cli"
#define YANGCLI_COMMAND     (const xmlChar *)"command"
#define YANGCLI_COMMANDS    (const xmlChar *)"commands"
#define YANGCLI_CONFIG      (const xmlChar *)"config"
#define YANGCLI_CONFIG_EDIT_MODE (const xmlChar *)"config-edit-mode"
#define YANGCLI_CONNECT_ALL  (const xmlChar *)"connect-all"
#define YANGCLI_COUNT       (const xmlChar *)"count"
#define YANGCLI_DEF_MODULE  (const xmlChar *)"default-module"
#define YANGCLI_DELETE      (const xmlChar *)"delete"
#define YANGCLI_DELETE_TEST  (const xmlChar *)"delete-test"
#define YANGCLI_DELTA       (const xmlChar *)"delta"
#define YANGCLI_DIR         (const xmlChar *)"dir"
#define YANGCLI_DISPLAY_MODE  (const xmlChar *)"display-mode"
#define YANGCLI_ECHO        (const xmlChar *)"echo"
#define YANGCLI_ECHO_NOTIF_LOGLEVEL (const xmlChar *)"echo-notif-loglevel"
#define YANGCLI_ECHO_NOTIFS (const xmlChar *)"echo-notifs"
#define YANGCLI_ECHO_REPLIES (const xmlChar *)"echo-replies"
#define YANGCLI_EDIT_TARGET (const xmlChar *)"edit-target"
#define YANGCLI_ERROR_OPTION (const xmlChar *)"error-option"
#define YANGCLI_EXIT        (const xmlChar *)"exit"
#define YANGCLI_FILES       (const xmlChar *)"files"
#define YANGCLI_FIXORDER    (const xmlChar *)"fixorder"
#define YANGCLI_FROM_CLI    (const xmlChar *)"from-cli"
#define YANGCLI_FORCE_TARGET (const xmlChar *)"force-target"
#define YANGCLI_FULL        (const xmlChar *)"full"
#define YANGCLI_GLOBAL      (const xmlChar *)"global"
#define YANGCLI_GLOBALS     (const xmlChar *)"globals"
#define YANGCLI_ID          (const xmlChar *)"id"
#define YANGCLI_INDEX       (const xmlChar *)"index"
#define YANGCLI_ITERATIONS  (const xmlChar *)"iterations"
#define YANGCLI_LEVEL       (const xmlChar *)"level"
#define YANGCLI_LINE        (const xmlChar *)"line"
#define YANGCLI_LOAD        (const xmlChar *)"load"
#define YANGCLI_LOCK_TIMEOUT (const xmlChar *)"lock-timeout"
#define YANGCLI_LOCAL       (const xmlChar *)"local"
#define YANGCLI_LOCALS      (const xmlChar *)"locals"
#define YANGCLI_MANUAL      (const xmlChar *)"manual"
#define YANGCLI_MODE        (const xmlChar *)"mode"
#define YANGCLI_MODULE      (const xmlChar *)"module"
#define YANGCLI_MODULES     (const xmlChar *)"modules"
#define YANGCLI_NCPORT      (const xmlChar *)"ncport"
#define YANGCLI_NOFILL      (const xmlChar *)"nofill"
#define YANGCLI_NORMAL      (const xmlChar *)"normal"
#define YANGCLI_OBJECTS     (const xmlChar *)"objects"
#define YANGCLI_OIDS        (const xmlChar *)"oids"
#define YANGCLI_OPERATION   (const xmlChar *)"operation"
#define YANGCLI_OPTIONAL    (const xmlChar *)"optional"
#define YANGCLI_ORDER       (const xmlChar *)"order"
#define YANGCLI_PASSWORD    (const xmlChar *)"password"
#define YANGCLI_PROMPT_TYPE (const xmlChar *)"prompt-type"
#define YANGCLI_PROTOCOLS   (const xmlChar *)"protocols"
#define YANGCLI_PRIVATE_KEY (const xmlChar *)"private-key"
#define YANGCLI_PUBLIC_KEY  (const xmlChar *)"public-key"
#define YANGCLI_RECORD_TEST  (const xmlChar *)"record-test"
#define YANGCLI_RECORD_CANCEL (const xmlChar *)"cancel"
#define YANGCLI_RECORD_FINISH (const xmlChar *)"finish"
#define YANGCLI_RECORD_PAUSE (const xmlChar *)"pause"
#define YANGCLI_RECORD_START (const xmlChar *)"start"
#define YANGCLI_RECORD_RESUME (const xmlChar *)"resume"
#define YANGCLI_RECORD_SUITENAME (const xmlChar *)"suite-name"
#define YANGCLI_RECORD_TESTNAME (const xmlChar *)"test-name"
#define YANGCLI_RESTART_OK  (const xmlChar *)"restart-ok"
#define YANGCLI_RETRY_INTERVAL (const xmlChar *)"retry-interval"
#define YANGCLI_RUN_ALL  (const xmlChar *)"run-all"
#define YANGCLI_RUN_COMMAND (const xmlChar *)"run-command"
#define YANGCLI_RUN_SCRIPT  (const xmlChar *)"run-script"
#define YANGCLI_START       (const xmlChar *)"start"
#define YANGCLI_SCRIPT_INPUT (const xmlChar *)"script-input"
#define YANGCLI_SCRIPTS     (const xmlChar *)"scripts"
#define YANGCLI_SERVER      (const xmlChar *)"server"
#define YANGCLI_SESSION    (const xmlChar *)"session"
#define YANGCLI_SESSION_CFG (const xmlChar *)"session-cfg"
#define YANGCLI_SESSIONS_CFG (const xmlChar *)"sessions-cfg"
#define YANGCLI_SESSION_NAME  (const xmlChar *)"session-name"
#define YANGCLI_START_SESSION (const xmlChar *)"start-session"
#define YANGCLI_START_RPC_TIMING (const xmlChar *)"start-rpc-timing"
#define YANGCLI_STDOUT (const xmlChar *)"stdout"
#define YANGCLI_STOP_SESSION (const xmlChar *)"stop-session"
#define YANGCLI_STOP_RPC_TIMING (const xmlChar *)"stop-rpc-timing"
#define YANGCLI_SYSTEM      (const xmlChar *)"system"
#define YANGCLI_TERM        (const xmlChar *)"term"
#define YANGCLI_TEST_OPTION (const xmlChar *)"test-option"
#define YANGCLI_TEST_SUITE (const xmlChar *)"test-suite"
#define YANGCLI_TEST_SUITE_FILE (const xmlChar *)"test-suite-file"
#define YANGCLI_TIMEOUT     (const xmlChar *)"timeout"
#define YANGCLI_TIME_RPCS   (const xmlChar *)"time-rpcs"
#define YANGCLI_TIME_RPCS_STATS (const xmlChar *)"time-rpcs-stats"
#define YANGCLI_TIME_RPCS_STATS_FILE (const xmlChar *)"time-rpcs-stats-file"
#define YANGCLI_TRANSPORT   (const xmlChar *)"transport"
#define YANGCLI_UPDATE_CONFIG (const xmlChar *)"update-config"
#define YANGCLI_USE_XMLHEADER (const xmlChar *)"use-xmlheader"
#define YANGCLI_USER        (const xmlChar *)"user"
#define YANGCLI_USERVARS_FILE  (const xmlChar *)"uservars-file"
#define YANGCLI_VALUE       (const xmlChar *)"value"
#define YANGCLI_VAR         (const xmlChar *)"var"
#define YANGCLI_VARREF      (const xmlChar *)"varref"
#define YANGCLI_VARS        (const xmlChar *)"vars"
#define YANGCLI_VARTYPE     (const xmlChar *)"vartype"
#define YANGCLI_WITH_DEFAULTS  (const xmlChar *)"with-defaults"

/* YANGCLI local RPC commands */
#define YANGCLI_ALIAS   (const xmlChar *)"alias"
#define YANGCLI_ALIASES (const xmlChar *)"aliases"
#define YANGCLI_CD      (const xmlChar *)"cd"
#define YANGCLI_CONNECT (const xmlChar *)"connect"
#define YANGCLI_CREATE  (const xmlChar *)"create"
#define YANGCLI_DELETE  (const xmlChar *)"delete"
#define YANGCLI_ELSE    (const xmlChar *)"else"
#define YANGCLI_ELIF    (const xmlChar *)"elif"
#define YANGCLI_END     (const xmlChar *)"end"
#define YANGCLI_EVAL    (const xmlChar *)"eval"
#define YANGCLI_EVENTLOG (const xmlChar *)"eventlog"
#define YANGCLI_FILL    (const xmlChar *)"fill"
#define YANGCLI_GET_LOCKS (const xmlChar *)"get-locks"
#define YANGCLI_HELP    (const xmlChar *)"help"
#define YANGCLI_HISTORY (const xmlChar *)"history"
#define YANGCLI_INSERT  (const xmlChar *)"insert"
#define YANGCLI_IF      (const xmlChar *)"if"
#define YANGCLI_LIST    (const xmlChar *)"list"
#define YANGCLI_LOG_ERROR (const xmlChar *)"log-error"
#define YANGCLI_LOG_WARN (const xmlChar *)"log-warn"
#define YANGCLI_LOG_INFO (const xmlChar *)"log-info"
#define YANGCLI_LOG_DEBUG (const xmlChar *)"log-debug"
#define YANGCLI_MERGE   (const xmlChar *)"merge"
#define YANGCLI_MGRLOAD (const xmlChar *)"mgrload"
#define YANGCLI_PWD     (const xmlChar *)"pwd"
#define YANGCLI_QUIT    (const xmlChar *)"quit"
#define YANGCLI_RECALL  (const xmlChar *)"recall"
#define YANGCLI_RELEASE_LOCKS (const xmlChar *)"release-locks"
#define YANGCLI_REMOVE  (const xmlChar *)"remove"
#define YANGCLI_REPLACE (const xmlChar *)"replace"
#define YANGCLI_RUN     (const xmlChar *)"run"
#define YANGCLI_SAVE    (const xmlChar *)"save"
#define YANGCLI_SET     (const xmlChar *)"set"
#define YANGCLI_SGET    (const xmlChar *)"sget"
#define YANGCLI_SGET_CONFIG   (const xmlChar *)"sget-config"
#define YANGCLI_SHOW    (const xmlChar *)"show"
#define YANGCLI_START_TIMER  (const xmlChar *)"start-timer"
#define YANGCLI_STOP_TIMER  (const xmlChar *)"stop-timer"
#define YANGCLI_TEST_SUITE  (const xmlChar *)"test-suite"
#define YANGCLI_WHILE   (const xmlChar *)"while"
#define YANGCLI_XGET    (const xmlChar *)"xget"
#define YANGCLI_XGET_CONFIG   (const xmlChar *)"xget-config"
#define YANGCLI_UNSET   (const xmlChar *)"unset"
#define YANGCLI_USERVARS  (const xmlChar *)"uservars"



/* specialized prompts for the fill command */
#define YANGCLI_PR_LLIST (const xmlChar *)"Add another leaf-list?"
#define YANGCLI_PR_LIST (const xmlChar *)"Add another list?"

/* retry for a lock request once per second */
#define YANGCLI_RETRY_INTERNVAL   1

#define YANGCLI_EXPR    (const xmlChar *)"expr"
#define YANGCLI_DOCROOT (const xmlChar *)"docroot"
#define YANGCLI_CONTEXT (const xmlChar *)"context"

#define YANGCLI_MSG      (const xmlChar *)"msg"
#define YANGCLI_MAXLOOPS (const xmlChar *)"maxloops"

#define YANGCLI_MAX_INDENT    9

#define YANGCLI_TEST_MOD (const xmlChar *)"yumaworks-test"
#define YANGCLI_TEST_SUITES (const xmlChar *)"test-suites"

#define YANGCLI_SAVED_SESSIONS  (const xmlChar *)"saved-sessions"
#define YANGCLI_START_COMMANDS  (const xmlChar *)"start-commands"
#define YANGCLI_SET_CURRENT     (const xmlChar *)"set-current"
#define YANGCLI_MEMORY          (const xmlChar *)"memory"
#define YANGCLI_STATFILE        (const xmlChar *)"statfile"


#define YANGCLI_CONFIG_MODE      (const xmlChar *)"(c)"
#define YANGCLI_CONFIG_START     (const xmlChar *)"(c "

#define YANGCLI_EVAL_COMMAND_LIMIT      10

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* cache the module pointers known by a particular server,
 * as reported in the session <hello> message
 */
typedef struct modptr_t_ {
    dlq_hdr_t            qhdr;
    ncx_module_t         *mod;               /* back-ptr, not live */
    ncx_list_t           *feature_list;      /* back-ptr, not live */
    ncx_list_t           *deviation_list;    /* back-ptr, not live */
} modptr_t;


/* save the requested result format type */
typedef enum result_format_t {
    RF_NONE,
    RF_TEXT,
    RF_XML,
    RF_JSON
} result_format_t;


/* command state enumerations for each situation
 * where the tecla get_line function is called
 */
typedef enum command_state_t {
    CMD_STATE_NONE,
    CMD_STATE_FULL,
    CMD_STATE_GETVAL,
    CMD_STATE_YESNO,
    CMD_STATE_MORE
} command_state_t;


/* command mode enumerations */
typedef enum command_mode_t {
    CMD_MODE_NONE,
    CMD_MODE_NORMAL,
    CMD_MODE_AUTOLOAD,
    CMD_MODE_AUTOLOCK,
    CMD_MODE_AUTOUNLOCK,
    CMD_MODE_AUTODISCARD,
    CMD_MODE_AUTOCONFIG,
    CMD_MODE_AUTONOTIF,
    CMD_MODE_AUTOTEST,
    CMD_MODE_CONF_APPLY,
    CMD_MODE_SAVE
} command_mode_t;

/*  yangcli files  */
typedef enum yangcli_file_t {
    HISTORY_FILE,
    ALIASES_FILE,
    USERVARS_FILE,
    SESSIONS_FILE,
    TESTSUITE_FILE,
    TIME_RPCS_STATS_FILE
} yangcli_file_t;

/* yangcli config mode edit mode */
typedef enum config_edit_mode_t_ {
    CFG_EDITMODE_NONE,
    CFG_EDITMODE_LINE,
    CFG_EDITMODE_LEVEL,
    CFG_EDITMODE_MANUAL
} config_edit_mode_t;

/* save the context-sensitve help action type */
typedef enum help_action_t_ {
    HELP_ACTION_NONE,
    HELP_ACTION_SHOW_LINES,
    HELP_ACTION_SHOW_NODE
} help_action_t;

    
/* saved state for libtecla command line completion */
typedef struct completion_state_t_ {
    dlq_hdr_t              help_backptrQ;
    obj_template_t        *cmdobj;
    obj_template_t        *cmdinput;
    obj_template_t        *cmdcurparm;
    obj_key_t             *curkey;
    struct server_cb_t_   *server_cb;
    ncx_module_t          *cmdmodule;
    GetLine               *gl;
    ncx_node_t             help_backptr_type;
    help_action_t          help_action;
    command_state_t        cmdstate;
    boolean                assignstmt;
    boolean                config_done;
    boolean                keys_done;
    boolean                no_command;
    boolean                do_command;
    boolean                help_mode;
    boolean                gl_normal_done;
    boolean                gl_unregister_done;
} completion_state_t;


/* save the server lock state for get-locks and release-locks */
typedef enum lock_state_t {
    LOCK_STATE_NONE,
    LOCK_STATE_IDLE,
    LOCK_STATE_REQUEST_SENT,
    LOCK_STATE_TEMP_ERROR,
    LOCK_STATE_FATAL_ERROR,
    LOCK_STATE_ACTIVE,
    LOCK_STATE_RELEASE_SENT,
    LOCK_STATE_RELEASED
} lock_state_t;


/* config mode: save each edit in case they are not applied at once */
typedef struct config_edit_t_ {
    dlq_hdr_t   qhdr;
    val_value_t *edit_payload;  /* start at top-level node */
    op_editop_t  edit_op;   /* op at target */
} config_edit_t;


/* program mode: client or server */
typedef enum program_mode_t {
    PROG_MODE_NONE,
    PROG_MODE_CLIENT,
    PROG_MODE_SERVER
} program_mode_t;


/* autolock control block, used by get-locks, release-locks */
typedef struct lock_cb_t_ {
    ncx_cfg_t             config_id;
    const xmlChar        *config_name;
    lock_state_t          lock_state;
    boolean               lock_used;
    time_t                start_time;
    time_t                last_msg_time;
} lock_cb_t;


/* yangcli command alias control block */
typedef struct alias_cb_t_ {
    dlq_hdr_t           qhdr;
    xmlChar            *name;
    xmlChar            *value;
    uint8               quotes;
}  alias_cb_t;


/* unit test state enumerations */
typedef enum ut_state_t {
    UT_STATE_NONE,
    UT_STATE_INIT,
    UT_STATE_READY,
    UT_STATE_SETUP,
    UT_STATE_RUNTEST,
    UT_STATE_CLEANUP,
    UT_STATE_DONE,
    UT_STATE_RECORD_PAUSE,
    UT_STATE_RECORD_IN_PROGRESS,
    UT_STATE_ERROR
} ut_state_t;

/* RPC result type */
typedef enum data_response_type_t_ {
    UT_RPC_DATA_NONE, // not set
    UT_RPC_ANY_DATA,
    UT_RPC_DATA_EMPTY,
    UT_RPC_DATA_NON_EMPTY,
    UT_RPC_DATA_MATCH,
} data_response_type_t;

/* RPC result type */
typedef enum response_type_t_ {
    UT_RPC_NONE, // not set
    UT_RPC_NO,   // enum 'none'
    UT_RPC_ANY,  // leaf not present; any allowed
    UT_RPC_OK,
    UT_RPC_DATA,
    UT_RPC_ERROR,
} response_type_t;

/* test record operation enumerations */
typedef enum test_record_op_ {
    TEST_RECORD_START,
    TEST_RECORD_RESUME,
    TEST_RECORD_PAUSE,
    TEST_RECORD_CANCEL,
    TEST_RECORD_FINISH
} test_record_op;

/* yangcli raw line */
typedef struct rawline_t_ {
    dlq_hdr_t           qhdr;
    xmlChar            *line;
}  rawline_t;

/* The ordered list of test names to run in this test suite.
 * Used for a test name pointer in run_test_leaflistQ.
 */
typedef struct run_test_t_ {
    dlq_hdr_t    qhdr;
    xmlChar     *run_test_name;

    /* state data saved for each test-run in test */
    struct yangcli_ut_test_t_ *test;
} run_test_t;


/* The names of the tests that have already 
 *  been run and passed.
 *  Used for mustpass_leaflistQ. 
 */
typedef struct mustpass_t_ {
    dlq_hdr_t    qhdr;
    xmlChar     *mustpass_name;

    /* state data saved for each test-run */
    boolean      checked;
    boolean      passed;
} mustpass_t;


/*  The result-error-info of test's step
 *  must "../result-type = 'error'";
 *  The error-info element name expected if the result-type
 *  is 'error'.
 */
typedef struct result_error_info_t_ {
    dlq_hdr_t    qhdr;
    xmlChar     *result_error_info;
    boolean      found;
} result_error_info_t;


/* 1 test-suite in suite list q */
typedef struct yangcli_ut_suite_t_ {
    dlq_hdr_t   qhdr;
    xmlChar     *name;
    xmlChar     *description;
    dlq_hdr_t   setup_rawlineQ;        /* Q of rawline_t */
    dlq_hdr_t   cleanup_rawlineQ;      /* Q of rawline_t */
    dlq_hdr_t   run_test_leaflistQ;   /* Q of run_test_t */
    dlq_hdr_t   test_listQ;    /* q of yangcli_ut_test_t */

    /* state data saved for each test run */
    boolean     suite_started;
    boolean     suite_errors;
} yangcli_ut_suite_t;


/*  1 test in test_listQ of test-suite */
typedef struct yangcli_ut_test_t_ {
    dlq_hdr_t       qhdr;
    xmlChar         *test_name;
    xmlChar         *description;
    dlq_hdr_t       mustpass_leaflistQ;  /* Q of mustpass_t */
    dlq_hdr_t       step_listQ;   /* Q of yangcli_ut_step_t */

    /* state data saved for each test run */
    boolean        test_started;
    boolean        test_errors;

    /* step number used to record a test */
    int32          step_num_to_record;
} yangcli_ut_test_t;


/* 1 step in step_listQ */
typedef struct yangcli_ut_step_t_ {
    dlq_hdr_t       qhdr;
    xmlChar         *name;
    xmlChar         *description;
    xmlChar         *session_name;
    response_type_t result_type;
    xmlChar         *result_error_tag;
    xmlChar         *result_error_apptag;
    data_response_type_t result_data_type;
    xmlChar         *result_rpc_reply_data;

    /* Q of result_error_info_t */
    dlq_hdr_t       result_error_infoQ;

    xmlChar         *command;

    // TBD: will not be saved in string format
    //xmlChar         *anyxml_rpc_reply_data;

    /* state data saved for each test-run */
    xmlChar          start_tstamp[TSTAMP_MIN_SIZE];
    xmlChar          stop_tstamp[TSTAMP_MIN_SIZE];
    response_type_t  step_result;
    xmlChar         *step_error_tag;
    xmlChar         *step_error_apptag;
    boolean          step_done;
    boolean          step_result_wrong;
    boolean          step_timed_out;
    boolean          step_local_error;
    boolean          step_error_tag_wrong;
    boolean          step_error_apptag_wrong;
    boolean          step_error_info_wrong;
} yangcli_ut_step_t;


/* unit-testing control context */
typedef struct yangcli_ut_context_t_ {
    dlq_hdr_t        suite_listQ;       /* q of yangcli_ut_suite_t */

    /* state data saved for each test-run */
    ut_state_t       ut_state;
    boolean          ut_input_mode;
    status_t         ut_status;
    xmlChar         *logfile;              /* malloced filespec */
    boolean          logfile_open;
    xmlChar         *test_sesname;         /* malloced name string */
    xmlChar         *linebuff;             /* malloced line-splice */
    uint32           linebuff_size;
    boolean          single_suite;         /* one suite or all suites */

    /* following are back-ptrs into suite_listQ structures */
    yangcli_ut_suite_t *cur_suite;
    run_test_t         *cur_run_test;
    yangcli_ut_step_t  *cur_step;
    rawline_t          *cur_rawline;
    boolean            cur_step_record_done;
} yangcli_ut_context_t;


/* state machine for autotest edit proceduires */
typedef enum autotest_state_t_ {
    AUTOTEST_ST_NONE,
    AUTOTEST_ST_STARTED,
    AUTOTEST_ST_WAIT_EDIT,
    AUTOTEST_ST_WAIT_SAVE,
    AUTOTEST_ST_DONE
} autotest_state_t;


/* auto-test control block for running edit compliance and
 * performance testing  */
typedef struct autotest_cb_t_ {
    val_value_t      *editroot;
    val_value_t      *target_val;
    obj_template_t   *target_obj;
    uint32            count;
    uint32            iterations;
    uint32            cur_iteration;
    int               seed_iteration;
    autotest_state_t  state;
    status_t          res;
} autotest_cb_t;


/* session config struct to/from the sessions-file */
typedef struct session_cfg_t_ {
    dlq_hdr_t  qhdr;
    xmlChar   *name;
    xmlChar   *username;
    xmlChar   *password;     // NULL = prompt or not used
    char      *public_key;   // NULL = not used
    char      *private_key;  // NULL = not used
    xmlChar   *server_addr;
    uint16     server_port;
    uint16     server_protocols; //  bits
    dlq_hdr_t  rawlineQ;
} session_cfg_t;


/* YANGCLI context control block */
typedef struct server_cb_t_ {
    dlq_hdr_t            qhdr;
    xmlChar             *name;
    xmlChar             *address;
    xmlChar             *password;
    xmlChar             *publickey;
    xmlChar             *privatekey;
    val_value_t         *connect_valset; 

    /* assignment statement support */
    xmlChar             *result_name;
    var_type_t           result_vartype;
    xmlChar             *result_filename;
    result_format_t      result_format;
    val_value_t         *local_result; 

    /* context-specific user variables */
    dlq_hdr_t            varbindQ;   /* Q of ncx_var_t */

    /* concurrent sessions support */
    xmlChar             *session_cfg_file;
    boolean              session_cfg_file_opened;
    struct session_cb_t_ *cur_session_cb;
    boolean              autosessions;
    dlq_hdr_t            session_cfgQ;      /* Q of session_cfg_t */
    dlq_hdr_t            session_cbQ;      /* Q of session_cb_t */

    /* support for temp directory for downloaded modules */
    ncxmod_temp_progcb_t *temp_progcb;

    /* runstack context for script processing */
    runstack_context_t  *runstack_context;

    /* per-context timer support */
    struct timeval       timers[YANGCLI_NUM_TIMERS];

    /* 1 per-context unit-test support */
    yangcli_ut_context_t ut_context;

    /* 1 per-context record-test support */
    yangcli_ut_context_t tr_context;

    /* per-context CLI support */
    program_mode_t      program_mode;
    const xmlChar       *cli_fn;
    GetLine             *cli_gl;
    xmlChar             *history_filename;
    xmlChar             *history_line;
    boolean              history_line_active;
    boolean              history_auto;

    uint32               history_size;
    boolean              overwrite_filevars;
    completion_state_t   completion_state;
    boolean              get_optional;
    boolean              first_leaflist;
    boolean              climore;
    xmlChar              clibuff[YANGCLI_BUFFLEN];
} server_cb_t;


/* NETCONF session control block */
typedef struct session_cb_t_ {
    dlq_hdr_t            qhdr;
    session_cfg_t       *session_cfg;
    boolean              session_cfg_new;

    /* per-session shadows of global config vars */
    ncx_display_mode_t   display_mode;
    uint32               timeout;
    uint32               lock_timeout;
    ncx_bad_data_t       baddata;
    log_debug_t          log_level;
    boolean              autoload;
    boolean              fixorder;
    op_testop_t          testoption;
    op_errop_t           erroption;
    op_defop_t           defop;
    ncx_withdefaults_t   withdefaults;
    int32                defindent;
    boolean              echo_notifs;
    log_debug_t          echo_notif_loglevel;
    boolean              echo_replies;
    const xmlChar       *default_target;
    ncx_cfg_t            default_target_id;
    boolean              time_rpcs;
    boolean              time_rpcs_stats;
    boolean              time_rpcs_stats_active;
    const xmlChar       *time_rpcs_stats_file;
    xmlChar             *time_rpcs_stats_file_rpc;
    FILE                *time_rpcs_fp;
    ncx_name_match_t     match_names;
    boolean              alt_names;
    boolean              use_xmlheader;
    boolean              def_session;
    boolean              autoconfig;
    boolean              autoconfig_done;
    boolean              autonotif;
    boolean              autonotif_done;
    boolean              script_input;
    config_edit_mode_t   config_edit_mode;
    xmlChar             *autoconfig_saveline;

    /* session support */
    mgr_io_state_t       state;
    ses_id_t             mysid;
    mgr_io_returncode_t  returncode;
    int32                errnocode;
    command_mode_t       command_mode;
    command_mode_t       return_command_mode;
    help_mode_t          prompt_type;

    /* get-locks and release-locks support
     * there is one entry for each database
     * indexed by the ncx_cfg_t enum
     */
    boolean              locks_active;
    boolean              locks_waiting;
    ncx_cfg_t            locks_cur_cfg;
    uint32               locks_timeout;
    uint32               locks_retry_interval;
    boolean              locks_cleanup;
    time_t               locks_start_time;
    lock_cb_t            lock_cb[NCX_NUM_CFGS];

    /* contains structs for any deviation modules found
     * in the server <hello>  */
    dlq_hdr_t           deviationQ;    /* Q of ncx_save_deviations_t */

    /* contains only the modules that the server is using
     * plus the 'yuma-netconf.yang' module
     */
    dlq_hdr_t            modptrQ;     /* Q of modptr_t */

    /* contains received notifications */
    dlq_hdr_t            notificationQ;   /* Q of mgr_not_msg_t */

    /* session-specific user variables */
    dlq_hdr_t            varbindQ;   /* Q of ncx_var_t */

    /* before any server modules are loaded, all the
     * modules are checked out, and the results are stored in
     * this Q of ncxmod_search_result_t 
     */
    dlq_hdr_t            searchresultQ; /* Q of ncxmod_search_result_t */
    ncxmod_search_result_t  *cursearchresult;

    /* support for temp directory for downloaded modules */
    ncxmod_temp_sescb_t  *temp_sescb;

    xmlChar               start_time[TSTAMP_MIN_SIZE+1];

    /* aupport for autotest edit compliance and performance testing */
    autotest_cb_t       *autotest_cb;

    /* support for autoconfig retrieval mode */
    val_value_t         *config_tree;
    xmlChar              config_tstamp[TSTAMP_MIN_SIZE];
    boolean              config_full;
    boolean              config_update_supported;
    boolean              config_tree_dirty;

    /* support for session configure mode */
    boolean               config_mode;
    boolean               config_no_active;
    boolean               config_alt_names;
    boolean               config_edit_dirty;
    ncx_name_match_t      config_match_names;
    xmlChar              *config_path;    /* malloced */
    val_value_t          *config_curval;  /* backptr into config_tree */
    obj_template_t       *config_curobj;  /* backptr into ncx schema tree */
    obj_key_t            *config_curkey;  /* backptr into ncx schema tree */
    val_value_t          *config_etree;   /* malloced edit tree */
    val_value_t          *config_ecurval; /* backptr into config_etree */
    val_value_t          *config_estartval; /* backptr into config_etree */
    val_value_t          *config_firstnew;  /* backptr into config_etree */
    dlq_hdr_t             config_editQ;   /* Q of config_edit_t */
} session_cb_t;


/********************************************************************
* FUNCTION get_autocomp
* 
*  Get the autocomp parameter value
* 
* RETURNS:
*    autocomp boolean value
*********************************************************************/
extern boolean
    get_autocomp (void);


/********************************************************************
* FUNCTION get_autoload
* 
*  Get the autoload parameter value
* 
* RETURNS:
*    autoload boolean value
*********************************************************************/
extern boolean
    get_autoload (void);


/********************************************************************
* FUNCTION get_batchmode
* 
*  Get the batchmode parameter value
* 
* RETURNS:
*    batchmode boolean value
*********************************************************************/
extern boolean
    get_batchmode (void);


/********************************************************************
* FUNCTION get_default_module
* 
*  Get the default module
* 
* RETURNS:
*    default module value
*********************************************************************/
extern const xmlChar *
    get_default_module (void);


/********************************************************************
* FUNCTION get_runscript
* 
*  Get the runscript variable
* 
* RETURNS:
*    runscript value
*********************************************************************/
extern const xmlChar *
    get_runscript (void);


/********************************************************************
* FUNCTION get_baddata
* 
*  Get the baddata parameter
* 
* RETURNS:
*    baddata enum value
*********************************************************************/
extern ncx_bad_data_t
    get_baddata (void);


/********************************************************************
* FUNCTION get_defindent
* 
*  Get the default indentation 'defindent parameter'
* 
* RETURNS:
*    defindent value
*********************************************************************/
extern int32
    get_defindent (void);



/********************************************************************
* FUNCTION get_yangcli_mod
* 
*  Get the yangcli module
* 
* RETURNS:
*    yangcli module
*********************************************************************/
extern ncx_module_t *
    get_yangcli_mod (void);


/********************************************************************
* FUNCTION get_unit_test_mod
* 
*  Get the unit-test module
* 
* RETURNS:
*    yangcli module
*********************************************************************/
extern ncx_module_t *
    get_unit_test_mod (void);

/********************************************************************
* FUNCTION get_mgr_cli_valset
* 
*  Get the CLI value set
* 
* RETURNS:
*    mgr_cli_valset variable
*********************************************************************/
extern val_value_t *
    get_mgr_cli_valset (void);


/********************************************************************
* FUNCTION get_connect_valset
* 
*  Get the connect value set
* 
* RETURNS:
*    connect_valset variable
*********************************************************************/
extern val_value_t *
    get_connect_valset (void);


/********************************************************************
* FUNCTION get_aliases_file
* 
*  Get the aliases-file value
* 
* RETURNS:
*    aliases_file variable
*********************************************************************/
extern const xmlChar *
    get_aliases_file (void);


/********************************************************************
* FUNCTION get_uservars_file
* 
*  Get the uservars-file value
* 
* RETURNS:
*    aliases_file variable
*********************************************************************/
extern const xmlChar *
    get_uservars_file (void);


/********************************************************************
* FUNCTION get_sessions_file
* 
*  Get the sessions-file value
* 
* RETURNS:
*    sessions_file variable
*********************************************************************/
extern const xmlChar *
    get_sessions_file (void);


/********************************************************************
* FUNCTION get_test_suite_file
* 
*  Get the test-suite-file value
* 
* RETURNS:
*    test_suite_file variable
*********************************************************************/
extern const xmlChar *
    get_test_suite_file (void);


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
extern status_t
    replace_connect_valset (const val_value_t *valset);


/********************************************************************
* FUNCTION get_mgrloadQ
* 
*  Get the mgrloadQ value pointer
* 
* RETURNS:
*    mgrloadQ variable
*********************************************************************/
extern dlq_hdr_t *
    get_mgrloadQ (void);


/********************************************************************
* FUNCTION get_aliasQ
* 
*  Get the aliasQ value pointer
* 
* RETURNS:
*    aliasQ variable
*********************************************************************/
extern dlq_hdr_t *
    get_aliasQ (void);


/********************************************************************
* FUNCTION get_time_rpcs
* 
*  Get the time-rpcs value
* 
* RETURNS:
*    time_rpcs variable
*********************************************************************/
extern boolean
    get_time_rpcs (void);


/********************************************************************
* FUNCTION get_time_rpcs_stats
* 
*  Get the time-rpcs-stats value
* 
* RETURNS:
*    time_rpcs_stats variable
*********************************************************************/
extern boolean
    get_time_rpcs_stats (void);


/********************************************************************
* FUNCTION get_echo_replies
* 
*  Get the echo-replies value
* 
* RETURNS:
*    echo_replies variable
*********************************************************************/
extern boolean
    get_echo_replies (void);


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
extern void
    yangcli_reply_handler (ses_cb_t *scb,
			   mgr_rpc_req_t *req,
			   mgr_rpc_rpy_t *rpy);


/********************************************************************
 * FUNCTION finish_result_assign
 * 
 * finish the assignment to result_name or result_filename
 * use 1 of these 2 parms:
 *    resultval == result to output to file
 *    resultstr == result to output as string
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   session_cb == session control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
extern status_t
    finish_result_assign (server_cb_t *server_cb,
                          session_cb_t *session_cb,
			  val_value_t *resultvar,
			  const xmlChar *resultstr);


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
extern void
    report_capabilities (server_cb_t *server_cb,
                         session_cb_t *session_cb,
                         const ses_cb_t *scb,
                         boolean isfirst,
                         help_mode_t mode);


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
extern void clean_session_cbQ (server_cb_t *server_cb,
                               boolean all);

/********************************************************************
 * FUNCTION delete_one_session_cb
 *
 * Delete one session for a server context
 *
 * INPUTS:
*    server_cb == server control block to use
*    session_cb_t *session_cb
*********************************************************************/
extern void delete_one_session_cb (server_cb_t *server_cb,
                                   session_cb_t *session_cb);


/********************************************************************
 * FUNCTION server_connected
 * 
 * Return TRUE if any sessions are connected
 *
 * INPUTS:
 *    server_cb == server control block to use
 *********************************************************************/
extern boolean 
    server_connected (server_cb_t *server_cb);


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
extern uint32
    session_connected_count (server_cb_t *server_cb);


/********************************************************************
 * FUNCTION session_connected
 * 
 * Return TRUE if the specified session is connected
 *
 * INPUTS:
 *    session_cb == session control block to use
 *********************************************************************/
extern boolean 
    session_connected (session_cb_t *session_cb);


/********************************************************************
 * FUNCTION find_session_cb
 * 
 * Return the specified entry or NULL if not found
 *
 * INPUTS:
 *    server_cb == server context to use
 *    name == name of active session struct to find
 *********************************************************************/
extern session_cb_t *find_session_cb (server_cb_t *server_cb,
                                      const xmlChar *name);

/********************************************************************
 * FUNCTION find_session_cfg
 * 
 * Return the specified entry or NULL if not found
 *
 * INPUTS:
 *    server_cb == server context to use
 *    name == name of saved session config struct to find
 *********************************************************************/
extern session_cfg_t *find_session_cfg (server_cb_t *server_cb,
                                        const xmlChar *name);


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
extern session_cb_t * add_session_cb (server_cb_t *server_cb,
                                      session_cfg_t *session_cfg);


/********************************************************************
 * FUNCTION set_cur_session_cb
 * 
 * Set the current session for a server context
 *
 * INPUTS:
 *    server_cb == server context to use
 *    session_cb == session_cb to make the current context
 *********************************************************************/
extern void
    set_cur_session_cb (server_cb_t *server_cb,
                        session_cb_t *session_cb);

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
extern const xmlChar *
    get_session_name (session_cb_t *session_cb);


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
extern void
    clean_session_cb_conn (session_cb_t *session_cb);

/********************************************************************
* FUNCTION get_def_aliases_file_mtime
*
*  Get the mtime of ~/.yumapro/yangcli_pro_tests.conf
*
* RETURNS:
*    mtime (modification time)
*********************************************************************/
extern time_t
    get_def_aliases_file_mtime (void);

/********************************************************************
* FUNCTION update_def_aliases_file_mtime
*
*  Update the mtime of ~/.yumapro/yangcli_pro_tests.conf
*  INPUTS:
*   time_t* mtime
*
*********************************************************************/
extern void
    update_def_aliases_file_mtime (time_t * mtime);

/********************************************************************
* FUNCTION get_yangcli_file_saved_mtime
*
* Get the last modification time of the yangcli file.
*
* INPUTS:
*   yangcli_file_t: which yangcli file.
* RETURNS:
*   the last modification time.
*********************************************************************/
extern time_t
    get_yangcli_file_saved_mtime (yangcli_file_t whichfile);

/********************************************************************
* FUNCTION update_def_yangcli_file_mtime
*
* Update the modification time of the specified yangcli file.
*
* INPUTS:
*   yangcli_def_file_t: yangcli default file.
*   fullspec: yangcli file.
* RETURNS: status
*
*********************************************************************/
extern status_t
    update_def_yangcli_file_mtime (yangcli_file_t whichfile,
                               const xmlChar *fullspec);

/********************************************************************
* FUNCTION check_for_saving_def_yangcli_file
*
* Check if this file can be saved.
*
* INPUTS:
*   yangcli_file_t: yangcli default file.
*   filespec == full file spec to check
*
* RETURNS: status
*********************************************************************/
extern status_t
    check_for_saving_def_yangcli_file (yangcli_file_t whichfile,
                                   const xmlChar *fullspec);

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
    get_yangcli_param_change_flag (yangcli_file_t which_flag);

/*********************************************************************
* FUNCTION update_yangcli_param_change_flag
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
                                      boolean change_flag );

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
    check_def_file_mtime_changed (yangcli_file_t whichfile);

/*********************************************************************
* FUNCTION update the status of connection_all_in_progress.
*
* INPUTS:
*   boolean: change_stat: TRUE or FALSE.
* RETURNS: none
*********************************************************************/
extern void
    update_connect_all_in_progress (boolean in_progress);

/********************************************************************
* FUNCTION connect_all_in_progress
*
* INPUTS: NONE
* RETURNS:
*   the change flag: TRUE or FALSE
*********************************************************************/
extern boolean
    check_connect_all_in_progress (void);

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
    get_yangcli_def_file (yangcli_file_t which_flag, status_t* res );

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
extern status_t 
    yangcli_init (int argc,
                  char *argv[],
                  program_mode_t progmode,
                  boolean *quickexit);


/********************************************************************
 * FUNCTION yangcli_cleanup
 * 
 * 
 * 
 *********************************************************************/
extern void
    yangcli_cleanup (void);


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
extern void
    update_session_cb_vars (session_cb_t *session_cb);


/********************************************************************
* FUNCTION clean_session_cb_vars
* 
*  Clean the session state variables
* 
* INPUTS:
*    session_cb == session control block to use
*
*********************************************************************/
extern void
    clean_session_cb_vars (session_cb_t *session_cb);


/********************************************************************
* FUNCTION get_cur_server_cb
* 
*  Get the current server control block
* 
* RETURNS:
*  pointer to current server control block or NULL if none
*********************************************************************/
extern server_cb_t *
    get_cur_server_cb (void);


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
extern config_edit_t *
    new_config_edit (op_editop_t editop,
                     val_value_t *etree);


/********************************************************************
 * FUNCTION free_config_edit
 *
 * Free a config_edit struct
 *
 * INPUTS:
 *   edit == struct to free
 *
 *********************************************************************/
extern void
    free_config_edit (config_edit_t *edit);


/********************************************************************
 * FUNCTION get_program_mode
* 
*  Get the program_mode value
* 
* RETURNS:
*    program_mode value
*********************************************************************/
extern program_mode_t
    get_program_mode (void);


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
extern status_t
    register_help_action (server_cb_t *server_cb);


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
extern status_t
    unregister_help_action (server_cb_t *server_cb);


#ifdef EVAL_VERSION
/********************************************************************
 * FUNCTION check_eval_command_limit
* 
*  Check if the command limit is reached.
* 
* RETURNS:
*    status
*********************************************************************/
extern status_t
    check_eval_command_limit (void);
#endif // EVAL_VERSION

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli */

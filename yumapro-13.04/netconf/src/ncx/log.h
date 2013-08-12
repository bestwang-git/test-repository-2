/*
* Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifndef _H_log
#define _H_log
/*  FILE: log.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    Logging manager

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
08-jan-06    abb      begun
28may12      mts      Logging feature set mods
*/

#include <stdio.h>
#include <xmlstring.h>

#include "procdefs.h"
#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_DEPRECATION_PENDING TRUE

/********************************************************************
*								    *
*			    M A C R O S                             *
*								    *
*********************************************************************/

/*
 * Log internal errors to stderr. We can't call logger code to
 * handle its own internal errors.
 */

/********************************************************************
* MACRO: LOG_INTERNAL_ERR
*
*    Output string to stderr reporting error, function name, and line number
*    and resulting action ...
*
* EXAMPLE:
*
*     MACRO: LOG_INTERNAL_ERR(Null buf ptr, RETURN);
*
*    OUTPUT: ERROR [log_util_init_buf@86]: Null buf ptr - RETURN
*
*
*********************************************************************/
#define LOG_INTERNAL_ERR(err_str, action)                                     \
  log_internal_err("\nERROR [%s@%d]: " #err_str " - " #action,                \
	       __FUNCTION__, __LINE__)


/********************************************************************
* MACRO: LOG_INTERNAL_BUFF_ERR
*
*    Output string to stderr reporting error, function name, and line number.
*    Dump salient buffer descriptor state, and resulting action ...
*
* EXAMPLE:
*
*     MACRO: LOG_INTERNAL_BUF_ERR(Bad buf, buf, remaining, REINIT);
*
*    OUTPUT: ERROR [upd_log_buf@179]: Bad buf 'remaining' [ptr=81783a0,  \
*            id=4242, start=81783cc, end=81787cc, len=1024(0x400),       \
*            remaining=1024(0x400)]->remaining=400 - REINIT              \
*
*********************************************************************/
#define LOG_INTERNAL_BUF_ERR(err_str, ptr, action)                            \
  log_internal_err("\nERROR [%s@%d]: " #err_str                               \
                   " [ptr=%p, id=%4x, start=%p, end=%p"                       \
                   ", len=%u(0x%x), remaining=%d(0x%x)]"                      \
                   "wp=%u" "- %s\n", __FUNCTION__, __LINE__,                  \
                  ptr, ptr->idid, ptr->start, ptr->end,                       \
		   ptr->len, ptr->len, ptr->remaining, ptr->remaining,        \
		   (uint)ptr->write_pending, #action)


/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/

/* macros to check the debug level */
#define LOGDEV0    (debug_level >= LOG_DEBUG_DEV0)
#define LOGERROR   (debug_level >= LOG_DEBUG_ERROR)
#define LOGWARN    (debug_level >= LOG_DEBUG_WARN)
#define LOGINFO    (debug_level >= LOG_DEBUG_INFO)
#define LOGDEV1    (debug_level >= LOG_DEBUG_DEV1)
#define LOGDEBUG   (debug_level >= LOG_DEBUG_DEBUG)
#define LOGDEBUG2  (debug_level >= LOG_DEBUG_DEBUG2)
#define LOGDEBUG3  (debug_level >= LOG_DEBUG_DEBUG3)
#define LOGDEBUG4  (debug_level >= LOG_DEBUG_DEBUG4)


#define LOG_DEBUG_STR_OFF     (const xmlChar *)"off"
#define LOG_DEBUG_STR_WRITE   (const xmlChar *)"write" /* "Force" output */
#define LOG_DEBUG_STR_DEV0    (const xmlChar *)"dev0"  /* Special debugging */
#define LOG_DEBUG_STR_ERROR   (const xmlChar *)"error"
#define LOG_DEBUG_STR_WARN    (const xmlChar *)"warn"
#define LOG_DEBUG_STR_INFO    (const xmlChar *)"info"
#define LOG_DEBUG_STR_DEV1    (const xmlChar *)"dev1" /* Special debugging */
#define LOG_DEBUG_STR_DEBUG   (const xmlChar *)"debug"
#define LOG_DEBUG_STR_DEBUG2  (const xmlChar *)"debug2"
#define LOG_DEBUG_STR_DEBUG3  (const xmlChar *)"debug3"
#define LOG_DEBUG_STR_DEBUG4  (const xmlChar *)"debug4"

  /* syslog wants to know what app is calling ... */
#define LOG_DEBUG_APP_STR_UNKNOWN   (const char *)"?yuma?"
#define LOG_DEBUG_APP_STR_YANGCLI   (const char *)"yangcli-pro"
#define LOG_DEBUG_APP_STR_YANGDUMP  (const char *)"yangdump-pro"
#define LOG_DEBUG_APP_STR_YANGDIFF  (const char *)"yangdiff-pro"
#define LOG_DEBUG_APP_STR_NETCONFD  (const char *)"netconfd-pro"

/*
 * Count to skip backtrace frames that are within the logging code ...
 * these are usually not interesting to the user.
 */
#define FRAME_OVERHEAD_CNT 4

/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/* The output stream enumerations used in util/log.c */
typedef enum log_stream_t_ {
    LOG_STREAM_NONE,           /* value not set or error */
    LOG_STREAM_STDOUT,         /* Output destined for stdout */
    LOG_STREAM_STDERR,         /* Output destined for stderr */
    LOG_STREAM_LOGFILE         /* Output destined for logfile */
}  log_stream_t;

/* The debug level enumerations used in util/log.c */
typedef enum log_debug_t_ {
    LOG_DEBUG_NONE,           /* value not set or error */
    LOG_DEBUG_OFF,            /* logging turned off */ 
    LOG_DEBUG_WRITE,          /* logging turned on */ 
    LOG_DEBUG_DEV0,           /* Special use developer debugging only */
    LOG_DEBUG_ERROR,          /* fatal + internal errors only */
    LOG_DEBUG_WARN,           /* all errors + warnings */
    LOG_DEBUG_INFO,           /* all previous + user info trace */
    LOG_DEBUG_DEV1,           /* Special use developer debugging only */
    LOG_DEBUG_DEBUG,          /* debug level 1 */
    LOG_DEBUG_DEBUG2,         /* debug level 2 */
    LOG_DEBUG_DEBUG3,         /* debug level 3 */
    LOG_DEBUG_DEBUG4          /* debug level 4 */
}  log_debug_t;

  /* syslog wants to know what app is logging ... */
typedef enum log_debug_app_t_ {
    LOG_DEBUG_APP_NONE,           /* value not set or error */
    LOG_DEBUG_APP_YANGCLI,
    LOG_DEBUG_APP_YANGDUMP,
    LOG_DEBUG_APP_YANGDIFF,
    LOG_DEBUG_APP_NETCONFD,
    LOG_DEBUG_APP_MAX
}  log_debug_app_t;

#define VALID_DEBUG_APP(app) \
                       (app > LOG_DEBUG_APP_NONE) && (app < LOG_DEBUG_APP_MAX)

/* logging function template to switch between
 * log_stdout and log_write
 */
typedef void (*logfn_void_t) (void);

/*
 * Function vectors to switch between native versus syslog (or
 * vendor-specific) logging.
 */
typedef void (*logfn_t) (const char *fstr, ...)
                         __attribute__ ((format (printf, 1, 2)));


  /* log_common() and log_xxx_common() */
typedef void (*logfn_cmn_va_t) (boolean recursive,
				log_debug_t level, log_debug_t sub_level,
				const char *fstr, va_list args);
  /* log_append() and log_xxx_append() */
typedef void (*logfn_app_va_t) (boolean recursive,
				log_debug_t level, log_debug_t sub_level,
				const char *fstr, va_list args);
  /* log_flush() and log_xxx_flush() */
typedef void (*logfn_flush_t) (void);
  /* log_xxx_connect() */
typedef void (*logfn_connect_t) (void);
  /* log_xxx_send() */
typedef void (*logfn_send_t) (log_debug_app_t app, log_debug_t level,
			      const char *fstr, ...)
                              __attribute__ ((format (printf, 3, 4)));


  /* Accessed by log_syslog.c and log_vendor.c */
extern logfn_connect_t logfn_connect;
extern logfn_send_t logfn_send;

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/
extern log_debug_t debug_level;

/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/
/********************************************************************
* FUNCTION log_cleanup
*
* Final logger cleanup prior to restart or shutdown
*
* INPUTS:
*    None
*
* RETURNS:
*    None
*********************************************************************/
void
    log_cleanup (void);

/********************************************************************
* FUNCTION log_internal_err
*
* Send internal logging error info to stderr. This function is called
* when an error is detected while in the process of trying to send
* info via the logging stream.
*
* See also  LOG_INTERNAL_ERR() and  LOG_INTERNAL_BUF_ERR macros.
*
* INPUTS:
*
*    fstr == format output string
*    ... == variable argument list
*
* RETURNS:
*    None
*********************************************************************/
void
    log_internal_err (const char *fstr, ...)
                      __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_set_xxx and log_get_xxx
*
* Read/write support for the various (binary) --log-xxx config
* options above. Additional comments inline where noteworthy.
*
* INPUTS:
*    None
*
* RETURNS:
*    if log_get_xxx(), the requested configured value
*********************************************************************/

/* Backtrace info level */
extern boolean
    log_get_backtrace_detail (void);
extern void
    log_set_backtrace_detail (void);

/* --logheader="custom" */
extern void
    log_set_custom (void);

/* --log-header="localtime" */
extern void
    log_set_localtime (void);

/* --log-mirroring */
extern void
    log_set_mirroring (void);
/* --log-mirroring */
extern boolean
    log_get_mirroring (void);

/* --log-stderr */
extern void
    log_set_stderr (void);

/* --log-suppress-ctrl */
extern void
    log_set_suppress_ctrl (void);

/* --log-syslog --log-vendor */
extern boolean
    log_get_syslog_bfr_allocated (void);

/* --log-syslog --log-vendor */
extern boolean
    log_get_vendor_bfr_allocated (void);

/* --log-syslog --log-vendor */
extern void
    log_set_syslog_bfr_allocated (void);

/* --log-syslog --log-vendor */
extern void
    log_set_vendor_bfr_allocated (void);

/* --log-syslog --log-vendor */
extern void
    log_clr_syslog_bfr_allocated (void);

/* --log-syslog --log-vendor */
extern void
    log_clr_vendor_bfr_allocated (void);

/* --log-syslog */
extern void
    log_set_syslog (void);

/* --log-vendor */
extern void
    log_set_vendor (void);

/* --log-backtrace-stream="logfile" */
extern void
    log_set_backtrace_logfile (void);

/* --log-backtrace-stream="stderr" */
extern void
    log_set_backtrace_stderr (void);

/* --log-backtrace-stream="stdout" */
extern void
    log_set_backtrace_stdout (void);

/* --log-backtrace-stream="syslog" */
extern boolean
    log_get_backtrace_syslog (void);
extern void
    log_set_backtrace_syslog (void);

/* --log-backtrace-stream="vendor" */
extern boolean
    log_get_backtrace_vendor (void);
extern void
    log_set_backtrace_vendor (void);


/********************************************************************
 * FUNCTION log_set_backtrace
*
*    Set the maximum frame count to report on a backtrace log entry
*    (--log-backtrace=<frame_count>)
*
* INPUTS:
*
*    frame_cnt == the requested frame count (if 0 then use internal default).
*
* RETURNS:
*    None
*
*********************************************************************/
extern void
    log_set_backtrace (uint frame_cnt);


/********************************************************************
* FUNCTIONs log_cvt_debug_level2bit
*
*    Convert a debug level to a unique bit mask
*
* INPUTS:
*
*    level == debug level enum to convert
*
* RETURNS:
*    Unique bit mask
*
*********************************************************************/
extern uint
    log_cvt_debug_level2bit (log_debug_t level);


/********************************************************************
* FUNCTIONs log_set_backtrace_level and log_clear_backtrace_level
*
*    Manage the debug level bit mask to allow or restrict
*    reporting of backtrace log entries
*
* INPUTS:
*
*    log_set_backtrace_level: level == debug level for which backtrace
*                             log entries will be reported
*
*    log_clear_backtrace_level: level == debug level for which backtrace
*                               log entries should no longer be reported
*
*
* RETURNS:
*    none
*********************************************************************/
extern void
    log_clear_backtrace_level (log_debug_t level);


extern void
    log_set_backtrace_level (log_debug_t);


/********************************************************************
* FUNCTION log_set_backtrace_level_mask
*
*    Set the debug level bit mask to restrict reporting of backtrace info
*
* INPUTS:
*    mask == bit mask respresenting debug levels for which to append
*            backtrace info
*
* RETURNS:
*    none
*********************************************************************/
extern void
    log_set_backtrace_level_mask (uint mask);


/********************************************************************
* FUNCTIONs log_get_backtrace_level_mask
*
*    Return the debug level bit mask in current use
*
* INPUTS:
*    none
*
* RETURNS:
*     mask == bit mask respresenting debug levels for which to append
*             backtrace info
*********************************************************************/
extern uint
    log_get_backtrace_level_mask (void);

/********************************************************************
* FUNCTIONs log_test_backtrace_level
*
*    Return whether a given debug level is enabled for backtrace
*
* INPUTS:
*    level == debug level to test
*
* RETURNS:
*      TRUE == backtrace enabled for level
*     FALSE == backtrace disabled for level
*********************************************************************/
extern boolean
    log_test_backtrace_level (log_debug_t level);


/********************************************************************
* FUNCTION log_do_backtrace
*
* Return the configured status for including backtrace info on a
* given output stream (e.g., logfile, syslog, stderr, etc.) at a
* given content level (e.g., error, warn, info, etc.).
*
* NOTE: Slightly funky. To use, pass the current (boolean) value
*       of the config flag to be tested, along with the current
*       content level (sub_level).
*
* INPUTS:
*
*    sub_level == the content level for the log output
*    flag == the --log-backtrace-stream flag to test
*
* RETURNS:
*
*    TRUE  == backtrace info should be generated
*    FALSE == backtrace info should NOT be generated
*
*********************************************************************/
extern boolean
    log_do_backtrace (log_debug_t sub_level, boolean flag);


/********************************************************************
* FUNCTION log_open
*
*   Open a logfile for writing
*   DO NOT use this function to send log entries to STDOUT
*   Leave the logfile NULL instead.
*
* INPUTS:
*   fname == full filespec string for logfile
*   append == TRUE if the log should be appended
*          == FALSE if it should be rewriten
*   tstamps == TRUE if the datetime stamp should be generated
*             at log-open and log-close time
*          == FALSE if no open and close timestamps should be generated
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    log_open (const char *fname,
	      boolean append,
	      boolean tstamps);


/********************************************************************
* FUNCTION log_close
*
*   Close the logfile
*
* RETURNS:
*    none
*********************************************************************/
extern void
    log_close (void);


/********************************************************************
* FUNCTION log_audit_open
*
*   Open the audit logfile for writing
*   DO NOT use this function to send log entries to STDOUT
*   Leave the audit_logfile NULL instead.
*
* INPUTS:
*   fname == full filespec string for audit logfile
*   append == TRUE if the log should be appended
*          == FALSE if it should be rewriten
*   tstamps == TRUE if the datetime stamp should be generated
*             at log-open and log-close time
*          == FALSE if no open and close timestamps should be generated
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    log_audit_open (const char *fname,
                    boolean append,
                    boolean tstamps);


/********************************************************************
* FUNCTION log_audit_close
*
*   Close the audit_logfile
*
* RETURNS:
*    none
*********************************************************************/
extern void
    log_audit_close (void);


/********************************************************************
* FUNCTION log_audit_is_open
*
*   Check if the audit log is open
*
* RETURNS:
*   TRUE if audit log is open; FALSE if not
*********************************************************************/
extern boolean
    log_audit_is_open (void);


/********************************************************************
* FUNCTION log_alt_open
*
*   Open an alternate logfile for writing
*   DO NOT use this function to send log entries to STDOUT
*   Leave the logfile NULL instead.
*
* INPUTS:
*   fname == full filespec string for logfile
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    log_alt_open (const char *fname);


/********************************************************************
* FUNCTION log_alt_open_ex
*
*   Open an alternate logfile for writing
*   DO NOT use this function to send log entries to STDOUT
*   Leave the logfile NULL instead.
*
* INPUTS:
*   fname == full filespec string for logfile
*   overwrite == TRUE if OK to overwrite; 
*                FALSE to generate DATA_EXISTS errors
* RETURNS:
*    status
*********************************************************************/
extern status_t
    log_alt_open_ex (const char *fname,
                     boolean overwrite);

/********************************************************************
* FUNCTION log_alt_close
*
*   Close the alternate logfile
*
* RETURNS:
*    none
*********************************************************************/
extern void
    log_alt_close (void);


/********************************************************************
* FUNCTION log_init_logfn_va
*
*   Initialize log stream vectors. Possible log streams include:
*
* - Classic (send to stdout, stderr, and/or logfile
* - Syslog (send to syslog) (+ Classic if --log-mirroring)
* - Vendor (send to vendor specific interface) (+ Classic if --log-mirroring)
*
* INPUTS:
*
*  cfg_log_syslog == TRUE if --log-syslog was set
*  cfg_log_vendor == TRUE if --log-vendor was set
*
* NOTE:
*
* Called early in initialization by each app after CLI parsing complete.
*
*********************************************************************/
extern void
    log_init_logfn_va (void);


/********************************************************************
* FUNCTION log_common
*
*   Generate "new" message log output, optionally pre-pended with an
*   internal status header (date, time, level, etc). Additional info
*   may be appended (via log_append()) on the same or subsequent lines.
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   args  == any additional arguments for printf
*
* KLUDGE ALERT:
*    For convenience we equate the 'recursive' param to a call from
*    print_backtrace(), passing this information unvetted on to
*    log_common_internal(). At present, this works because print_backtrace()
*    is the ONLY routine to pass recursive as TRUE. Someday this may change.
*
*********************************************************************/
extern void 
    log_common (boolean recursive, log_debug_t level, log_debug_t sub_level,
		const char *fstr, va_list args);

/********************************************************************
* FUNCTION log_append
*
*   Append formatted string to the current logger output stream
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   args == additional arguments for printf
*
*********************************************************************/
extern void 
    log_append (boolean recursive, log_debug_t level, log_debug_t sub_level,
		const char *fstr, va_list args);


/********************************************************************
* FUNCTION log_flush
*
*   Flush output buffers
*
* NOTE: For "classic" output this is probably redundant since we
*       clear buffers as we go along. However, for syslog and vendor
*       streams this is important. That's because we buffer our
*       formatted output and only send it on in full chunks. The
*       buffering code can't always know when a buffered log message
*       is complete.
*
* INPUTS:
*    None
*
*********************************************************************/
extern void 
    log_flush (void);

extern void 
    log_common_va_list (boolean recursive,
			log_debug_t level, log_debug_t sub_level,
			logfn_cmn_va_t logfn, const char *fstr, ...)
  __attribute__ ((format (printf, 5, 6)));

extern void 
    log_append_va_list (boolean recursive,
			log_debug_t level, log_debug_t sub_level,
			logfn_app_va_t logfn, const char *fstr, ...)
  __attribute__ ((format (printf, 5, 6)));



/********************************************************************
* FUNCTION log_print_backtrace
*
*   Print up to (array[] size) stack frame addresses
*
* INPUT:
*
*    orig_sub_level == content level
*    logfn_common == common call vector function (native, syslog, or vendor)
*    logfn_append == append call vector function
*    localfn_flush == flush call vector function 
*    max_detail == include maximum backtrace return information
*    frame_overhead == number of frames in logger code (skip)
*    preamble == prompt string to precede backtrace output
*
* RETURNS:
*    none
*********************************************************************/
extern void
    log_print_backtrace (log_debug_t sub_level,
			 logfn_cmn_va_t logfn_common,
			 logfn_app_va_t logfn_append,
			 logfn_flush_t logfn_flush,
			 boolean max_detail,
			 uint frame_overhead,
			 const char *preamble);

/********************************************************************
* FUNCTION log_backtrace
*
*    Output a string followed by backtrace detail, regardless of
*    backtrace setting.
*
* In general, use only for debugging ... should not be present in released
* software (though it might be useful for debugging in the field).
*
* INPUTS:
*   level == output level
*   fstr == format string for log message
*   ... == variable arg list for format string
*********************************************************************/
extern void
    log_backtrace (log_debug_t level, const char *fstr, ...)
    __attribute__ ((format (printf, 2, 3)));



/********************************************************************
* FUNCTION log_stdout
*
*   Write lines of text to STDOUT, even if the logfile
*   is open, unless the debug mode is set to NONE
*   to indicate silent batch mode
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_stdout (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_write and log_write_append
*
*   Generate (append to) a log entry, regardless of log level (except
*   batch mode).
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_write (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void 
    log_write_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

/********************************************************************
 FUNCTIONs log_write_error/info and log_write_error/info_append
*
*   Generate (append to) a log entry, regardless of log level (except
*   batch mode). However, report functional level as error or info.
*
* NOTE: Useful? Currently deprecated ...
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/

#ifndef LOG_DEPRECATION_PENDING // ***** USEFUL? DEPRECATION PLANNED *****
extern void 
    log_write_error (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void 
    log_write_error_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void 
    log_write_info (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void 
    log_write_info_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));
#endif /* #ifdef LOG_DEPRECATION_PENDING */


/********************************************************************
* FUNCTION log_audit_write
*
*   Generate an audit log entry, regardless of log level
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_audit_write (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTION log_alt_write
*
*   Write to the alternate log file
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for fprintf
*
*********************************************************************/
extern void 
    log_alt_write (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));



/********************************************************************
* FUNCTION log_alt_indent
* 
* Printf a newline to the alternate logfile,
* then the specified number of space chars
*
* INPUTS:
*    indentcnt == number of indent chars, -1 == skip everything
*
*********************************************************************/
extern void
    log_alt_indent (int32 indentcnt);


/********************************************************************
* FUNCTION vlog_error
*
*   Generate a LOG_DEBUG_ERROR log entry
*
* INPUTS:
*   fstr == format string in printf format
*   valist == any additional arguments for printf
*
*********************************************************************/
void
    vlog_error (const char *fstr, va_list args );


/********************************************************************
* FUNCTIONs log_error and log_error_append
*
*   Generate a LOG_DEBUG_ERROR log entry, or append output to previous.
*
* In general, call log_error() once and append additional output as
* many times as necessary to complete the message (for example, from
* a loop). Note that additional output at a different log level should
* NOT be attempted, but no check is made for this condition currently.
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void
    log_error (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void
    log_error_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTION log_warn and log_warn_append
*
*   Generate (append to) a LOG_DEBUG_WARN log entry
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_warn (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void 
    log_warn_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
*   FUNCTIONs log_info and log_info_append
*
*   Generate (append to) a LOG_DEBUG_INFO log entry
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_info (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void 
    log_info_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_debug and log_debug_append
*
*   Generate (append to) a LOG_DEBUG_DEBUG log entry
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_debug (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void
    log_debug_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_debug2 and log_debug2_append
*
*   Generate (append to) a LOG_DEBUG_DEBUG2 log trace entry
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_debug2 (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void 
    log_debug2_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_debug3 and log_debug3_append
*
*   Generate (append to) a LOG_DEBUG_DEBUG3 log trace entry
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_debug3 (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void 
    log_debug3_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_debug4 and log_debug4_append
*
*   Generate (append to) a LOG_DEBUG_DEBUG4 log trace entry
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_debug4 (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void 
    log_debug4_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_dev0 and log_dev0_append
*
*   Generate (append to) a LOG_DEBUG_DEV0 log entry
*
* NOTE: This level is intended primarily for debugging, where output related
*       issues at {ERROR, WARN, INFO} levels change program behavior)
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_dev0 (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void
    log_dev0_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_dev1 and log_dev1_append
*
*   Generate (append to) a LOG_DEBUG_DEV1 log entry
*
* NOTE: This level is intended primarily for debugging, where output related
*       issues at {DEBUG, DEBUG2, DEBUG3, DEBUG4} change program behavior)
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_dev1 (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));

extern void
    log_dev1_append (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTIONs log_write_level and log_write_level_append
*
*   Generate (append to) a LOG_DEBUG_<LEVEL> log trace entry
*
* NOTE: Useful when the desired debug level is passed as a param
*
* INPUTS:
*   level == debug level
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void
    log_write_level (log_debug_t level, const char *fstr, ...)
    __attribute__ ((format (printf, 2, 3)));

extern void
    log_write_level_append (log_debug_t level, const char *fstr, ...)
    __attribute__ ((format (printf, 2, 3)));


/********************************************************************
* FUNCTION log_noop
*
*  Do not generate any log message NO-OP
*  Used to set logfn_t to no-loggging option
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_noop (const char *fstr, ...)
    __attribute__ ((format (printf, 1, 2)));


/********************************************************************
* FUNCTION log_set_debug_level
* 
* Set the global debug filter threshold level
* 
* INPUTS:
*   dlevel == desired debug level
*
*********************************************************************/
extern void
    log_set_debug_level (log_debug_t dlevel);


/********************************************************************
* FUNCTION log_get_debug_level
* 
* Get the global debug filter threshold level
* 
* RETURNS:
*   the global debug level
*********************************************************************/
extern log_debug_t
    log_get_debug_level (void);


/********************************************************************
* FUNCTION log_get_debug_level_enum
* 
* Get the corresponding debug enum for the specified string
* 
* INPUTS:
*   str == string value to convert
*
* RETURNS:
*   the corresponding enum for the specified debug level
*********************************************************************/
extern log_debug_t
    log_get_debug_level_enum (const char *str);

extern uint
    log_parse_debug_level_str (const char *str);


/********************************************************************
* FUNCTION log_get_debug_level_string
* 
* Get the corresponding string for the debug enum
* 
* INPUTS:
*   level ==  the enum for the specified debug level
*
* RETURNS:
*   the string value for this enum

*********************************************************************/
extern const xmlChar *
    log_get_debug_level_string (log_debug_t level);


/********************************************************************
* FUNCTION log_get_debug_app_string
* 
* Get the corresponding string for the debug app enum
* 
* INPUTS:
*   app ==  the enum for the specified debug app
*
* RETURNS:
*   the string value for this enum

*********************************************************************/
extern const char *
    log_debug_get_app_string (log_debug_app_t app);

/********************************************************************
* FUNCTION log_debug_app2facility
* 
* Translate from YumaPro app to syslog facility
* 
* INPUTS:
*   app ==  the enum for the specified YUMA app
*
* RETURNS:
*   Appropriate syslog facility code
*
*********************************************************************/
extern int
    log_debug_app2facility (log_debug_app_t app);


/********************************************************************
* FUNCTION log_set_debug_app
* 
*    Set syslog application level ... for example, yangcli will set
*    USER while netconfd will set DAEMON for use by syslog.
* 
* INPUTS:
*    app ==  the enum for the specified YUMA app
*
* RETURNS:
*    None
*
*********************************************************************/
extern void
    log_set_debug_app (log_debug_app_t app);


/********************************************************************
* FUNCTION log_get_debug_app
* 
*    Return syslog application level.
* 
* INPUTS:
*    None
*
* RETURNS:
*    enum for the current YUMA app
*
*********************************************************************/
extern int
    log_get_debug_app (void);


/********************************************************************
* FUNCTION log_debug_app_string
* 
*    Return syslog application level string equivalent
* 
* INPUTS:
*    None
*
* RETURNS:
*    String equivalent for the current YUMA app type
*
*********************************************************************/
extern const char *
    log_debug_app_string (void);

/********************************************************************
* FUNCTION log_is_open
* 
* Check if the logfile is active
* 
* RETURNS:
*   TRUE if logfile open, FALSE otherwise
*********************************************************************/
extern boolean
    log_is_open (void);


/********************************************************************
* FUNCTION log_indent
* 
* Printf a newline, then the specified number of chars
*
* INPUTS:
*    indentcnt == number of indent chars, -1 == skip everything
*
*********************************************************************/
extern void
    log_indent (int32 indentcnt);
extern void
    log_indent_append (int32 indentcnt);


/********************************************************************
* FUNCTION log_stdout_indent
* 
* Printf a newline to stdout, then the specified number of chars
*
* INPUTS:
*    indentcnt == number of indent chars, -1 == skip everything
*
*********************************************************************/
extern void
    log_stdout_indent (int32 indentcnt);


/********************************************************************
* FUNCTION log_get_logfile
* 
* Get the open logfile for direct output
* Needed by libtecla to write command line history
*
* RETURNS:
*   pointer to open FILE if any
*   NULL if no open logfile
*********************************************************************/
extern FILE *
    log_get_logfile (void);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_log */

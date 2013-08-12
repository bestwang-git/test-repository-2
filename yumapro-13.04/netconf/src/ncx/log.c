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
/*  FILE: log.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
08jan06      abb      begun, borrowed from openssh code
28may12      mts      Logging feature set mods

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/

#if (!(defined(CYGWIN) || defined(MACOSX)))
#define WITH_BACKTRACE 1
#endif

#ifdef WITH_BACKTRACE
#include <execinfo.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
     
#include "procdefs.h"
#include "log.h"
#include "log_syslog.h"
#include "log_vendor.h"
#include "log_vendor_extern.h"
#include "ncxconst.h"
#include "status.h"
#include "tstamp.h"
#include "xml_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/* #define LOG_DEBUG_TRACE 1 */

/*
 * --log-backtrace=<frame_count> defs:
 */
#define bt_max_frames 100 /* Max backtrace frame count */
#define bt_def_frames 20  /* Default backtrace frame count */

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

/* global logging only at this time
 * per-session logging is TBD, and would need
 * to be in the agent or mgr specific session handlers,
 * not in the ncx directory
 */
log_debug_t debug_level = LOG_DEBUG_NONE;

/* syslog app (i.e, daemon, user app, etc.) */
static log_debug_app_t debug_app = LOG_DEBUG_APP_NONE;

static boolean       use_tstamps, use_audit_tstamps;

static FILE *logfile = NULL;

static FILE *altlogfile = NULL;

static FILE *auditlogfile = NULL;

/*
 * Config params under the control of CLI:
 *
 * --log-header="custom localtime":
 *
 *   Prepend extended status header to log entries
 *
 *   Arguments:
 *         custom: Prepend extended status header to log entries
 *               Example: [2012-06-26 11:03:12] [warn] Warning: xxxxx
 *
 *      localtime: Use local time for --log-header="custom"  date/time field
 *
 * --log-syslog: Directs the logging stream to syslog
 *
 * --log-vendor: Directs the logging stream to vendor interface.
 *               Defaults to syslog if vendor callback not present.
 *
 * --log-mirroring: Mirror stdout/stderr log stream even if logging
 *                  to syslog or vendor.
 *
 * --log-stderr: Direct error level log entries to stderr (unless --log=<file>
 *               is in effect)
 *
 * --log-backtrace: Enable bactrace output to accompany main log messages (as
 *                  opposed to appended output)
 *                  Specify a max frame count, or 0 to choose the default.
 *
 * --log-backtrace-level="error warn info debug debug2 debug3 debug4"
 *   Note: The presence of any of the above will limit backtrace output
 *         to those log levels specified by --log-backtrace-level=<xsdlist>
 *
 * --log-backtrace-stream="logfile stderr stdout syslog vendor"
 *   Note: The presence of any of the above will limit backtrace output
 *         to those streams specified by --log-backtrace-stream=<xsdlist>
 */

/* ***** NOTE: Add new config state entries BELOW to log_cleanup() ***** */
static boolean cfg_log_custom=FALSE;            // --log-header="custom"
static boolean cfg_log_localtime=FALSE;         // --log-header="localtime"
static boolean cfg_log_mirroring=FALSE;         // --log-mirroring
static boolean cfg_log_stderr=FALSE;            // --log-stderr
static boolean cfg_log_suppress_ctrl=FALSE;     // --log-suppress-ctrl
static boolean cfg_log_syslog=FALSE;            // --log-syslog
static boolean cfg_log_vendor=FALSE;            // --log-vendor

/* For use by malloc/free checking in ncx_cleanup() */
static boolean log_syslog_bfr_allocated=FALSE;
static boolean log_vendor_bfr_allocated=FALSE;

static boolean cfg_log_backtrace=FALSE;         // --log-backtrace=<frame_count>
static uint    cfg_log_frame_cnt=bt_def_frames; // Max backtrace frame count

static log_debug_t cfg_log_backtrace_level_mask=0;

/* 
 * --log-backtrace-stream="logfile stderr stdout syslog vendor"
 *
 * (Convert into a bit mask someday soon.)
 */
static boolean cfg_log_backtrace_logfile=FALSE;
static boolean cfg_log_backtrace_stderr=FALSE;
static boolean cfg_log_backtrace_stdout=FALSE;
static boolean cfg_log_backtrace_syslog=FALSE;
static boolean cfg_log_backtrace_vendor=FALSE;

static boolean cfg_log_backtrace_max_detail=FALSE; // Backtrace detail level

/* Possible future CLI params? */

/* Control space compression in stream */
//static boolean log_compression=TRUE;

/* Suppress ctl chars in stream */
//static boolean cfg_log_suppress=TRUE;

/* Set size of syslog buffer */
//static size_t syslog_buffer_data_size=XXX;

/* ***** NOTE: Add new config state entries ABOVE to log_cleanup() ***** */

/*
 * Define basic logger function vectors. These are modified appropriately
 * in order to support syslog or vendor output.
 */
static logfn_cmn_va_t logfn_va_common = log_common; /* Info (main) line */
static logfn_app_va_t logfn_va_append = log_append; /* Appended output */
static logfn_void_t   logfn_flush     = log_flush;  /* Flush buffered data */
logfn_connect_t       logfn_connect   = NULL;  /* Connect to syslog stream */
logfn_send_t          logfn_send      = NULL;  /* Send to syslog stream */


/********************************************************************
*                                                                   *
*                       F U N C T I O N S                           *
*                                                                   *
*********************************************************************/

/********************************************************************
* FUNCTION log_cleanup
*
* Final cleanup prior to restart or shutdown
*
* INPUTS:
*    None
*
* RETURNS:
*    None
*********************************************************************/
void
    log_cleanup (void)
{
    cfg_log_syslog = FALSE;
    cfg_log_vendor = FALSE;
    log_init_logfn_va(); /* Reset vectors to default */
    log_syslog_cleanup();
    log_vendor_cleanup();

    cfg_log_custom = FALSE;
    cfg_log_localtime = FALSE;
    cfg_log_mirroring = FALSE;
    cfg_log_stderr = FALSE;
    cfg_log_suppress_ctrl = FALSE;
    cfg_log_backtrace = FALSE;
    cfg_log_frame_cnt = bt_def_frames;
    cfg_log_backtrace_level_mask = 0;
    cfg_log_backtrace_logfile=FALSE;
    cfg_log_backtrace_stderr=FALSE;
    cfg_log_backtrace_stdout=FALSE;
    cfg_log_backtrace_syslog=FALSE;
    cfg_log_backtrace_vendor=FALSE;
    cfg_log_backtrace_max_detail=FALSE;

}

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
*    If log_get_xxx(), the requested configured value
*********************************************************************/

/* cfg backtrace detail level */
boolean
    log_get_backtrace_detail (void)
{
    return(cfg_log_backtrace_max_detail);
}

/* cfg backtrace detail level */
void
    log_set_backtrace_detail (void)
{
    cfg_log_backtrace_max_detail = TRUE;
}

/* --logheader="custom" */
void
    log_set_custom (void)
{
    cfg_log_custom = TRUE;
}

/* --log-header="localtime" */
void
    log_set_localtime (void)
{
    cfg_log_localtime = TRUE;
    log_set_custom(); /* Force, in case localtime specified by itself */
}

/* --log-mirroring */
void
    log_set_mirroring (void)
{
    cfg_log_mirroring = TRUE;
}

/* --log-mirroring */
boolean
    log_get_mirroring (void)
{
    return(cfg_log_mirroring);
}

/* --log-stderr */
void
log_set_stderr (void)
{
    cfg_log_stderr = TRUE;
}

/* --log-suppress-ctrl */
void
log_set_suppress_ctrl (void)
{
    cfg_log_suppress_ctrl = TRUE;
}

/* --log-syslog --log-vendor */
boolean
    log_get_syslog_bfr_allocated (void)
{
  return log_syslog_bfr_allocated;
}

/* --log-syslog --log-vendor */
boolean
    log_get_vendor_bfr_allocated (void)
{
    return log_vendor_bfr_allocated;
}

/* --log-syslog --log-vendor */
void
    log_set_syslog_bfr_allocated (void)
{
    log_syslog_bfr_allocated = TRUE;
}

/* --log-syslog --log-vendor */
void
    log_set_vendor_bfr_allocated (void)
{
    log_vendor_bfr_allocated = TRUE;
}

/* --log-syslog --log-vendor */
void
    log_clr_syslog_bfr_allocated (void)
{
    log_syslog_bfr_allocated = FALSE;
}

/* --log-syslog --log-vendor */
void
    log_clr_vendor_bfr_allocated (void)
{
    log_vendor_bfr_allocated = FALSE;
}

/* --log-syslog */
void
log_set_syslog (void)
{
    cfg_log_syslog = TRUE;
    log_init_logfn_va();

    /* Auto enable log file output if present */
    if (logfile) {
        log_set_mirroring();
    }
}

/* --log-vendor */
void
log_set_vendor (void)
{
    cfg_log_vendor = TRUE;
    log_init_logfn_va();

    /* Auto enable log file output if present */
    if (logfile) {
        log_set_mirroring();
    }
}

/* --log-backtrace-stream="logfile" */
void
    log_set_backtrace_logfile (void)
{
    cfg_log_backtrace_logfile = TRUE;
}

/* --log-backtrace-stream="stderr" */
void
    log_set_backtrace_stderr (void)
{
    cfg_log_backtrace_stderr = TRUE;
}

/* --log-backtrace-stream="stdout" */
void
    log_set_backtrace_stdout (void)
{
    cfg_log_backtrace_stdout = TRUE;
}

/* --log-backtrace-stream="syslog" */
boolean
    log_get_backtrace_syslog (void)
{
    return(cfg_log_backtrace_syslog);
}

void
    log_set_backtrace_syslog (void)
{
    cfg_log_backtrace_syslog = TRUE;
}

/* --log-backtrace-stream="vendor" */
boolean
    log_get_backtrace_vendor (void)
{
    return(cfg_log_backtrace_vendor);
}

void
    log_set_backtrace_vendor (void)
{
    cfg_log_backtrace_vendor = TRUE;
}

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
void
    log_set_backtrace (uint frame_cnt)
{
    cfg_log_backtrace = TRUE;

    if ((frame_cnt == 0) || (frame_cnt > bt_max_frames)) {
        frame_cnt = bt_def_frames;
    }

    cfg_log_frame_cnt = frame_cnt;
}



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
uint log_cvt_debug_level2bit (log_debug_t level)
{
    switch (level) {
    case LOG_DEBUG_NONE:
    case LOG_DEBUG_OFF:
        return 0;
    case LOG_DEBUG_WRITE:
        return bit0;
    case LOG_DEBUG_DEV0:
        return bit1;
    case LOG_DEBUG_ERROR:
        return bit2;
    case LOG_DEBUG_WARN:
        return bit3;
    case LOG_DEBUG_INFO:
        return bit4;
    case LOG_DEBUG_DEV1:
        return bit5;
    case LOG_DEBUG_DEBUG:
        return bit6;
    case LOG_DEBUG_DEBUG2:
        return bit7;
    case LOG_DEBUG_DEBUG3:
        return bit8;
    case LOG_DEBUG_DEBUG4:
        return bit9;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return 0;
    }
    /*NOTREACHED*/
}

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
void
    log_clear_backtrace_level (log_debug_t level)
{
    uint level_bit = log_cvt_debug_level2bit(level);

    cfg_log_backtrace_level_mask &= ~level_bit;
}

void
    log_set_backtrace_level (log_debug_t level)
{
    uint level_bit = log_cvt_debug_level2bit(level);

    cfg_log_backtrace_level_mask |= level_bit;
}

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
void
    log_set_backtrace_level_mask (uint mask)
{
    cfg_log_backtrace_level_mask = mask;
}


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
uint
    log_get_backtrace_level_mask (void)
{
    return(cfg_log_backtrace_level_mask);
}

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
boolean
    log_test_backtrace_level (log_debug_t level)
{
    uint level_bit = log_cvt_debug_level2bit(level);
    uint bit_mask = cfg_log_backtrace_level_mask;

    return( (bit_mask & level_bit) ? TRUE : FALSE );
}


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
#define ANY_BT_FLAGS (cfg_log_backtrace_logfile ||                    \
                      cfg_log_backtrace_stderr  ||                    \
                      cfg_log_backtrace_stdout  ||                    \
                      cfg_log_backtrace_syslog  ||                    \
		      cfg_log_backtrace_vendor)

boolean
    log_do_backtrace (log_debug_t sub_level, boolean flag)
{
    /* Never, if --log-backtrace=<> is not present */
    if (!cfg_log_backtrace) {
        return(FALSE);
    }

    /* Not now, if --log-backtrace-level specified but does not match */
    if (cfg_log_backtrace_level_mask &&
	(!log_test_backtrace_level(sub_level))) {
        return(FALSE);
    }

    /*
     * If any of the direction flags have been specified, return
     * the state of the specific flag as passed. Otherwise, only
     * --log-backtrace=<frame_count> is present and ALL streams
     * should include backtrace detail.
     */
    if (ANY_BT_FLAGS) {
        return(flag);
    } else {
        return(TRUE);
    }
}

/********************************************************************
* FUNCTION log_internal_err
*
* Send internal logging error info to stderr. This function may be
* called when an error is detected while trying to send output via
* the logging stream. (In other words, a way to log an error
* encountered while you're logging something.)
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
    //__attribute__ ((format (printf, 1, 2)))
{
    va_list args;

    va_start(args, fstr);

    vfprintf(stderr, fstr, args);
    fflush(stderr);

    va_end(args);
}

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
status_t
    log_open (const char *fname,
              boolean append,
              boolean tstamps)
{
    const char *str;
    xmlChar buff[TSTAMP_MIN_SIZE];

#ifdef DEBUG
    if (!fname) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (logfile) {
        return ERR_NCX_DATA_EXISTS;
    }

    if (append) {
        str="a";
    } else {
        str="w";
    }

    logfile = fopen(fname, str);
    if (!logfile) {
        return ERR_FIL_OPEN;
    }

    use_tstamps = tstamps;
    if (tstamps) {
        tstamp_datetime(buff);
        fprintf(logfile, "\n*** log open at %s ***\n", buff);
    }

    /* Auto enable log file output if syslog present */
    if (cfg_log_syslog || cfg_log_vendor) {
        log_set_mirroring();
    }

#ifdef DEBUG
    log_info("\nLog file: '%s'", fname);
#endif

    return NO_ERR;

}  /* log_open */


/********************************************************************
* FUNCTION log_close
*
*   Close the logfile
*
* RETURNS:
*    none
*********************************************************************/
void
    log_close (void)
{
    xmlChar buff[TSTAMP_MIN_SIZE];

    if (!logfile) {
        return;
    }

    if (use_tstamps) {
        tstamp_datetime(buff);
        fprintf(logfile, "\n*** log close at %s ***\n", buff);
    }

    fclose(logfile);
    logfile = NULL;

}  /* log_close */


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
status_t
    log_audit_open (const char *fname,
                    boolean append,
                    boolean tstamps)
{
    const char *str;
    xmlChar buff[TSTAMP_MIN_SIZE];

#ifdef DEBUG
    if (fname == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (auditlogfile != NULL) {
        return ERR_NCX_DATA_EXISTS;
    }

    if (append) {
        str="a";
    } else {
        str="w";
    }

    auditlogfile = fopen(fname, str);
    if (auditlogfile == NULL) {
        return ERR_FIL_OPEN;
    }

    use_audit_tstamps = tstamps;
    if (tstamps) {
        tstamp_datetime(buff);
        fprintf(auditlogfile, "\n*** audit log open at %s ***\n", buff);
    }

#ifdef DEBUG
    log_info("\nAudit file: '%s'", fname);
#endif

    return NO_ERR;

}  /* log_audit_open */


/********************************************************************
* FUNCTION log_audit_close
*
*   Close the audit_logfile
*
* RETURNS:
*    none
*********************************************************************/
void
    log_audit_close (void)
{
    xmlChar buff[TSTAMP_MIN_SIZE];

    if (auditlogfile == NULL) {
        return;
    }

    if (use_audit_tstamps) {
        tstamp_datetime(buff);
        fprintf(auditlogfile, "\n*** audit log close at %s ***\n", buff);
    }

    fclose(auditlogfile);
    auditlogfile = NULL;

}  /* log_audit_close */


/********************************************************************
* FUNCTION log_audit_is_open
*
*   Check if the audit log is open
*
* RETURNS:
*   TRUE if audit log is open; FALSE if not
*********************************************************************/
boolean
    log_audit_is_open (void)
{
    return (auditlogfile == NULL) ? FALSE : TRUE;

}  /* log_audit_is_open */


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
status_t
    log_alt_open (const char *fname)
{
    return log_alt_open_ex(fname, FALSE);

}  /* log_alt_open */


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
status_t
    log_alt_open_ex (const char *fname,
                     boolean overwrite)
{
    const char *str;

#ifdef DEBUG
    if (!fname) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (!overwrite && altlogfile) {
        return ERR_NCX_DATA_EXISTS;
    }

    str="w";

    altlogfile = fopen(fname, str);
    if (!altlogfile) {
        return ERR_FIL_OPEN;
    }

    return NO_ERR;

}  /* log_alt_open_ex */


/********************************************************************
* FUNCTION log_alt_close
*
*   Close the alternate logfile
*
* RETURNS:
*    none
*********************************************************************/
void
    log_alt_close (void)
{
    if (!altlogfile) {
        return;
    }

    fclose(altlogfile);
    altlogfile = NULL;

}  /* log_alt_close */



/********************************************************************
* FUNCTION log_flush_internal
*
*   Flush native stream output buffers
*
* NOTE: This is probably redundant since we clear buffers as we go along.
*       However, for consistency with syslog, do it until proven unnecessary.
*
* INPUTS:
*    None
*
*********************************************************************/
static void 
    log_flush_internal (void)
{
    fflush(stdout);
    fflush(stderr);

    if (logfile) {
        fflush(logfile);
    }
    if (altlogfile) {
        fflush(altlogfile);
    }
    if (auditlogfile) {
        fflush(auditlogfile);
    }
}

/********************************************************************
* FUNCTION log_init_logfn_va
*
*   Initialize log stream vectors. Possible log streams include:
*
* - Classic (send to stdout, stderr, and/or logfile
* - Syslog (send to syslog)
* - Vendor (send to vendor specific external function)
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
void
    log_init_logfn_va (void)
{
  /* Assume default logging */
    logfn_va_common = log_common;
    logfn_va_append = log_append;
    logfn_flush     = log_flush_internal;
    logfn_connect   = NULL; /* Required for syslog only */
    logfn_send      = NULL; /* Required for syslog only */

    /* Revector if vendor logging specified */
    if (cfg_log_vendor) {
        logfn_va_common = log_vendor_common;
	logfn_va_append = log_vendor_append;
	logfn_flush     = log_vendor_flush;
	logfn_connect   = log_vendor_connect;
	logfn_send      = log_vendor_send;
    }

    /* Revector if syslog logging specified */
    if (cfg_log_syslog) {
	logfn_va_common = log_syslog_common;
	logfn_va_append = log_syslog_append;
	logfn_flush     = log_syslog_flush;
	logfn_connect   = log_syslog_connect;
	logfn_send      = log_syslog_send;
    }

} /* log_init_logfn_va */

/********************************************************************
* FUNCTION log_output_stream
*
*   Return the current log output enum, based on the content level.
*   Possible log output enums include:
*
* - LOG_STREAM_LOGFILE
* - LOG_STREAM_STDERR
* - LOG_STREAM_STDOUT
*
* INPUTS:
*
*  sub_level == log content level
*
* RETURNS:
*    Current output file enum
*
*********************************************************************/
static log_stream_t
    log_output_stream (log_debug_t sub_level)
{
  /* If there logfile is open, all output goes there */
  if (logfile) {
      return(LOG_STREAM_LOGFILE);
  }
  /* If this is error content, and --log-stderr, direct output to stderr */
  if (cfg_log_stderr && (sub_level == LOG_DEBUG_ERROR)) {
      return(LOG_STREAM_STDERR);
  }
  /* Anything else goes to stdout */
  return(LOG_STREAM_STDOUT);
}


/********************************************************************
* FUNCTION log_output_file
*
*   Return the current log output file descriptor, based on the content level.
*   Possible log output file descriptors include:
*
* - logfile
* - stderr
* - stdout
*
* INPUTS:
*
*  sub_level == log content level
*
* RETURNS:
*    Current output file descriptor (FILE *)
*
*********************************************************************/
static FILE *
    log_output_file (log_debug_t sub_level)
{
    switch (log_output_stream(sub_level)) {
    case LOG_STREAM_STDOUT:  return stdout;
    case LOG_STREAM_STDERR:  return stderr;
    case LOG_STREAM_LOGFILE: return logfile;
    default:
      return stdout;
    }
}


/********************************************************************
* FUNCTION log_common_internal
*
*   Generate "classic" log output
*
* Note that at this point, all level screening has been performed ...
* specified output will be directed "somewhere".
*
* INPUTS:

*   header == TRUE  indicates initial msg output (prepend extended header info)
*   header == FALSE indicates supplemental output (do NOT prepend header info)
*   bt     == TRUE  ... output is from print_backtrace()
*   level == Main screening filter. Note that LOG_WRITE passes all screens
*            except silent batch mode (LOG_DEBUG_NONE).
*   sub_level == Actual level represented by message content.
*
* Examples:
*                                                  level         sub_level
* - Force (unmodified) output into log stream: LOG_DEBUG_WRITE LOG_DEBUG_WRITE
* - Force ERROR output into log stream:        LOG_DEBUG_WRITE LOG_DEBUG_ERROR
* - ERROR output filtered by log-level:        LOG_DEBUG_ERROR LOG_DEBUG_ERROR
*
*   fstr == format string in printf format
*   args == arguments for printf format string
*
*********************************************************************/
static void 
    log_common_internal (boolean header, boolean bt,
			 log_debug_t level, log_debug_t sub_level,
			 const char *fstr, va_list args)
{
    const char *local_fstr = fstr;
    FILE    *out;
    xmlChar tstampbuff[TSTAMP_MIN_SIZE];

    out = log_output_file(sub_level);

    if (bt || ((level == LOG_DEBUG_WRITE) && (sub_level == LOG_DEBUG_WRITE))) {
        /*
	 * "Force" unmodified text into stdout/stderr/logfile per current
	 * behavior. This is for supplemental text, like extended
	 * output from parsing.
	 *
	 * Note that all backtrace output comes here since it never wants
	 * a custom header pre-pended.
	 */
	vfprintf(out, fstr, args);
	fflush(out);

    } else {

        /*
	 * "Non-forced" main line and appended output come here. The former
	 * (main line) may be prepended with custom header info. The latter
	 * (appended) never includes header info, and may also have control
	 * characters filtered.
	 */
        if (header) {
	    /*
	     * This is "main line" output and may require a header. If so,
	     * reformat so the log output doesn't end up on the next line.
	     */
	    if (cfg_log_custom) {
		/* Strip leading carriage returns, if any */
		while (*local_fstr == '\n') {
		    local_fstr++;
		}
		/* Make sure there's something left to output */
		if (*local_fstr == 0) {
		    return;
		}
	    }

	} else {

	    /*
	     * This is appended output ... we might do a bit of
	     * reformatting, if requested ...
	     */
	    if (cfg_log_suppress_ctrl) {
	        /* Strip leading carriage returns, if any */
	        while (*local_fstr == '\n') {
		    local_fstr++;
		}
		/* Make sure there's something left to output */
		if (*local_fstr == 0) {
		    return;
		}
	    }
	}

	/*
	 * Output (possibly reformatted) log entry to logfile or
	 * stdout/stderr. Prepend custom header info if so configured.
	 */
	if (header && cfg_log_custom) {
	    if (cfg_log_localtime) {
	        tstamp_local_datetime(tstampbuff);
	    } else {
	        tstamp_datetime(tstampbuff);
	    }
	    fprintf(out, "\n[%s] [%s] ", tstampbuff,
		    log_get_debug_level_string(sub_level));
	}

	vfprintf(out, local_fstr, args);
	fflush(out);
    }

}  /* log_common_internal */


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
void 
    log_common (boolean recursive, log_debug_t level, log_debug_t sub_level,
		const char *fstr, va_list args)
{
    log_stream_t stream;

    if (log_get_debug_level() < level) {
        return;
    }

    log_common_internal(TRUE, recursive, level, sub_level, fstr, args);

    if (recursive) {
        return; /* No need to call backtrace again */
    }

    if ((level != LOG_DEBUG_WRITE) || (sub_level != LOG_DEBUG_WRITE)) {

        /* Check if SOME output stream wants backtrace output */
        if (log_do_backtrace(sub_level, ANY_BT_FLAGS)) {

	    stream = log_output_stream(sub_level);

	    /* Yes ... now check if THIS output stream wants the output */
	    if (((stream == LOG_STREAM_LOGFILE) &&
		 log_do_backtrace(sub_level, cfg_log_backtrace_logfile)) ||
		((stream == LOG_STREAM_STDERR) &&
		 log_do_backtrace(sub_level, cfg_log_backtrace_stderr)) ||
		((stream == LOG_STREAM_STDOUT) &&
		 log_do_backtrace(sub_level, cfg_log_backtrace_stdout))) {

	      log_print_backtrace(sub_level, &log_common, &log_append,
				  &log_flush, cfg_log_backtrace_max_detail,
				  FRAME_OVERHEAD_CNT, "\n--Backtrace: ");
	    }
	}
    }

}  /* log_common */


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
void 
    log_append (boolean recursive, log_debug_t level, log_debug_t sub_level,
		const char *fstr, va_list args)
{
    if (log_get_debug_level() == LOG_DEBUG_NONE) {
        return;
    }

    if (log_get_debug_level() < level) {
        return;
    }

    log_common_internal(FALSE, recursive, level, sub_level, fstr, args);

    if (recursive) return; /* Weird compiler issue: Effectively a NOP */

}  /* log_append */


/********************************************************************
* FUNCTION log_vlog_common
*
*   Shared common log code
*
* INPUTS:
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   valist == any additional arguments for printf
*
*********************************************************************/
static void
    log_vlog_common (log_debug_t level, log_debug_t sub_level,
		     const char *fstr, va_list args)
{
    va_list args_copy;

    va_copy(args_copy, args);
    (*logfn_va_common)(FALSE, level, sub_level, fstr, args_copy);
    va_end(args_copy);

    if (cfg_log_mirroring && (cfg_log_syslog || cfg_log_vendor)) {
        va_copy(args_copy, args);
	log_common(FALSE, level, sub_level, fstr, args_copy);
	va_end(args_copy);
    }

} /* log_vlog_common */

static void
    log_vlog_append (log_debug_t level, log_debug_t sub_level,
		     const char *fstr, va_list args)
{
    va_list args_copy;

    va_copy(args_copy, args);
    (*logfn_va_append)(FALSE, level, sub_level, fstr, args_copy);
    va_end(args_copy);

    if (cfg_log_mirroring && (cfg_log_syslog || cfg_log_vendor)) {
        va_copy(args_copy, args);
	log_append(FALSE, level, sub_level, fstr, args_copy);
	va_end(args_copy);
    }

} /* log_vlog_append */


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
void 
    log_flush (void)
{
    (*logfn_flush)(); /* Flush the main output stream */

    if (log_get_mirroring()) {
        log_flush_internal(); /* Flush native stream if in use */
    }
}


/********************************************************************
* FUNCTION log_common_va_list
*
*   Process a main message variable argument list and pass it on
*   to the configured output stream.
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   ... == variable argument list for format string
*
*********************************************************************/
void 
    log_common_va_list (boolean recursive,
			log_debug_t level, log_debug_t sub_level,
			logfn_cmn_va_t logfn, const char *fstr, ...)
{
    va_list args;

    va_start(args, fstr);
    (*logfn)(recursive, level, sub_level, fstr, args );
    va_end(args);
}


/********************************************************************
* FUNCTION log_append_va_list
*
*   Process an appended message variable argument list and pass it on
*   to the configured output stream.
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   ... == variable argument list for format string
*
*********************************************************************/
void 
    log_append_va_list (boolean recursive,
			log_debug_t level, log_debug_t sub_level,
			logfn_app_va_t logfn, const char *fstr, ...)
{
    va_list args;

    va_start(args, fstr);
    (*logfn)(recursive, level, sub_level, fstr, args );
    va_end(args);
}

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

/* Obtain a backtrace and print it to the log stream managed by logfn_xxx */
void
    log_print_backtrace (log_debug_t orig_sub_level,
			 logfn_cmn_va_t logfn_common,
			 logfn_app_va_t logfn_append,
			 logfn_flush_t localfn_flush,
			 boolean max_detail,
			 uint frame_overhead,
			 const char *preamble)
{
    uint size;
    char **bt_strings;
    char *bt_str;
    uint i;

    if ((!cfg_log_frame_cnt) || (cfg_log_frame_cnt > bt_max_frames)) {
        return;
    }

#ifdef WITH_BACKTRACE
    void *array[bt_max_frames+1];
    size = backtrace(array, cfg_log_frame_cnt);
    bt_strings = backtrace_symbols(array, size);
#else
    return;
#endif

    if (bt_strings == NULL) {
        LOG_INTERNAL_ERR(Insufficient memory for backtrace(), CONTINUE);
	return;
    }
    malloc_cnt++;

    log_common_va_list(TRUE, LOG_DEBUG_WRITE, orig_sub_level,
		       logfn_common, "%s", preamble);

    for (i = 0; i < size; i++) {
        bt_str = bt_strings[i];

	if (!max_detail) {
	    while (*bt_str != '[') { /* Step past module info to "[0xADDR]" */
	        bt_str++;
	    }
	}

	if (i < frame_overhead) { /* Skip topmost frames (in logger) */
	    continue;
	}

	/* Now append the next stack frame detail, e.g., "[0xDEADBEEF]" */
        log_append_va_list(TRUE, LOG_DEBUG_WRITE, orig_sub_level,
			   logfn_append, "%s%s", bt_str,
			   max_detail ? "  " : "");
    }

    (*localfn_flush)(); /* Force completed backtrace out */

    free(bt_strings);
    free_cnt++;

} /* log_print_backtrace */

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
/* Only need to skip 2 frames calling from here */
#define LOG_FRAME_OVERHEAD_CNT 2

void
    log_backtrace (log_debug_t level, const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < level) {
        return;
    }

    va_start(args, fstr);

    log_vlog_common(level, level, fstr, args );

    if (!cfg_log_backtrace) { /* No need to do this again if just done */

        log_print_backtrace(level, logfn_va_common, logfn_va_append,
			    logfn_flush, cfg_log_backtrace_max_detail,
			    LOG_FRAME_OVERHEAD_CNT, "\n--Backtrace: ");

        if (cfg_log_mirroring && (cfg_log_syslog || cfg_log_vendor)) {
	    log_print_backtrace(level, &log_common, &log_append, &log_flush,
				cfg_log_backtrace_max_detail,
				LOG_FRAME_OVERHEAD_CNT, "\n--Backtrace: ");

	}
    } 

    va_end(args);
}

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
void 
    log_stdout (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() == LOG_DEBUG_NONE) {
        return;
    }

    va_start(args, fstr);
    vprintf(fstr, args);
    fflush(stdout);
    va_end(args);

}  /* log_stdout */


/********************************************************************
* FUNCTION log_write
*
*   Generate a log entry, regardless of log level (unless silent)
*
* INPUTS:
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
void 
    log_write (const char *fstr, ...)
{
    va_list args;

    /* Screen out if in silent (batch) mode: */
    if (log_get_debug_level() == LOG_DEBUG_NONE) {
        return;
    }

    /* Force output unmodified into the log stream */
    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_WRITE, LOG_DEBUG_WRITE, fstr, args);
    va_end(args);

}  /* log_write */

void 
    log_write_append (const char *fstr, ...)
{
    va_list args;

    /* Screen out if in silent (batch) mode: */
    if (log_get_debug_level() == LOG_DEBUG_NONE) {
        return;
    }

    /* Append output unmodified into the log stream */
    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_WRITE, LOG_DEBUG_WRITE, fstr, args);
    va_end(args);

}  /* log_write_append */


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
void 
    log_audit_write (const char *fstr, ...)
{
    va_list args;

    va_start(args, fstr);

    if (auditlogfile != NULL) {
        vfprintf(auditlogfile, fstr, args);
        fflush(auditlogfile);
    }

    va_end(args);

}  /* log_audit_write */


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
void 
    log_alt_write (const char *fstr, ...)
{
    va_list args;

    va_start(args, fstr);

    if (altlogfile) {
        vfprintf(altlogfile, fstr, args);
        fflush(altlogfile);
    }

    va_end(args);

}  /* log_alt_write */


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
void
    log_alt_indent (int32 indentcnt)
{
    int32  i;

    if (indentcnt >= 0) {
        log_alt_write("\n");
        for (i=0; i<indentcnt; i++) {
            log_alt_write(" ");
        }
    }

} /* log_alt_indent */

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
    vlog_error (const char *fstr, va_list args )
{
    (*logfn_va_common)(FALSE, LOG_DEBUG_ERROR, LOG_DEBUG_ERROR, fstr, args );

} /* vlog_error */


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
void log_error (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_ERROR) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_ERROR, LOG_DEBUG_ERROR, fstr, args );
    va_end(args);

}  /* log_error */


void 
    log_error_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_ERROR) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_ERROR, LOG_DEBUG_ERROR, fstr, args);
    va_end(args);

}  /* log_error_append */


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
void 
    log_warn (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_WARN) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_WARN, LOG_DEBUG_WARN, fstr, args);
    va_end(args);

}  /* log_warn */


void 
    log_warn_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_WARN) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_WARN, LOG_DEBUG_WARN, fstr, args);
    va_end(args);

}  /* log_warn_append */


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
void 
    log_info (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_INFO) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_INFO, LOG_DEBUG_INFO, fstr, args);
    va_end(args);

}  /* log_info */


void 
    log_info_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_INFO) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_INFO, LOG_DEBUG_INFO, fstr, args);
    va_end(args);

}  /* log_info_append */


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
void 
    log_debug (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_DEBUG, LOG_DEBUG_DEBUG, fstr, args);
    va_end(args);

}  /* log_debug */


void 
    log_debug_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_DEBUG, LOG_DEBUG_DEBUG, fstr, args);
    va_end(args);

}  /* log_debug_append */


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
void 
    log_debug2 (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG2) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_DEBUG2, LOG_DEBUG_DEBUG2, fstr, args);
    va_end(args);

}  /* log_debug2 */

void 
    log_debug2_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG2) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_DEBUG2, LOG_DEBUG_DEBUG2, fstr, args);
    va_end(args);

}  /* log_debug2_append */


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
void 
    log_debug3 (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG3) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_DEBUG3, LOG_DEBUG_DEBUG3, fstr, args);
    va_end(args);

}  /* log_debug3 */

void 
    log_debug3_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG3) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_DEBUG3, LOG_DEBUG_DEBUG3, fstr, args);
    va_end(args);

}  /* log_debug3_append */


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
void 
    log_debug4 (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG4) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_DEBUG4, LOG_DEBUG_DEBUG4, fstr, args);
    va_end(args);

}  /* log_debug4 */

void 
    log_debug4_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEBUG4) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_DEBUG4, LOG_DEBUG_DEBUG4, fstr, args);
    va_end(args);

}  /* log_debug4_append */


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
void 
    log_dev0 (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEV0) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_DEV0, LOG_DEBUG_DEV0, fstr, args);
    va_end(args);

}  /* log_dev0 */


void 
    log_dev0_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEV0) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_DEV0, LOG_DEBUG_DEV0, fstr, args);
    va_end(args);

}  /* log_dev0_append */


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
void 
    log_dev1 (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEV1) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(LOG_DEBUG_DEV1, LOG_DEBUG_DEV1, fstr, args);
    va_end(args);

}  /* log_dev1 */

void 
    log_dev1_append (const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < LOG_DEBUG_DEV1) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(LOG_DEBUG_DEV1, LOG_DEBUG_DEV1, fstr, args);
    va_end(args);

}  /* log_dev1_append */


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
void 
    log_write_level (log_debug_t level, const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < level) {
        return;
    }

    va_start(args, fstr);
    log_vlog_common(level, level, fstr, args);
    va_end(args);

}  /* log_write_level */


void 
log_write_level_append (log_debug_t level, const char *fstr, ...)
{
    va_list args;

    if (log_get_debug_level() < level) {
        return;
    }

    va_start(args, fstr);
    log_vlog_append(level, level, fstr, args);
    va_end(args);

}  /* log_write_level_append */


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
void 
    log_noop (const char *fstr, ...)
{
    (void)fstr;
}  /* log_noop */


/********************************************************************
* FUNCTION log_set_debug_level
* 
* Set the global debug filter threshold level
* 
* INPUTS:
*   dlevel == desired debug level
*
*********************************************************************/
void
    log_set_debug_level (log_debug_t dlevel)
{
    if (dlevel > LOG_DEBUG_DEBUG4) {
        dlevel = LOG_DEBUG_DEBUG4;
    }

    if (dlevel != debug_level) {
#ifdef LOG_DEBUG_TRACE
        log_write("\n\nChanging log-level from '%s' to '%s'\n",
                  log_get_debug_level_string(debug_level),
                  log_get_debug_level_string(dlevel));
#endif
        debug_level = dlevel;
    }

}  /* log_set_debug_level */


/********************************************************************
* FUNCTION log_get_debug_level
* 
* Get the global debug filter threshold level
* 
* RETURNS:
*   the global debug level
*********************************************************************/
log_debug_t
    log_get_debug_level (void)
{
    return debug_level;

}  /* log_get_debug_level */


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
log_debug_t
    log_get_debug_level_enum (const char *str)
{
#ifdef DEBUG
    if (!str) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return LOG_DEBUG_NONE;
    }
#endif

    if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_OFF)) {
        return LOG_DEBUG_OFF;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_WRITE)) {
        return LOG_DEBUG_WRITE;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_ERROR)) {
        return LOG_DEBUG_ERROR;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_WARN)) {
        return LOG_DEBUG_WARN;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_INFO)) {
        return LOG_DEBUG_INFO;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG)) {
        return LOG_DEBUG_DEBUG;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG2)) {
        return LOG_DEBUG_DEBUG2;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG3)) {
        return LOG_DEBUG_DEBUG3;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG4)) {
        return LOG_DEBUG_DEBUG4;

	/* Seldom used, so last ... */
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEV0)) {
        return LOG_DEBUG_DEV0;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEV1)) {
        return LOG_DEBUG_DEV1;
    } else {
        return LOG_DEBUG_NONE;
    }

}  /* log_get_debug_level_enum */

/********************************************************************
* FUNCTION log_parse_debug_level_str
* 
* Get the corresponding debug enum bit value for the specified string
* 
* INPUTS:
*   str == string value to convert to a bit mask
*
* RETURNS:
*   the corresponding bit mask for the specified debug level
*********************************************************************/
uint
    log_parse_debug_level_str (const char *str)
{

#ifdef DEBUG
    if (!str) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return 0;
    }
#endif

    if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_OFF)) {
        return 0;
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_WRITE)) {
        return log_cvt_debug_level2bit(LOG_DEBUG_WRITE);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_ERROR)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_ERROR);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_WARN)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_WARN);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_INFO)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_INFO);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_DEBUG);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG2)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_DEBUG2);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG3)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_DEBUG3);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEBUG4)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_DEBUG4);

	/* Seldom used, so last ... */
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEV0)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_DEV0);
    } else if (!xml_strcmp((const xmlChar *)str, LOG_DEBUG_STR_DEV1)) {
        return  log_cvt_debug_level2bit(LOG_DEBUG_DEV1);
    } else {
	return 0;
    }

}  /* log_parse_debug_level_str */



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
const xmlChar *
    log_get_debug_level_string (log_debug_t level)
{
    switch (level) {
    case LOG_DEBUG_NONE:
    case LOG_DEBUG_OFF:
        return LOG_DEBUG_STR_OFF;
    case LOG_DEBUG_WRITE:
        return LOG_DEBUG_STR_WRITE;
    case LOG_DEBUG_DEV0:
        return LOG_DEBUG_STR_DEV0;
    case LOG_DEBUG_ERROR:
        return LOG_DEBUG_STR_ERROR;
    case LOG_DEBUG_WARN:
        return LOG_DEBUG_STR_WARN;
    case LOG_DEBUG_INFO:
        return LOG_DEBUG_STR_INFO;
    case LOG_DEBUG_DEV1:
        return LOG_DEBUG_STR_DEV1;
    case LOG_DEBUG_DEBUG:
        return LOG_DEBUG_STR_DEBUG;
    case LOG_DEBUG_DEBUG2:
        return LOG_DEBUG_STR_DEBUG2;
    case LOG_DEBUG_DEBUG3:
        return LOG_DEBUG_STR_DEBUG3;
    case LOG_DEBUG_DEBUG4:
        return LOG_DEBUG_STR_DEBUG4;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return LOG_DEBUG_STR_OFF;
    }
    /*NOTREACHED*/

}  /* log_get_debug_level_string */


/********************************************************************
* FUNCTION log_debug_get_app_string
* 
* Get the corresponding string for the app for use by syslog()
* 
* INPUTS:
*   level ==  the enum for the specified YUMA app
*
* RETURNS:
*   the string value for this enum

*********************************************************************/
const char *
    log_debug_get_app_string (log_debug_app_t app)
{
    switch (app) {
    case LOG_DEBUG_APP_NONE:
        return LOG_DEBUG_APP_STR_UNKNOWN;
    case LOG_DEBUG_APP_YANGCLI:
        return LOG_DEBUG_APP_STR_YANGCLI;
    case LOG_DEBUG_APP_YANGDUMP:
        return LOG_DEBUG_APP_STR_YANGDUMP;
    case LOG_DEBUG_APP_YANGDIFF:
        return LOG_DEBUG_APP_STR_YANGDIFF;
    case LOG_DEBUG_APP_NETCONFD:
        return LOG_DEBUG_APP_STR_NETCONFD;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return LOG_DEBUG_APP_NONE;
    }
    /*NOTREACHED*/

}  /* log_get_app_string */


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
int
    log_debug_app2facility (log_debug_app_t app)
{
    switch (app) {
    case LOG_DEBUG_APP_NONE:
    case LOG_DEBUG_APP_YANGCLI:
    case LOG_DEBUG_APP_YANGDUMP:
    case LOG_DEBUG_APP_YANGDIFF:
        return LOG_USER;
    case LOG_DEBUG_APP_NETCONFD:
        return LOG_DAEMON;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return LOG_USER;
    }
    /*NOTREACHED*/

}  /* log_debug_app2facility */


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
void
    log_set_debug_app (log_debug_app_t app)
{
    if (VALID_DEBUG_APP(app)) {
        debug_app = app;
    } else {
        debug_app = LOG_DEBUG_APP_NONE;
    }
}

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
int
    log_get_debug_app (void)
{
    return(debug_app);
}

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
const char *
    log_debug_app_string (void)
{
    return (log_debug_get_app_string(debug_app));
}

/********************************************************************
* FUNCTION log_is_open
* 
* Check if the logfile is active
* 
* RETURNS:
*   TRUE if logfile open, FALSE otherwise
*********************************************************************/
boolean
    log_is_open (void)
{
    return (logfile) ? TRUE : FALSE;

}  /* log_is_open */


/********************************************************************
* FUNCTION log_indent
* 
* Printf a newline, then the specified number of chars
*
* INPUTS:
*    indentcnt == number of indent chars, -1 == skip everything
*
*********************************************************************/
void
    log_indent (int32 indentcnt)
{
    int32  i;

    if (indentcnt >= 0) {
        log_write("\n");
        for (i=0; i<indentcnt; i++) {
            log_write(" ");
        }
    }

} /* log_indent */

void
    log_indent_append (int32 indentcnt)
{
    int32  i;

    if (indentcnt >= 0) {
        log_write_append("\n");
        for (i=0; i<indentcnt; i++) {
            log_write_append(" ");
        }
    }

} /* log_indent_append */


/********************************************************************
* FUNCTION log_stdout_indent
* 
* Printf a newline to stdout, then the specified number of chars
*
* INPUTS:
*    indentcnt == number of indent chars, -1 == skip everything
*
*********************************************************************/
void
    log_stdout_indent (int32 indentcnt)
{
    int32  i;

    if (indentcnt >= 0) {
        log_stdout("\n");
        for (i=0; i<indentcnt; i++) {
            log_stdout(" ");
        }
    }

} /* log_stdout_indent */


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
FILE *
    log_get_logfile (void)
{
    return logfile;

}  /* log_get_logfile */


/* END file log.c */

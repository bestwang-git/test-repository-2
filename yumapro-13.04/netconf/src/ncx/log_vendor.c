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
/*  FILE: log_vendor.c

Vendor specifc code and API. Note: the first vendor is still TBD. For
now, we use syslog as a framework.

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
08jan06      abb      begun, borrowed from openssh code
02jun12      mts      adapted from log.c

         1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <syslog.h>
#include <memory.h>

#include "procdefs.h"
#include "log.h"
#include "log_util.h"
#include "log_syslog.h"
#include "log_vendor.h"
#include "log_vendor_extern.h"
#include "status.h"
#include "tstamp.h"
#include "xml_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

// #define LOG_DEBUG_VERBOSE 1

/********************************************************************
*                                                                   *
*               T Y P E   D E C L A R A T I O N S                   *
*                                                                   *
*********************************************************************/

/********************************************************************
*                                                                   *
*                         V A R I A B L E S                         *
*                                                                   *
*********************************************************************/

static syslog_msg_desc_t *logP = NULL;

/********************************************************************
*                                                                   *
*                         F U N C T I O N S                         *
*                                                                   *
*********************************************************************/

/********************************************************************
* FUNCTION log_vendor_cleanup
*
*   Cleanup vendor related logging state, if any.
*
*   Note: Invocation should be delayed to the  LAST POSSIBLE MOMENT.
*   This is because the various cleanup routines often want to log
*   errors/warnings/info as well various debugs using standard logging.
*
*********************************************************************/
void
    log_vendor_cleanup ( void )
{
    if (logP) {
        m__free(logP);
	logP = NULL;
    }
    logfn_vendor_send = NULL;
    log_clr_vendor_bfr_allocated();

}

/********************************************************************
* FUNCTION log_vendor_connect
*
*   Connect to vendor log output stream TBD. If and when a vendor needs
*   some form of "connect" (e.g., how syslog works) an API will be defined
*   and this function will call the vendor's registered "connect" function.
*
*********************************************************************/
void
    log_vendor_connect (void)
{
    /* Nothing to do on vendor side for now */
    return;
}


/********************************************************************
* FUNCTION log_vendor_send_to_syslog
*
*   Send vendor log data to syslog output stream.
*
*   This is used to allow internal testing of the --log-vendor param
*   when no actual vendor callback is present.
*
*********************************************************************/
void
    log_vendor_send_to_syslog (log_debug_app_t app, log_debug_t level,
			       const char *fstr, va_list args)
{
    int syslog_prio;

    if (!log_syslog_connected()) {
        log_syslog_connect();
    }

    syslog_prio = cvt_level2syslog_prio(level);

    vsyslog(syslog_prio, fstr, args);

    /* Compiler fakeout ... app param is not used directly by syslog */
    if (FALSE) {
        printf("%d", app);
    }

}


/********************************************************************
* FUNCTION log_vendor_test_send_to_syslog
*
*   Pass vendor log data to syslog, but pretend to be a vendor callback.
*   This is for internal testing only.
*
*********************************************************************/
void
    log_vendor_test_send_to_syslog (log_debug_app_t app, log_debug_t level,
			       const char *fstr, va_list args)
{
    log_vendor_send_to_syslog(app, level, fstr, args);
}

/********************************************************************
* FUNCTION log_vendor_send
*
*   Send log data to vendor log output stream
*
*********************************************************************/
void
    log_vendor_send (log_debug_app_t app, log_debug_t level,
		     const char *fstr, ...)
{
    va_list args;

    if (!logfn_vendor_send) {
	return;
    }

    va_start(args, fstr);
    (*logfn_vendor_send)(app, level, fstr, args);
    va_end(args);
}


/********************************************************************
* FUNCTION log_vendor_flush
*
*   Flush contents of internal buffer to vendor logging system
*
* INPUTS:
*   None
*
*********************************************************************/
void 
    log_vendor_flush (void)
{
    if (logP == NULL) {
        return;
    }

    log_util_flush(logP);
}

/********************************************************************
* FUNCTION log_vendor_common
*
*   Generate a log entry destined for vendor specific processing
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == YumaPro log level
*   sub_level == YumaPro log sub-level
*   fstr == format string in printf format
*   args == any additional arguments for printf
*
*********************************************************************/
void 
    log_vendor_common (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args)
{
    log_debug_app_t app = log_get_debug_app();

    if (logP == NULL) { /***** MOVE ME *****/

        logP = m__getObj(syslog_msg_desc_t);
	if (!logP) {
	    LOG_INTERNAL_ERR(Vendor output buffer malloc failure, TBD);
	    return;
	}

	log_set_vendor_bfr_allocated();
	memset(logP, 0x0, sizeof *logP);
	//logP->initialized = FALSE;
	logP->first_time = TRUE;   /* Bypass integrity check if first init */
        log_util_init_buf(logP);   /* First init */

	/*
	 * Ideally, the vendor callback function has already have been
	 * set to a vendor supplied function at this point. (If so, the
	 * the following call is a NOP.) However, if vendor code is late
	 * to register, or in the complete absence of actual vendor code
	 * capable of registering, redirect to syslog. This allows
	 * --log-vendor testing without actual vendor code (because
	 * the interface is the same).
	 */
	log_vendor_extern_register_send_fn(log_vendor_send_to_syslog);
    }

    /*
     * Call common buffering code:
     */
    log_util_logbuf_common(logP, app, level, sub_level, fstr, args);

    /* Bail if this was backtrace output */
    if (recursive) {
        return;
    }

    /*
     * If this was a conventional log message, and if backtrace
     * is configured for vendor, then do the callback. Note that
     * this becomes a reentrant call, but we protect ourselves
     * via the early exit above.
     */
    if ((level != LOG_DEBUG_WRITE) || (sub_level != LOG_DEBUG_WRITE)) {
        if (log_do_backtrace(sub_level, log_get_backtrace_vendor())) {
	    /* Call recursively */
	    log_print_backtrace(sub_level, &log_vendor_common, 
				&log_vendor_append, &log_vendor_flush,
				log_get_backtrace_detail(),
				FRAME_OVERHEAD_CNT, "\n--Backtrace: ");
	}
    }

} /* log_vendor_common */


/********************************************************************
* FUNCTION log_vendor_append
*
*   Append formatted string output to the internal vendor log buffer
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   args == any additional arguments for printf
*
*********************************************************************/
void
    log_vendor_append (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args)
{
    log_debug_app_t app = log_get_debug_app();

    if (logP == NULL) {
        return;
    }

    log_util_logbuf_append(logP, app, level, sub_level, fstr, args);

    if (recursive) {
        return;
    }

}  /* log_vendor_append */

/* END file log_vendor.c */

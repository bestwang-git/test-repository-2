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
/*  FILE: log_syslog.c

Syslog specific code and API.
                
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
static boolean syslog_connected = FALSE;


/********************************************************************
* FUNCTION cvt_level2syslog_prio
*
*   Convert from log_debug (native) level to syslog priority
*
* INPUTS:
*   level: Native debug level
*
* RETURNS:
*   Mapping to syslog priority (see syslog.h)
*
*********************************************************************/
int
    cvt_level2syslog_prio (log_debug_t level)
{
    switch (level) {

    case LOG_DEBUG_OFF:
    case LOG_DEBUG_NONE:
        return LOG_SYSLOG_PRIO_NONE;
    case LOG_DEBUG_WRITE:
        return LOG_INFO;
    case LOG_DEBUG_ERROR:
        return LOG_ERR;
    case LOG_DEBUG_WARN:
        return LOG_WARNING;
    case LOG_DEBUG_INFO:
        return LOG_INFO;
    case LOG_DEBUG_DEV0:
    case LOG_DEBUG_DEV1:
    case LOG_DEBUG_DEBUG:
    case LOG_DEBUG_DEBUG2:
    case LOG_DEBUG_DEBUG3:
    case LOG_DEBUG_DEBUG4:
        return LOG_DEBUG;

    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return LOG_SYSLOG_PRIO_NONE;
    }
    /*NOTREACHED*/
}


/********************************************************************
* FUNCTION log_syslog_close
*
*   Close down syslog
*
*********************************************************************/
void
    log_syslog_close (void)
{
    if (syslog_connected) {
        closelog();
	syslog_connected = FALSE;
    }
}


/********************************************************************
* FUNCTION log_syslog_cleanup
*
*   Cleanup syslog related logging state, if any.
*
*   Note: Invocation should be delayed to the  LAST POSSIBLE MOMENT.
*   This is because the various cleanup routines often want to log
*   errors/warnings/info as well various debugs using standard logging.
*
*********************************************************************/
void
    log_syslog_cleanup (void)
{
    if (logP) {
	log_syslog_close();
        m__free(logP);
	logP = NULL;
	log_clr_syslog_bfr_allocated();
    }

    return;
}


/********************************************************************
* FUNCTION log_syslog_connected
*
*   Return syslog connect status
*
*********************************************************************/
boolean
    log_syslog_connected (void)
{
    return syslog_connected;
}


/********************************************************************
* FUNCTION log_syslog_connect
*
*   Connect to syslog output stream via openlog()
*
*********************************************************************/
void
    log_syslog_connect (void)
{
    openlog(log_debug_app_string(), (LOG_PID|LOG_NDELAY),
	    log_debug_app2facility(log_get_debug_app()));
    syslog_connected = TRUE;
}


/********************************************************************
* FUNCTION log_syslog_send
*
*   Send log data to syslog via vsyslog()
*
* INPUTS:
*    app   == YumaPro application (such as netconfd-pro or yangcli-pro)
*             This info is already known to syslog via the connect.
*    level == YumaPro log message level. This will be translated
*              appropriately into a syslog equivalent
*    start == ptr to format string
*    ...   == va_list
*
*********************************************************************/
void
    log_syslog_send (log_debug_app_t app, log_debug_t level,
		     const char *fstr, ...)
{
    int syslog_prio;
    va_list args;

    /* Note that syslog already knows "app" (via connect) */

    syslog_prio = cvt_level2syslog_prio(level);

    va_start(args, fstr);
    vsyslog(syslog_prio, fstr, args);
    va_end(args);

    if (FALSE) {           /* Is there a better way to do this ??? */
        printf("%d", app); /* Fiddle compiler error (unused param) */
    }
}

/********************************************************************
* FUNCTION log_syslog_flush
*
*   External interface to force contents of current syslog buffer out to the
*   syslog daemon.
*
*********************************************************************/
void 
    log_syslog_flush (void)
{
    if (logP == NULL) {
        return;
    }
    log_util_flush(logP);
}

/********************************************************************
* FUNCTION log_syslog_common
*
*   Generate log output
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == YumaPro log level
*   sub_level == YumaPro log sub-level
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
void 
    log_syslog_common (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args)
{
    log_debug_app_t app = log_get_debug_app();

    if (logP == NULL) { /***** MOVE ME *****/
        logP = m__getObj(syslog_msg_desc_t);
	if (!logP) {
	    LOG_INTERNAL_ERR(Syslog output buffer malloc failure, TBD);
	    return;
	}
	log_set_syslog_bfr_allocated();
	memset(logP, 0x0, sizeof *logP);
	//logP->initialized = FALSE;
	logP->first_time = TRUE;   /* Bypass integrity check if first init */
	log_util_init_buf(logP);   /* First init */
    }

    log_util_logbuf_common(logP, app, level, sub_level,
    			   fstr, args);

    /* Bail if this was backtrace output */
    if (recursive) {
        return;
    }

    /*
     * If this was a conventional log message, and if backtrace
     * is configured for syslog, then do the callback. Note that
     * this becomes a reentrant call, but we protect ourselves
     * via the early exit above.
     */
    if ((level != LOG_DEBUG_WRITE) || (sub_level != LOG_DEBUG_WRITE)) {
        if (log_do_backtrace(sub_level, log_get_backtrace_syslog())) {
	    /* Call recursively */
	    log_print_backtrace(sub_level, &log_syslog_common,
				&log_syslog_append, &log_syslog_flush,
				log_get_backtrace_detail(),
				FRAME_OVERHEAD_CNT, "\n--Backtrace: ");
	}
    }
}


/********************************************************************
* FUNCTION log_syslog_append
*
*   Append formatted string output to the internal syslog log buffer
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
void
    log_syslog_append (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args)
{ 
    int syslog_prio = cvt_level2syslog_prio(level);

    if (logP == NULL) {
        return;
    }

    log_util_logbuf_append(logP, syslog_prio, level, sub_level, fstr, args);

    if (recursive) {
        return;
    }

} /* log_syslog_append */

/* END file log_syslog.c */

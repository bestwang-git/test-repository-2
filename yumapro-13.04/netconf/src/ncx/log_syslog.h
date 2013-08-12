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
#ifndef _H_log_syslog
#define _H_log_syslog
/*  FILE: log_syslog.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    Syslog logging manager API

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
08-jan-06    abb      begun
02-jun-2012  mts      adapted from log.h
*/

#include <stdio.h>
#include <xmlstring.h>

#include "procdefs.h"
#include "status.h"
#include "log.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/
#define LOG_SYSLOG_PRIO_NONE -1;

/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/

/********************************************************************
* FUNCTION log_syslog_close
*
*   Close down syslog connection
*
*********************************************************************/
extern void
    log_syslog_close (void);


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
    log_syslog_cleanup (void);

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
extern int
    cvt_level2syslog_prio (log_debug_t level);


/********************************************************************
* FUNCTION log_syslog_connected
*
*   Return syslog connect status
*
*********************************************************************/
extern boolean
    log_syslog_connected (void);


/********************************************************************
* FUNCTION log_syslog_connect
*
*   Connect to syslog output stream via openlog()
*
*********************************************************************/
extern void
    log_syslog_connect (void);


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
extern void
log_syslog_send (log_debug_app_t, log_debug_t level, const char *fstr, ...)
                 __attribute__ ((format (printf, 3, 4)));


/********************************************************************
* FUNCTION log_syslog_flush
*
*   Flush contents of internal buffer to vendor logging system
*
* INPUTS:
*   None
*
*********************************************************************/
extern void 
    log_syslog_flush (void);


/********************************************************************
* FUNCTION log_syslog_common
*
*   Generate a log entry destined for vendor specific processing
*
* INPUTS:
*   recursive == TRUE means this is a recursive callback from print_backtrace()
*   level == YumaPro log level
*   sub_level == YumaPro log sub-level
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_syslog_common (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args);


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
extern void
    log_syslog_append (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_log_syslog */

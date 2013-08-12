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
#ifndef _H_log_vendor
#define _H_log_vendor
/*  FILE: log_vendor.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    Logging manager vendor API

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
#include "log_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/

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
    log_vendor_cleanup ( void );

/********************************************************************
* FUNCTION log_vendor_connect
*
*   Connect to vendor log output stream (implementation TBD).
*
*********************************************************************/
extern void
    log_vendor_connect (void);

/********************************************************************
* FUNCTION log_vendor_send_to_syslog
*
*   Send vendor log data to syslog output stream.
*
*   This is used to allow internal testing of the --log-vendor param
*   when no actual vendor callback is present.
*
*********************************************************************/
extern void
    log_vendor_send_to_syslog (log_debug_app_t app, log_debug_t level,
			       const char *fstr, va_list args);
/********************************************************************
* FUNCTION log_vendor_test_send_to_syslog
*
*   Pass vendor log data to syslog, but pretend to be a vendor callback.
*   This is for internal testing only.
*
*********************************************************************/
extern void
    log_vendor_test_send_to_syslog (log_debug_app_t app, log_debug_t level,
				    const char *fstr, va_list args);

/********************************************************************
* FUNCTION log_vendor_send
*
*   Send log data to vendor log output stream
*
*********************************************************************/
extern void
log_vendor_send (log_debug_app_t app, log_debug_t level, const char *fstr, ...)
                 __attribute__ ((format (printf, 3, 4)));


/********************************************************************
* FUNCTION log_vendor_flush
*
*   Flush contents of internal buffer to vendor logging system
*
* INPUTS:
*   None
*
*********************************************************************/
extern void 
    log_vendor_flush (void);


/********************************************************************
* FUNCTION log_vendor_common
*
*   Generate a log entry destined for vendor specific processing
*
* INPUTS:
*   level == YumaPro log level
*   sub_level == YumaPro log sub-level
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_vendor_common (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args);


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
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void
    log_vendor_append (boolean recursive,
		       log_debug_t level, log_debug_t sub_level,
		       const char *fstr, va_list args);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_log_vendor */

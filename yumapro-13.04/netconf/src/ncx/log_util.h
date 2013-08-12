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
#ifndef _H_log_util
#define _H_log_util
/*  FILE: log_util.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    Logging manager API

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

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/

/*
 * Update of the internal syslog buffer is based on:
 *
 *       vsnprintf()
 *
 * This primitive expects the content area to be followed by an additional
 * byte in order to append a NULL terminator to a max size content string.
 * We in turn add yet another byte to store a well known marker byte in an
 * attempt to detect errors and aid debugging. (Not rigorous, but better
 * than nothing.)
 *
 * The below defs do this:
 *
 */

//#define SYSLOG_BUF_CONTENT_SIZE 64    /* For overflow testing */
#define SYSLOG_BUF_CONTENT_SIZE 1024    /* Max syslog buffer content size */
#define SYSLOG_BUF_NULL_BYTE_SIZE   1  /* Add a byte for a NULL terminator */
#define SYSLOG_BUF_MARKER_BYTE_SIZE 1  /* Add a byte for a well known marker */

#define SYSLOG_BUF_TOTAL_SIZE (SYSLOG_BUF_CONTENT_SIZE +                \
			       SYSLOG_BUF_NULL_BYTE_SIZE +		\
			       SYSLOG_BUF_MARKER_BYTE_SIZE)

#define SYSLOG_BUF_NULL_BYTE_INDEX (SYSLOG_BUF_CONTENT_SIZE +           \
				    SYSLOG_BUF_NULL_BYTE_SIZE - 1)

#define SYSLOG_BUF_MARKER_BYTE_INDEX (SYSLOG_BUF_CONTENT_SIZE +         \
				      SYSLOG_BUF_NULL_BYTE_SIZE +	\
				      SYSLOG_BUF_MARKER_BYTE_SIZE - 1)

#define LOG_PRIO_NONE -1

/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/* Syslog/Vendor Message Buffer descriptor */
typedef struct syslog_msg_desc_t_ {
    uint            idid;
    char           *start;
    char           *end;
    char           *ptr;
    uint32          len;
    int32           remaining;
    boolean         initialized;
    boolean         first_time;
    boolean         write_pending;
    log_debug_app_t app;
    log_debug_t     level;
    log_debug_t     sub_level;
    char            buf[SYSLOG_BUF_TOTAL_SIZE];
} syslog_msg_desc_t;

/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/

/********************************************************************
* FUNCTION log_util_init_buf
*
*   Initialize the contents of an internal log buffer descriptor
*
* INPUTS:
*   dp == addr of descriptor
*
*********************************************************************/

extern void 
    log_util_init_buf (syslog_msg_desc_t *dp);

/********************************************************************
* FUNCTION log_util_flush
*
*   Flush contents of internal buffer to external logging system
*
* INPUTS:
*   buf == pointer to a syslog msg descriptor
*
*********************************************************************/
extern void 
    log_util_flush (syslog_msg_desc_t *desc);


/********************************************************************
* FUNCTION log_util_logbuf_common
*
*   Send formatted string output to an internal log buffer
*
* INPUTS:
*   desc == Pointer to syslog msg buffer descriptor
*   app  == YumaPro app (like netconfd-pro or yangcli-pro). This will
*           be (or has been) translated by vendor or syslog into an
*           appropriate equivalent.
*   level == internal send priority
*   sub_level == internal content priority
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_util_logbuf_common (syslog_msg_desc_t *desc, log_debug_app_t app,
			    log_debug_t level, log_debug_t sub_level,
			    const char *fstr, va_list args);

/********************************************************************
* FUNCTION log_util_logbuf_append
*
*   Append formatted string output to an internal log buffer
*
* INPUTS:
*   desc == ptr to buffer descriptor
*   app  == YumaPro app (like netconfd-pro or yangcli-pro). This will
*           be (or has been) translated by vendor or syslog into an
*           appropriate equivalent.
*   level == internal logger "screening" level
*   sub_level == internal logger functional (content) level
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
extern void 
    log_util_logbuf_append (syslog_msg_desc_t *desc, log_debug_app_t app,
			    log_debug_t level, log_debug_t sub_level,
			    const char *fstr, va_list args);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_log_util */

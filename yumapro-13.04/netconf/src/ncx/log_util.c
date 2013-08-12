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
/*  FILE: log_util.c

Logger code utility routines.
                
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
#include "status.h"
#include "tstamp.h"
#include "xml_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define SYSLOG_BUF_ID 0x42424242
#define SYSLOG_BUF_MARKER 'X'

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

/********************************************************************
*                                                                   *
*                         F U N C T I O N S                         *
*                                                                   *
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
void 
    log_util_init_buf (syslog_msg_desc_t *dp)
{
    char              *np;               /* ptr to NULL */
    char              *mp;               /* ptr to marker */

    if (dp == NULL) {
	LOG_INTERNAL_ERR(Null buf ptr, RETURN);
        return;
    }

    /* No need to init twice */
    if (dp->initialized && (dp->idid == SYSLOG_BUF_ID)) {
        return;
    }

    if ((dp->idid != SYSLOG_BUF_ID) && (!dp->first_time)) {
        LOG_INTERNAL_ERR(Bad buf internal ID, REINIT);
    }

    if (dp->write_pending) {
        LOG_INTERNAL_BUF_ERR(SYSLOG BUF STATE (write_pending), dp, CONTINUE);
    }

    //memset(dp, 0x0, sizeof *dp);

    dp->idid = SYSLOG_BUF_ID;
    dp->first_time = FALSE;
    dp->start = dp->ptr = &dp->buf[0];
    dp->end = &dp->buf[SYSLOG_BUF_CONTENT_SIZE];
    dp->len = dp->remaining = SYSLOG_BUF_CONTENT_SIZE;
    dp->level = LOG_DEBUG_NONE;
    dp->sub_level = LOG_DEBUG_NONE;
    dp->write_pending = FALSE;
    np = &dp->buf[SYSLOG_BUF_NULL_BYTE_INDEX];
   *np = '\0'; /* NULL terminator */
    mp = &dp->buf[SYSLOG_BUF_MARKER_BYTE_INDEX];
   *mp = SYSLOG_BUF_MARKER;  /* marker byte */

    dp->initialized = TRUE;

}

/* Force syslog buffer re-initialization */
static void 
    log_util_reinit_buf (syslog_msg_desc_t *dp)
{
    if (dp == NULL) {
	LOG_INTERNAL_ERR(Null buf ptr, RETURN);
        return;
    }
    dp->initialized = FALSE;
    log_util_init_buf(dp);
}
/********************************************************************
* FUNCTION log_util_flush_internal
*
*   Force contents of the specified syslog buffer (syslog_buf) out to the
*   syslog daemon. Always add NULL terminator. (This should always work
*   since the buffer is 1 character longer than the max content string.)
*   Finally, reinitialize the descriptor before returning.
*
*********************************************************************/
static void 
    log_util_flush_internal (syslog_msg_desc_t *buf)
{
    if (buf->write_pending) {

        /* EC TBD ... */

        /*
	 * buf->start points to beginning of current content.
	 * buf->ptr points to the end of the current content.
	 * Test for null termination ... [TBD]
	 */
	if (logfn_send) {
	    (*logfn_send)(buf->app, buf->level, "%s", buf->start);
	    buf->write_pending = FALSE;
	} else {
	    LOG_INTERNAL_ERR(logfn_send is NULL, CONTINUE);
	}
    }

    log_util_init_buf(buf);

} /* log_util_flush_internal() */


//#define FULL_VALIDATION TRUE
static boolean
    validate_log_buf (syslog_msg_desc_t *buf)
{
    syslog_msg_desc_t *dp = buf;
#ifdef FULL_VALIDATION
    char *bp = &buf->buf[0];
    char *mp;
    uint i;
#endif

    if (dp->idid != SYSLOG_BUF_ID) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (id), buf, CONTINUE);
	return FALSE;
    }

#ifdef FULL_VALIDATION
    if (dp->start != &dp->buf[0]) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (start), buf, CONTINUE);
	return FALSE;
    }
    if (dp->end != &dp->buf[SYSLOG_BUF_CONTENT_SIZE]) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (end), buf, CONTINUE);
	return FALSE;
    }
    if (dp->len != SYSLOG_BUF_CONTENT_SIZE) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (len), buf, CONTINUE);
	return FALSE;
    }
    if (dp->remaining < 0) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (remaining), buf, CONTINUE);
	return FALSE;
    }
    if (dp->ptr >= dp->end) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (ptr), buf, CONTINUE);
	return FALSE;
    }

    for (i = 0; i < SYSLOG_BUF_CONTENT_SIZE; i++) {
        if (*bp != '\0') {
	    bp++;
	    continue;
	}
	if (bp < dp->ptr) {
	    log_internal_err("\n%s: Bad ptr, bp = %p, dp->ptr = %p",
			     __FUNCTION__, bp, dp->ptr);
	    LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE, buf, CONTINUE);
	    return FALSE;
	}
    }

    if (i > SYSLOG_BUF_CONTENT_SIZE) {
        log_internal_err("\n%s: Bad size? bp = %p, dp->ptr = %p",
			 __FUNCTION__, bp, dp->ptr);
	fflush(stdout);
	LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (overrun?), buf, CONTINUE);
	return FALSE;
    }

    mp = &dp->buf[SYSLOG_BUF_NULL_BYTE_INDEX];
    if ((uint)*mp != '\0') {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (overrun '\0'?), buf, CONTINUE);
	return FALSE;
    }

    mp = &dp->buf[SYSLOG_BUF_MARKER_BYTE_INDEX];
    if (*mp != SYSLOG_BUF_MARKER) {
        LOG_INTERNAL_BUF_ERR(Bad buf VALIDATE (overrun 'X'?), buf, CONTINUE);
	return FALSE;
    }
#endif
    //LOG_INTERNAL_BUF_ERR(ALL GOOD, buf, CONTINUE);

    return TRUE;
}

/********************************************************************
* FUNCTION upd_log_buf
*
*   Buffer log output (possibly flush buffered output first)
*
* INPUTS:
*   desc == Pointer to syslog msg buffer descriptor
*   app  == YumaPro app (like netconfd-pro or yangcli-pro). This will
*           be (or has been) translated by vendor or syslog into an
*           appropriate equivalent.
*   level == internal send priority
*   sub_level == internal content priority
*   compression == TRUE: compress leading spaces (may be adjustable in future)
*   fstr == format string in printf format
*   ... == any additional arguments for printf
*
*********************************************************************/
static boolean spaces = FALSE;

static void 
    upd_log_buf (syslog_msg_desc_t *buf, log_debug_app_t app,
		 log_debug_t level, log_debug_t sub_level,
		 boolean compression, const char *fstr, va_list args)
{
    char ch;
    int str_size;
    const char *local_fstr = fstr; /* Copy */
    const char *space_fstr;        /* Copy */
    va_list args_copy;

    if (!buf->write_pending) {
        log_util_init_buf(buf);
	buf->level = level;
	buf->sub_level = sub_level;
	buf->app = app;
    }

    if (buf->remaining < 0) {
      LOG_INTERNAL_BUF_ERR(Bad buf (remaining), buf, REINIT);
	  /* Force reinit and attempt to continue */
	  log_util_reinit_buf(buf);
    }

    /* Always strip leading carriage returns, if any */
    while (*local_fstr == '\n') {
        local_fstr++;
    }
    /* Make sure there's something left to output */
    if (*local_fstr == 0) {
        return;
    }

    /*
     * Strip single leading spaces, if any:
     *
     * Start out by looking for '%c' + NULL terminator, with a space
     * as the output char. We allow the first instance of this to pass
     * through, but reject subsequent instances (controlled by the
     * static "spaces" boolean).
     *
     * Note: These are generated in the code by numerous cases of
     * log_xxx_append("%c", ch), generally in tight loops running
     * over a previously buffered string.
     */
    if (compression) {

        space_fstr = local_fstr;

        if ((*space_fstr++ == '%') &&
	    (*space_fstr++ == 'c') &&
	    (*space_fstr++ == '\0')) {     /* "%c\0" */

	    va_copy(args_copy, args);
	    ch = va_arg(args_copy, int);
	    va_end(args_copy);

	    if ((ch == ' ') && spaces) {   /* Filter if more than one */
	        return;
	    } else {
	        if (ch == ' ') {
		    spaces = TRUE;         /* Remember first space */
		} else {
		    spaces = FALSE;        /* Clear on first non-space */
		}
	    }

	} else {
	    spaces = FALSE;
	}

	/* Compress real leading spaces (something at entry like "\n   ") */
	/* Pass the first space: */
	if (*local_fstr == ' ') {
	    local_fstr++;
	}
	/* Now, strip remaining spaces, if any */
	while (*local_fstr == ' ') {
	    local_fstr++;
	}
	/* Make sure there's something left to output */
	if (*local_fstr == 0) {
	    return;
	}
    }

    /* Make sure there's something left to output */
    if (*local_fstr == 0) {
        return;
    }

    if (buf->write_pending) {
        validate_log_buf(buf);
    }

    /* Copy remaining string content into local buffer */
    str_size = vsnprintf(buf->ptr, buf->remaining, local_fstr, args);

    if (str_size < 0) {
        LOG_INTERNAL_ERR(vsnprintf() error, REINIT);
	log_util_reinit_buf(buf);
	return;
    }

    buf->initialized = FALSE;
    buf->write_pending = TRUE;

    //validate_log_buf(buf);

    /* Update descriptor */
    if (str_size < buf->remaining) {
        /* Space remaining ... update and continue ... */
        buf->ptr += str_size;
	buf->remaining -= str_size;

#ifdef FULL_VALIDATION
	if ((*buf->ptr != 0) || (*buf->end != 0) ||
	    (buf->buf[SYSLOG_BUF_NULL_BYTE_INDEX] != 0) ||
	    (buf->buf[SYSLOG_BUF_MARKER_BYTE_INDEX] != SYSLOG_BUF_MARKER)) {
	    LOG_INTERNAL_BUF_ERR(??? BAD SYSLOG BUF ???, buf, CONTINUE);
	}
#endif

    } else {
        /* Buffer full ... update, check, and flush ... */
        buf->ptr += buf->remaining;
	buf->remaining = 0;
	if (buf->ptr != buf->end) {
	    LOG_INTERNAL_BUF_ERR(Overflow? Bad buf, buf, REINIT);
	    log_util_reinit_buf(buf);
	} else {
	    log_util_flush_internal(buf);
	}
	/*
	 * Note that at this point we have dropped whatever remains of
	 * input fstr/args (if anything). This is not a huge problem since
	 * we intend to use a buffer that is large enough that most users
	 * won't see it, or even care if they do.
	 *
	 * In theory, the fix is "easy" ... call ourselves recursively
	 * passing an updated fstr/arg combo. Probably easier to find a
	 * way to prevent it from happening in the first place.
	 *
	 *  Exact fix is TBD.
	 */
    }

    /* Error checking ... */
    if (buf->remaining < 0) { /* SHOULD NEVER HAPPEN! */
        LOG_INTERNAL_BUF_ERR(Bad buf (end), buf, REINIT);
	/* Force reinit and attempt to continue */
	log_util_reinit_buf(buf);
    }
}


/********************************************************************
* FUNCTION log_util_flush
*
*   External interface to force contents of current syslog buffer out to the
*   syslog daemon.
*
*********************************************************************/
void 
    log_util_flush (syslog_msg_desc_t *desc)
{
    log_util_flush_internal(desc);
}

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
static boolean first_time=TRUE;

void 
    log_util_logbuf_common (syslog_msg_desc_t *desc, log_debug_app_t app,
			    log_debug_t level, log_debug_t sub_level,
			    const char *fstr, va_list args)
{
    if (desc->write_pending) {
        log_util_flush_internal(desc);
    }

    if (first_time) { /***** MOVE ME *****/
	if (logfn_connect) {
	    (*logfn_connect)();
	    first_time = FALSE;
	} else {
	    LOG_INTERNAL_ERR(logfn_connect is NULL, CONTINUE);
	}
    }

    upd_log_buf(desc, app, level, sub_level, FALSE, fstr, args);

} /* log_util_logbuf_common */


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
void
    log_util_logbuf_append (syslog_msg_desc_t *desc, log_debug_app_t app,
			    log_debug_t level, log_debug_t sub_level,
			    const char *fstr, va_list args)
{
    /* We have everything we need ... just buffer (and maybe output) it */
    upd_log_buf(desc, app, level, sub_level, TRUE, fstr, args);

}  /* log_util_append */

/* END file log_util.c */

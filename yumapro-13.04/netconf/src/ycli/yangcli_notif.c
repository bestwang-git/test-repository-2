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
/*  FILE: yangcli_notif.c

   NETCONF YANG-based CLI Tool

   notification handler

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
07-dec-12    abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
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
#include <assert.h>

#include "libtecla.h"

#include "procdefs.h"
#include "log.h"
#include "mgr.h"
#include "mgr_not.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_notif.h"
#include "yangcli_util.h"


/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* event callback function control block */
typedef struct event_cb_t_ {
    dlq_hdr_t qhdr;
    xmlChar *modname;
    xmlChar *event;
    yangcli_notif_cbfn_t cbfn;
} event_cb_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/
/* event callback handler Queue */

static boolean yangcli_notif_init_done = FALSE;
static dlq_hdr_t event_cbQ;  // Q of event_cb_t


/********************************************************************
 * FUNCTION free_event_cb
 * 
 * Clean and free an event control block
 *
 * INPUTS:
 *   cb == control block to free
 *********************************************************************/
static void
    free_event_cb (event_cb_t *cb)
{
    if (cb == NULL) {
        return;
    }

    m__free(cb->modname);
    m__free(cb->event);
    m__free(cb);

} /* free_event_cb */


/********************************************************************
 * FUNCTION new_event_cb
 * 
 * Malloc and fill in a new event control block
 *
 * INPUTS:
 *   modname == module name
 *   event == eventname
 *   cbfn == callback function to use
 *
 * RETURNS:
 *   filled in struct or NULL if ERR_INTERNAL_MEM
 *********************************************************************/
static event_cb_t *
    new_event_cb (const xmlChar *modname,
                 const xmlChar *event,
                 yangcli_notif_cbfn_t cbfn)
{
    event_cb_t *cb = m__getObj(event_cb_t);
    if (cb == NULL) {
        return NULL;
    }
    memset(cb, 0x0, sizeof(event_cb_t));

    cb->modname = xml_strdup(modname);
    if (cb->modname == NULL) {
        free_event_cb(cb);
        return NULL;
    }

    cb->event = xml_strdup(event);
    if (cb->event == NULL) {
        free_event_cb(cb);
        return NULL;
    }

    cb->cbfn = cbfn;

    return cb;

} /* new_event_cb */


/********************************************************************
 * FUNCTION find_event_cb
 * 
 * Find a specified event control block
 *
 * INPUTS:
 *   modname == module name
 *   event == eventname
 *   curcb == current starting point; NULL to use start
 * RETURNS:
 *   found record or NULL if not found
 *********************************************************************/
static event_cb_t *
    find_event_cb (const xmlChar *modname,
                   const xmlChar *event,
                   event_cb_t *curcb)
{
    event_cb_t *cb = (curcb) ? 
        (event_cb_t *)dlq_nextEntry(curcb) :
        (event_cb_t *)dlq_firstEntry(&event_cbQ);
    for (; cb; cb = (event_cb_t *)dlq_nextEntry(cb)) {
        int ret = xml_strcmp(modname, cb->modname);
        if (ret < 0) {
            return NULL;
        } else if (ret == 0) {
            int ret2 = xml_strcmp(event, cb->event);
            if (ret2 < 0) {
                return NULL;
            } else if (ret2 == 0) {
                return cb;
            }
        }
    }
    return NULL;

} /* find_event_cb */


/********************************************************************
 * FUNCTION insert_event_cb
 * 
 * Insert a new event control block
 *
 * INPUTS:
 *   newcb == control block to insert
 *********************************************************************/
static void
    insert_event_cb (event_cb_t *newcb)
{
    /* insert sorted by module-name, event-name */
    event_cb_t *cb = (event_cb_t *)dlq_firstEntry(&event_cbQ);
    for (; cb; cb = (event_cb_t *)dlq_nextEntry(cb)) {
        int ret = xml_strcmp(newcb->modname, cb->modname);
        if (ret < 0) {
            dlq_insertAhead(newcb, cb);
            return;
        } else if (ret == 0) {
            int ret2 = xml_strcmp(newcb->event, cb->event);
            if (ret2 <= 0) {
                dlq_insertAhead(newcb, cb);
                return;
            }
        }
    }

    /* new last entry */
    dlq_enque(newcb, &event_cbQ);

} /* insert_event_cb */


/********************************************************************
 * FUNCTION dispatch_notif_event
 * 
 * Look for a callback handler for this event
 *
 * INPUTS:
 *   newcb == control block to insert
 * RETURNS:
 *   number of handlers found for this event
 *********************************************************************/
static uint32
    dispatch_notif_event (session_cb_t *session_cb,
                          mgr_not_msg_t *msg)
{
    uint32 retcnt = 0;

    if (msg->eventType) {
        const xmlChar *modname = val_get_mod_name(msg->eventType);
        const xmlChar *event = msg->eventType->name;
        if (modname && event) {
            uint32 hid = 1;
            const xmlChar *etime = NULL;
            event_cb_t *cb = find_event_cb(modname, event, NULL);
            if (cb) {
                if (msg->eventTime) {
                    etime = VAL_STR(msg->eventTime);
                } else {
                    etime = NCX_EL_NONE;
                }
            }
            while (cb) {
                if (LOGDEBUG) {
                    log_debug("\nDispatching <%s> notification (handler: %u)",
                              msg->eventType->name, hid++);
                }
                retcnt++;
                (*cb->cbfn)(session_cb, modname, event, etime, msg->eventType);
                cb = find_event_cb(modname, event, cb);
            }
        }
    }

    return retcnt;

} /* dispatch_notif_event */


/********************************************************************
 * FUNCTION yangcli_notification_handler
 * 
 * matches callback template mgr_not_cbfn_t
 *
 * INPUTS:
 *   scb == session receiving RPC reply
 *   msg == notification msg that was parsed
 *   consumed == address of return consumed message flag
 *
 *  OUTPUTS:
 *     *consumed == TRUE if msg has been consumed so
 *                  it will not be freed by mgr_not_dispatch
 *               == FALSE if msg has been not consumed so
 *                  it will be freed by mgr_not_dispatch
 *********************************************************************/
void
    yangcli_notification_handler (ses_cb_t *scb,
                                  mgr_not_msg_t *msg,
                                  boolean *consumed)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    assert(consumed && "consumed is NULL!");

    *consumed = FALSE;

    mgr_scb_t *mgrcb = scb->mgrcb;
    server_cb_t *server_cb = get_cur_server_cb();
    session_cb_t *save_session_cb = server_cb->cur_session_cb;
    session_cb_t *session_cb = NULL;

    if (mgrcb) {
        session_cb = mgrcb->session_cb;
    }

    if (session_cb == NULL) {
        log_error("\nError: session no longer valid; dropping notification");
        return;
    }

    const xmlChar *sesname = get_session_name(session_cb);
    if (session_cb != save_session_cb) {
        set_cur_session_cb(server_cb, session_cb);
    }

    /* check the contents of the notification */
    if (msg && msg->notification) {

        /* dispatch this event to any handler registered for
         * this event;  !!! not session-specific !!!     */
        uint32 retcnt = dispatch_notif_event(session_cb, msg);
         
        /* display only unhandled notifications, and only if
         * the session is configured to show them         */
        log_debug_t loglevel = log_get_debug_level();
        log_debug_t base_loglevel = loglevel + 1;
        if (loglevel == LOG_DEBUG_INFO) {
            base_loglevel++;
        }
        if (retcnt == 0 && session_cb->echo_notifs &&
            base_loglevel >= session_cb->echo_notif_loglevel) {

            gl_normal_io(server_cb->cli_gl);
            if (loglevel >= session_cb->echo_notif_loglevel) {
                log_write("\n\nIncoming notification for session %u [%s]:",
                          scb->sid, sesname);
                val_dump_value_max(msg->notification, 
                                   0, session_cb->defindent,
                                   DUMP_VAL_LOG,
                                   session_cb->display_mode,
                                   FALSE, FALSE);
                log_write_append("\n\n");
            } else if (msg->eventType) {
                log_write("\n\nIncoming <%s> notification for session "
                          "%u [%s]\n\n", msg->eventType->name,
                          scb->sid, sesname);
            }
        }

        /* store msg in the session notification log */
        dlq_enque(msg, &session_cb->notificationQ);
        *consumed = TRUE;
    }
    
    if (session_cb != save_session_cb) {
        set_cur_session_cb(server_cb, save_session_cb);
    }

}  /* yangcli_notification_handler */


/********************************************************************
 * FUNCTION register_notif_event_handler
 * 
 * Register an event callback function
 *
 * INPUTS:
 *   modname == module defining the notification
 *   event == notification event name
 *   cbfn == callback function to register
 *
 * RETURNS:
 *   status: error 
 *********************************************************************/
status_t
    register_notif_event_handler (const xmlChar *modname,
                                  const xmlChar *event,
                                  yangcli_notif_cbfn_t cbfn)
{
    event_cb_t *cb = new_event_cb(modname, event, cbfn);
    if (cb == NULL) {
        return ERR_INTERNAL_MEM;
    }
    insert_event_cb(cb);
    return NO_ERR;

}  /* register_notif_event_handler */


/********************************************************************
 * FUNCTION unregister_notif_event_handler
 * 
 * Unregister an event callback function
 * This is optional -- data structures will get cleaned up
 * when the program terminates
 * Assumes function is registered once!!!
 * Caller must make sure to register each callback only once
 * or call this function multiple times
 *
 * INPUTS:
 *   modname == module defining the notification
 *   event == notification event name
 *   cbfn == function to unregister
 *********************************************************************/
void
    unregister_notif_event_handler (const xmlChar *modname,
                                    const xmlChar *event,
                                    yangcli_notif_cbfn_t cbfn)
{
    event_cb_t *cb = find_event_cb(modname, event, NULL);
    while (cb) {
        if (cb->cbfn == cbfn) {
            dlq_remove(cb);
            free_event_cb(cb);
            return;
        }
        cb = find_event_cb(modname, event, cb);
    }

}  /* unregister_notif_event_handler */


/********************************************************************
 * FUNCTION yangcli_notif_init
 * 
 * Init this module
 *
 *********************************************************************/
void
    yangcli_notif_init (void)
{
    if (!yangcli_notif_init_done) {
        dlq_createSQue(&event_cbQ);
        yangcli_notif_init_done = TRUE;
    } else {
        log_error("\nError: yangcli_notif module already initialized\n");
    }

}  /* yangcli_notif_init */


/********************************************************************
 * FUNCTION yangcli_notif_cleanup
 * 
 * Cleanup this module
 *
 *********************************************************************/
void
    yangcli_notif_cleanup (void)
{
    if (yangcli_notif_init_done) {
        while (!dlq_empty(&event_cbQ)) {
            event_cb_t *cb = dlq_deque(&event_cbQ);
            free_event_cb(cb);
        }
        yangcli_notif_init_done = FALSE;
    }

}  /* yangcli_notif_cleanup */


/* END yangcli_notif.c */

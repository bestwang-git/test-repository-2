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
/*  FILE: mgr_not.c

   NETCONF Protocol Operations: RPC Manager Side Support

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
17may05      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <time.h>

#include "procdefs.h"
#include "dlq.h"
#include "log.h"
#include "mgr.h"
#include "mgr_not.h"
#include "mgr_ses.h"
#include "mgr_val_parse.h"
#include "mgr_xml.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxtypes.h"
#include "obj.h"
#include "rpc.h"
#include "rpc_err.h"
#include "ses.h"
#include "status.h"
#include "top.h"
#include "typ.h"
#include "val.h"
#include "xmlns.h"
#include "xml_msg.h"
#include "xml_util.h"
#include "xml_wr.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define MGR_NOT_DEBUG 1
#endif

/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/
    

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/
static boolean               mgr_not_init_done = FALSE;

static obj_template_t       *notification_obj;

mgr_not_cbfn_t               callbackfn;


/********************************************************************
* FUNCTION new_msg
*
* Malloc and initialize a new mgr_not_msg_t struct
*
* INPUTS:
*   none
* RETURNS:
*   pointer to struct or NULL or memory error
*********************************************************************/
static mgr_not_msg_t *
    new_msg (void)
{
    mgr_not_msg_t *msg;

    msg = m__getObj(mgr_not_msg_t);
    if (!msg) {
        return NULL;
    }
    memset(msg, 0x0, sizeof(mgr_not_msg_t));

    msg->notification = val_new_value();
    if (!msg->notification) {
        m__free(msg);
        return NULL;
    }
    /* xml_msg_init_hdr(&msg->mhdr); */
    return msg;

} /* new_msg */


/************** E X T E R N A L   F U N C T I O N S  ***************/


/********************************************************************
* FUNCTION mgr_not_init
*
* Initialize the mgr_not module
* call once to init module
* Adds the mgr_not_dispatch function as the handler
* for the NETCONF <rpc> top-level element.
*
* INPUTS:
*   none
* RETURNS:
*   NO_ERR if all okay, the minimum spare requests will be malloced
*********************************************************************/
status_t 
    mgr_not_init (void)
{
    if (mgr_not_init_done) {
        return ERR_INTERNAL_INIT_SEQ;
    }

    /* get the notification template */
    notification_obj = NULL;
    callbackfn = NULL;

    ncx_module_t *mod = ncx_find_module(NCN_MODULE, NULL);
    if (mod == NULL) {
        return ERR_NCX_MOD_NOT_FOUND;
    }
    notification_obj = ncx_find_object(mod, NCX_EL_NOTIFICATION);
    if (notification_obj == NULL) {
        return ERR_NCX_DEF_NOT_FOUND;
    }

    status_t res = 
        top_register_node(NCN_MODULE, NCX_EL_NOTIFICATION, 
                          mgr_not_dispatch);
    if (res != NO_ERR) {
        return res;
    }

    mgr_not_init_done = TRUE;
    return NO_ERR;

} /* mgr_not_init */


/********************************************************************
* FUNCTION mgr_not_cleanup
*
* Cleanup the mgr_not module.
* call once to cleanup module
* Unregister the top-level NETCONF <rpc> element
*
*********************************************************************/
void 
    mgr_not_cleanup (void)
{
    if (mgr_not_init_done) {
        top_unregister_node(NCN_MODULE, NCX_EL_NOTIFICATION);
        notification_obj = NULL;
        callbackfn = NULL;
        mgr_not_init_done = FALSE;
    }

} /* mgr_not_cleanup */


/********************************************************************
* FUNCTION mgr_not_free_msg
*
* Free a mgr_not_msg_t struct
*
* INPUTS:
*   msg == struct to free
*
* RETURNS:
*   none
*********************************************************************/
void
    mgr_not_free_msg (mgr_not_msg_t *msg)
{
#ifdef DEBUG
    if (!msg) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    if (msg->notification) {
        val_free_value(msg->notification);
    }

    m__free(msg);

} /* mgr_not_free_msg */


/********************************************************************
* FUNCTION mgr_not_clean_msgQ
*
* Clean the msg Q of mgr_not_msg_t entries
*
* INPUTS:
*   msgQ == Q of entries to free; the Q itself is not freed
*
* RETURNS:
*   none
*********************************************************************/
void
    mgr_not_clean_msgQ (dlq_hdr_t *msgQ)
{
    mgr_not_msg_t *msg;

#ifdef DEBUG
    if (!msgQ) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    msg = (mgr_not_msg_t *)dlq_deque(msgQ);
    while (msg) {
        mgr_not_free_msg(msg);
        msg = (mgr_not_msg_t *)dlq_deque(msgQ);
    }

} /* mgr_not_clean_msgQ */


/********************************************************************
* FUNCTION mgr_not_dispatch
*
* Dispatch an incoming <rpc-reply> response
* handle the <notification> element
* called by mgr_top.c: 
* This function is registered with top_register_node
* for the module 'notification', top-node 'notification'
*
* INPUTS:
*   scb == session control block
*   top == top element descriptor
*********************************************************************/
void 
    mgr_not_dispatch (ses_cb_t *scb,
                      xml_node_t *top)
{
#ifdef DEBUG
    if (!scb || !top) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    /* check if the notification template is already cached */
    if (notification_obj == NULL) {
        log_error("\nNo notification module found\n");
        mgr_xml_skip_subtree(scb->reader, top);
        return;
    }

    /* the current node is 'notification' in the notifications namespace
     * First get a new notification msg struct
     */
    mgr_not_msg_t *msg = new_msg();
    if (!msg) {
        log_error("\nError: mgr_not: skipping incoming message");
        mgr_xml_skip_subtree(scb->reader, top);
        return;
    }
    
    /* parse the notification as a val_value_t tree,
     * stored in msg->notification
     */
    msg->res = mgr_val_parse_notification(scb, notification_obj,
                                          top, msg->notification);
    if (msg->res != NO_ERR && LOGINFO) {        
        log_info("\nmgr_not: got invalid notification on session %d (%s)",
                 scb->sid, get_error_string(msg->res));
    } 

    /* check that there is nothing after the <rpc-reply> element */
    if (msg->res==NO_ERR && !xml_docdone(scb->reader) && LOGINFO) {
        log_info("\nmgr_not: got extra nodes in notification on session %d",
                 scb->sid);
    }

    boolean consumed = FALSE;

    if (msg->res == NO_ERR && msg->notification) {
        val_value_t *child = val_get_first_child(msg->notification);
        if (child) {
            if (!xml_strcmp(child->name, 
                            (const xmlChar *)"eventTime")) {
                msg->eventTime = child;
            } else {
                log_error("\nError: expected 'eventTime' in "
                          "notification, got '%s'", child->name);
            }

            child = val_get_next_child(child);
            if (child) {
                /* eventType is expected to be next!! */
                msg->eventType = child;
            }
        } else {
            log_error("\nError: expected 'eventTime' in "
                      "notification, got nothing");
        }
        
        /* invoke the notification handler */
        if (callbackfn) {
            (*callbackfn)(scb, msg, &consumed);
        }
    }

    if (!consumed) {
        mgr_not_free_msg(msg);
    }

} /* mgr_not_dispatch */


/********************************************************************
* FUNCTION mgr_not_set_callback_fn
*
* Set the application callback function to handle
* notifications when they arrive
*
* INPUTS:
*   cbfn == callback function to use
*        == NULL to clear the callback function
*********************************************************************/
void 
    mgr_not_set_callback_fn (mgr_not_cbfn_t cbfn)
{
    callbackfn = cbfn;
}  /* mgr_not_set_callback_fn */


/* END file mgr_not.c */

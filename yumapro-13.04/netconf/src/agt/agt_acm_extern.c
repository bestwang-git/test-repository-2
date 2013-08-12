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
/*  FILE: agt_acm_extern.c

  Access Control Model Hooks for external data model


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
18jun12      abb      begun; split from agt_acm.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <assert.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_acm.h"
#include "agt_acm_extern.h"
#include "ncxconst.h"
#include "obj.h"
#include "op.h"
#include "status.h"
#include "val.h"



/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/********************************************************************
*                                                                    *
*                             T Y P E S                              *
*                                                                    *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

static agt_acm_extern_rpc_fn_t extern_rpcfn;
static agt_acm_extern_notif_fn_t extern_notfn;
static agt_acm_extern_write_fn_t extern_writefn;
static agt_acm_extern_read_fn_t extern_readfn;


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION agt_acm_extern_init2
* 
* Phase 2:
*   Initialize the external data model configuration data structures
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
status_t 
    agt_acm_extern_init2 (void)
{

    return NO_ERR;

}  /* agt_acm_extern_init2 */


/********************************************************************
* FUNCTION agt_acm_extern_init0
* 
* Pre-Phase 1:
*   Init the callback pointers
*   Need to do this first before the external system library
*   init1 function is called
*********************************************************************/
void agt_acm_extern_init0 (void)
{

    extern_rpcfn = NULL;
    extern_notfn = NULL;
    extern_writefn = NULL;
    extern_readfn = NULL;

}  /* agt_acm_extern_init0 */


/********************************************************************
* FUNCTION agt_acm_extern_init1
* 
* Phase 1:
*   Nothing to do because the external library is
*   expected to register its callbacks by now
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
status_t 
    agt_acm_extern_init1 (void)
{

    /* this is really called at the start of agt_init2 */
    return NO_ERR;

}  /* agt_acm_extern_init1 */


/********************************************************************
* FUNCTION agt_acm_extern_cleanup
*
* Cleanup the external access control module
* 
*********************************************************************/
void
    agt_acm_extern_cleanup (void)
{
    extern_rpcfn = NULL;
    extern_notfn = NULL;
    extern_writefn = NULL;
    extern_readfn = NULL;

}   /* agt_acm_extern_cleanup */


/********************************************************************
* FUNCTION agt_acm_extern_init_msg_cache
*
* Malloc and initialize an agt_acm_cache_t struct
* and attach it to the incoming message
*
* INPUTS:
*   scb == session control block to use
*   msg == message to use
*
* OUTPUTS:
*   scb->acm_cache pointer may be set, if it was NULL
*   msg->acm_cache pointer set
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_acm_extern_init_msg_cache (ses_cb_t *scb,
                                   xml_msg_hdr_t *msg)
{
    (void)scb;
    msg->acm_cbfn = agt_acm_extern_val_read_allowed;

    // external ACM handles caching

    return NO_ERR;

} /* agt_acm_extern_init_msg_cache */



/********************************************************************
* FUNCTION agt_acm_extern_rpc_allowed
*
* Check if the specified user is allowed to invoke an RPC
* 
* INPUTS:
*   msg == XML header in incoming message in progress
*   user == user name string
*   rpcobj == obj_template_t for the RPC method to check
*
* RETURNS:
*   TRUE if user allowed invoke this RPC; FALSE otherwise
*********************************************************************/
boolean 
    agt_acm_extern_rpc_allowed (xml_msg_hdr_t *msg,
                                const xmlChar *user,
                                const obj_template_t *rpcobj)
{
    boolean ret = TRUE;
    if (extern_rpcfn) {
        ret = (*extern_rpcfn)(msg, user, rpcobj);
    }
    return ret;

}   /* agt_acm_rpc_extern_allowed */


/********************************************************************
* FUNCTION agt_acm_extern_notif_allowed
*
* Check if the specified user is allowed to receive
* a notification event
* 
* INPUTS:
*   user == user name string
*   notifobj == obj_template_t for the notification event to check
*
* RETURNS:
*   TRUE if user allowed receive this notification event;
*   FALSE otherwise
*********************************************************************/
boolean 
    agt_acm_extern_notif_allowed (const xmlChar *user,
                                  const obj_template_t *notifobj)
{
    boolean ret = TRUE;
    if (extern_notfn) {
        ret = (*extern_notfn)(user, notifobj);
    }
    return ret;

}   /* agt_acm_extern_notif_allowed */


/********************************************************************
* FUNCTION agt_acm_extern_val_write_allowed
*
* Check if the specified user is allowed to access a value node
* The val->obj template will be checked against the val->editop
* requested access and the user's configured max-access
* 
* INPUTS:
*   msg == XML header from incoming message in progress
*   newval  == val_value_t in progress to check
*                (may be NULL, if curval set)
*   curval  == val_value_t in progress to check
*                (may be NULL, if newval set)
*   val  == val_value_t in progress to check
*   editop == requested CRUD operation
*
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
boolean 
    agt_acm_extern_val_write_allowed (xml_msg_hdr_t *msg,
                                      const xmlChar *user,
                                      const val_value_t *newval,
                                      const val_value_t *curval,
                                      op_editop_t editop)
{
    boolean ret = TRUE;
    if (extern_writefn) {
        ret = (*extern_writefn)(msg, user, newval, curval, editop);
    }
    return ret;

}   /* agt_acm_extern_val_write_allowed */


/********************************************************************
* FUNCTION agt_acm_extern_val_read_allowed
*
* Check if the specified user is allowed to read a value node
* 
* INPUTS:
*   msg == XML header from incoming message in progress
*   user == user name string
*   val  == val_value_t in progress to check
*
* RETURNS:
*   TRUE if user allowed read access to the value node
*********************************************************************/
boolean 
    agt_acm_extern_val_read_allowed (xml_msg_hdr_t *msg,
                                     const xmlChar *user,
                                     const val_value_t *val)
{
    boolean ret = TRUE;
    if (extern_readfn) {
        ret = (*extern_readfn)(msg, user, val);
    }
    return ret;

}   /* agt_acm_extern_val_read_allowed */



/********************************************************************
* FUNCTION agt_acm_extern_register_callbacks
*
* Register the external callbacks for ACM implementation
* A NULL callback means that type of access will always
* be granted!!!
*
* INPUTS:
*    rpcfn == check-rpc function callback
*    notfn == check-notification function callback
*    writefn == check-val-write function callback
*    readfn == check-val-write function callback
*
*********************************************************************/
void agt_acm_extern_register_callbacks 
    (agt_acm_extern_rpc_fn_t rpcfn,
     agt_acm_extern_notif_fn_t notfn,
     agt_acm_extern_write_fn_t writefn,
     agt_acm_extern_read_fn_t readfn)
{
    extern_rpcfn = rpcfn;
    extern_notfn = notfn;
    extern_writefn = writefn;
    extern_readfn = readfn;

}  /* agt_acm_extern_register_callbacks */


/* END file agt_acm_extern.c */

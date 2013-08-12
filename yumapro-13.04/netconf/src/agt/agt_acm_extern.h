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
#ifndef _H_agt_acm_extern
#define _H_agt_acm_extern

/*  FILE: agt_acm_extern.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    NETCONF Server Access Control handler for external data model

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
18-jun-12    abb      Begun; split from agt_acm.h
*/

#include <xmlstring.h>


#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_op
#include "op.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xml_msg
#include "xml_msg.h"
#endif





#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/


/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/


/********************************************************************
* FUNCTION agt_acm_extern_rpc_fn
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
typedef boolean 
    (*agt_acm_extern_rpc_fn_t) (xml_msg_hdr_t *msg,
                                const xmlChar *user,
                                const obj_template_t *rpcobj);


/********************************************************************
* FUNCTION agt_acm_extern_notif_fn
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
typedef boolean 
    (*agt_acm_extern_notif_fn_t) (const xmlChar *user,
                                  const obj_template_t *notifobj);


/********************************************************************
* FUNCTION agt_acm_extern_write_fn
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
typedef boolean 
    (*agt_acm_extern_write_fn_t) (xml_msg_hdr_t *msg,
                                  const xmlChar *user,
                                  const val_value_t *newval,
                                  const val_value_t *curval,
                                  op_editop_t editop);


/********************************************************************
* FUNCTION agt_acm_extern_read_fn
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
typedef boolean 
    (*agt_acm_extern_read_fn_t) (xml_msg_hdr_t *msg,
                                 const xmlChar *user,
                                 const val_value_t *val);

    
/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/


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
extern status_t 
    agt_acm_extern_init2 (void);


/********************************************************************
* FUNCTION agt_acm_extern_init0
* 
* Pre-Phase 1:
*   Init the callback pointers
*   Need to do this first before the external system library
*   init1 function is called
*********************************************************************/
extern void agt_acm_extern_init0 (void);


/********************************************************************
* FUNCTION agt_acm_extern_init1
* 
* Phase 1:
*   Load the external data module
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
extern status_t 
    agt_acm_extern_init1 (void);


/********************************************************************
* FUNCTION agt_acm_extern_cleanup
*
* Cleanup the external access control module
* 
*********************************************************************/
extern void
    agt_acm_extern_cleanup (void);


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
extern status_t
    agt_acm_extern_init_msg_cache (ses_cb_t *scb,
                                   xml_msg_hdr_t *msg);


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
extern void agt_acm_extern_register_callbacks 
    (agt_acm_extern_rpc_fn_t rpcfn,
     agt_acm_extern_notif_fn_t notfn,
     agt_acm_extern_write_fn_t writefn,
     agt_acm_extern_read_fn_t readfn);


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
extern boolean 
    agt_acm_extern_rpc_allowed (xml_msg_hdr_t *msg,
                                const xmlChar *user,
                                const obj_template_t *rpcobj);


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
extern boolean 
    agt_acm_extern_notif_allowed (const xmlChar *user,
                                  const obj_template_t *notifobj);


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
extern boolean 
    agt_acm_extern_val_write_allowed (xml_msg_hdr_t *msg,
                                      const xmlChar *user,
                                      const val_value_t *newval,
                                      const val_value_t *curval,
                                      op_editop_t editop);


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
extern boolean 
    agt_acm_extern_val_read_allowed (xml_msg_hdr_t *msg,
                                     const xmlChar *user,
                                     const val_value_t *val);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_agt_acm_extern */

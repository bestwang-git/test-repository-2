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
#ifndef _H_agt_acm_yuma
#define _H_agt_acm_yuma

/*  FILE: agt_acm_yuma.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    NETCONF Server Access Control handler

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

#ifndef _H_agt
#include "agt.h"
#endif

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xml_msg
#include "xmlmsg.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xpath
#include "xpath.h"
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
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/


/********************************************************************
* FUNCTION agt_acm_yuma_init1
* 
* Phase 1:
*   Load the yuma-nacm.yang module
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
extern status_t 
    agt_acm_yuma_init1 (void);


/********************************************************************
* FUNCTION agt_acm_yuma_init2
* 
* Phase 2:
*   Initialize the yuma-nacm.yang configuration data structures
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
extern status_t 
    agt_acm_yuma_init2 (void);


/********************************************************************
* FUNCTION agt_acm_yuma_cleanup
* 
* Cleanup the yuma-nacm.yang access control module
* 
* INPUTS:
*   none
* RETURNS:
*   none
*********************************************************************/
extern void
    agt_acm_yuma_cleanup (void);


/********************************************************************
* FUNCTION agt_acm_yuma_rpc_allowed
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
    agt_acm_yuma_rpc_allowed (xml_msg_hdr_t *msg,
                              const xmlChar *user,
                              const obj_template_t *rpcobj);


/********************************************************************
* FUNCTION agt_acm_yuma_notif_allowed
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
    agt_acm_yuma_notif_allowed (const xmlChar *user,
                                const obj_template_t *notifobj);


/********************************************************************
* FUNCTION agt_acm_yuma_val_write_allowed
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
    agt_acm_yuma_val_write_allowed (xml_msg_hdr_t *msg,
                                      const xmlChar *user,
                                      const val_value_t *newval,
                                      const val_value_t *curval,
                                      op_editop_t editop);


/********************************************************************
* FUNCTION agt_acm_yuma_val_read_allowed
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
    agt_acm_yuma_val_read_allowed (xml_msg_hdr_t *msg,
                                     const xmlChar *user,
                                     const val_value_t *val);


/********************************************************************
* FUNCTION agt_acm_yuma_init_msg_cache
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
    agt_acm_yuma_init_msg_cache (ses_cb_t *scb,
                                 xml_msg_hdr_t *msg);


/********************************************************************
* FUNCTION agt_acm_yuma_clear_session_cache
*
* Clear an agt_acm_cache_t struct in a session control block
*
* INPUTS:
*   scb == sesion control block to use
*
* OUTPUTS:
*   scb->acm_cache pointer is freed and set to NULL
*
*********************************************************************/
extern void agt_acm_yuma_clear_session_cache (ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_yuma_invalidate_session_cache
*
* Invalidate an agt_acm_cache_t struct in a session control block
*
* INPUTS:
*   scb == sesion control block to use
*
* OUTPUTS:
*   scb->acm_cache pointer is freed and set to NULL
*
*********************************************************************/
extern void agt_acm_yuma_invalidate_session_cache (ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_yuma_session_cache_valid
*
* Check if a session ACM cache is valid
*
* INPUTS:
*   scb == sesion control block to check
*
* RETURNS:
*   TRUE if cache calid
*   FALSE if cache invalid or NULL
*********************************************************************/
extern boolean agt_acm_yuma_session_cache_valid (const ses_cb_t *scb);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_agt_acm_yuma */

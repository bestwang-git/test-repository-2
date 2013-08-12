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
#ifndef _H_agt_acm
#define _H_agt_acm

/*  FILE: agt_acm.h
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
03-feb-06    abb      Begun
14-may-09    abb      add per-msg cache to speed up performance
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
#include "xml_msg.h"
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

/* this is defined by the vendor and not allowed to change by
 * the user since there are no translation functions between models
 * Pick RFC 6536 as the default.
 */
// To change ACM models, also change the nacm:default-deny-* extensions
// in all YANG modules using these YANG extensions
#define AGT_DEF_ACM_MODEL         AGT_ACM_MODEL_IETF_NACM
//#define AGT_DEF_ACM_MODEL         AGT_ACM_MODEL_YUMA_NACM


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
* FUNCTION agt_acm_init
* 
* Initialize the NETCONF Server access control module
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
extern status_t 
    agt_acm_init (void);


/********************************************************************
* FUNCTION agt_acm_init2
* 
* Phase 2:
*   Initialize the nacm.yang configuration data structures
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
extern status_t 
    agt_acm_init2 (void);


/********************************************************************
* FUNCTION agt_acm_cleanup
*
* Cleanup the NETCONF Server access control module
* 
*********************************************************************/
extern void 
    agt_acm_cleanup (void);


/********************************************************************
* FUNCTION agt_acm_rpc_allowed
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
    agt_acm_rpc_allowed (xml_msg_hdr_t *msg,
			 const xmlChar *user,
			 const obj_template_t *rpcobj);


/********************************************************************
* FUNCTION agt_acm_notif_allowed
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
    agt_acm_notif_allowed (const xmlChar *user,
                           const obj_template_t *notifobj);


/********************************************************************
* FUNCTION agt_acm_val_write_allowed
*
* Check if the specified user is allowed to access a value node
* The val->obj template will be checked against the val->editop
* requested access and the user's configured max-access
* 
* INPUTS:
*   msg == XML header from incoming message in progress
*   user == user name string
*   newval  == val_value_t in progress to check
*                (may be NULL, if curval set)
*   curval  == val_value_t in progress to check
*                (may be NULL, if newval set)
*   editop == requested CRUD operation
*
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
extern boolean 
    agt_acm_val_write_allowed (xml_msg_hdr_t *msg,
			       const xmlChar *user,
			       val_value_t *newval,
			       val_value_t *curval,
                               op_editop_t editop);


/********************************************************************
* FUNCTION agt_acm_val_read_allowed
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
    agt_acm_val_read_allowed (xml_msg_hdr_t *msg,
			      const xmlChar *user,
			      val_value_t *val);


/********************************************************************
* FUNCTION agt_acm_init_msg_cache
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
    agt_acm_init_msg_cache (ses_cb_t *scb,
                            xml_msg_hdr_t *msg);


/********************************************************************
* FUNCTION agt_acm_clear_msg_cache
*
* Clear an agt_acm_cache_t struct
* attached to the specified message
*
* INPUTS:
*   msg == message to use
*
* OUTPUTS:
*   msg->acm_cache pointer is freed and set to NULL
*
*********************************************************************/
extern void
    agt_acm_clear_msg_cache (xml_msg_hdr_t *msg);


/********************************************************************
* FUNCTION agt_acm_clear_session_cache
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
extern void agt_acm_clear_session_cache (ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_invalidate_session_cache
*
* Mark an agt_acm_cache_t struct in a session control block
* as invalid so it will be refreshed next use
*
* INPUTS:
*   scb == sesion control block to use
*
*********************************************************************/
extern void agt_acm_invalidate_session_cache (ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_session_cache_valid
*
* Check if the specified session NACM cache is valid
*
* INPUTS:
*   scb == session to check
*
* RETURNS:
*   TRUE if session acm_cache is valid
*   FALSE if session acm_cache is NULL or not valid
*********************************************************************/
extern boolean
    agt_acm_session_cache_valid (const ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_session_is_superuser
*
* Check if the specified session is the superuser
*
* INPUTS:
*   scb == session to check
*
* RETURNS:
*   TRUE if session is for the superuser
*   FALSE if session is not for the superuser
*********************************************************************/
extern boolean
    agt_acm_session_is_superuser (const ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_get_acmode
*
* Get the --access-control mode
*
* RETURNS:
*   acmode
*********************************************************************/
extern agt_acmode_t
    agt_acm_get_acmode (void);


/********************************************************************
* FUNCTION agt_acm_set_acmode
*
* Set the --access-control mode
*
* INPUTS:
*   newmode == new enum to use for acmode
*********************************************************************/
extern void
    agt_acm_set_acmode (agt_acmode_t newmode);


/********************************************************************
* FUNCTION agt_acm_get_log_writes
*
* Get the log_writes flag
*
* RETURNS:
*   log_writes value
*********************************************************************/
extern boolean
    agt_acm_get_log_writes (void);


/********************************************************************
* FUNCTION agt_acm_get_log_reads
*
* Get the log_reads flag
*
* RETURNS:
*   log_reads value
*********************************************************************/
extern boolean
    agt_acm_get_log_reads (void);


/********************************************************************
* FUNCTION agt_acm_is_superuser
*
* Check if the specified user name is the superuser
* Low-level access; no scb available
*
* INPUTS:
*   username == username to check
*
* RETURNS:
*   TRUE if username is the superuser
*   FALSE if username is not the superuser
*********************************************************************/
extern boolean
    agt_acm_is_superuser (const xmlChar *username);


/********************************************************************
* FUNCTION agt_acm_get_deniedRpcs
*
* Get the deniedRpcs counter
*
* RETURNS:
*   counter value
*********************************************************************/
extern uint32
    agt_acm_get_deniedRpcs (void);


/********************************************************************
* FUNCTION agt_acm_get_deniedDataWrites
*
* Get the deniedDataWrites counter
*
* RETURNS:
*   counter value
*********************************************************************/
extern uint32
    agt_acm_get_deniedDataWrites (void);


/********************************************************************
* FUNCTION agt_acm_get_deniedNotifications
*
* Get the deniedNotification counter
*
* RETURNS:
*   counter value
*********************************************************************/
extern uint32
    agt_acm_get_deniedNotifications (void);


/********************************************************************
* FUNCTION agt_acm_clean_xpath_cache
*
* Clean any cached XPath results because the data rule results
* may not be valid anymore.
*********************************************************************/
extern void
    agt_acm_clean_xpath_cache (void);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_agt_acm */

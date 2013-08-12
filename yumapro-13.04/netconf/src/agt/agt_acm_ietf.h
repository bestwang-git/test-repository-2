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
#ifndef _H_agt_acm_ietf
#define _H_agt_acm_ietf

/*  FILE: agt_acm_ietf.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    NETCONF Server Access Control handler for IETF data model

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

#define y_ietf_netconf_acm_M_ietf_netconf_acm \
    (const xmlChar *)"ietf-netconf-acm"
#define y_ietf_netconf_acm_R_ietf_netconf_acm (const xmlChar *)"2012-02-22"

#define y_ietf_netconf_acm_N_access_operations \
    (const xmlChar *)"access-operations"
#define y_ietf_netconf_acm_N_action (const xmlChar *)"action"
#define y_ietf_netconf_acm_N_comment (const xmlChar *)"comment"
#define y_ietf_netconf_acm_N_data_node (const xmlChar *)"data-node"
#define y_ietf_netconf_acm_N_denied_data_writes \
    (const xmlChar *)"denied-data-writes"
#define y_ietf_netconf_acm_N_denied_notifications \
    (const xmlChar *)"denied-notifications"
#define y_ietf_netconf_acm_N_denied_operations \
    (const xmlChar *)"denied-operations"
#define y_ietf_netconf_acm_N_enable_external_groups \
    (const xmlChar *)"enable-external-groups"
#define y_ietf_netconf_acm_N_enable_nacm (const xmlChar *)"enable-nacm"
#define y_ietf_netconf_acm_N_exec_default (const xmlChar *)"exec-default"
#define y_ietf_netconf_acm_N_group (const xmlChar *)"group"
#define y_ietf_netconf_acm_N_groups (const xmlChar *)"groups"
#define y_ietf_netconf_acm_N_module_name (const xmlChar *)"module-name"
#define y_ietf_netconf_acm_N_nacm (const xmlChar *)"nacm"
#define y_ietf_netconf_acm_N_name (const xmlChar *)"name"
#define y_ietf_netconf_acm_N_notification (const xmlChar *)"notification"
#define y_ietf_netconf_acm_N_notification_name \
    (const xmlChar *)"notification-name"
#define y_ietf_netconf_acm_N_path (const xmlChar *)"path"
#define y_ietf_netconf_acm_N_protocol_operation \
    (const xmlChar *)"protocol-operation"
#define y_ietf_netconf_acm_N_read_default (const xmlChar *)"read-default"
#define y_ietf_netconf_acm_N_rpc_name (const xmlChar *)"rpc-name"
#define y_ietf_netconf_acm_N_rule (const xmlChar *)"rule"
#define y_ietf_netconf_acm_N_rule_list (const xmlChar *)"rule-list"
#define y_ietf_netconf_acm_N_rule_type (const xmlChar *)"rule-type"
#define y_ietf_netconf_acm_N_user_name (const xmlChar *)"user-name"
#define y_ietf_netconf_acm_N_write_default (const xmlChar *)"write-default"


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
* FUNCTION agt_acm_ietf_init2
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
    agt_acm_ietf_init2 (void);


/********************************************************************
* FUNCTION agt_acm_ietf_init1
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
    agt_acm_ietf_init1 (void);


/********************************************************************
* FUNCTION agt_acm_ietf_cleanup
*
* Cleanup the external access control module
* 
*********************************************************************/
extern void
    agt_acm_ietf_cleanup (void);


/********************************************************************
* FUNCTION agt_acm_ietf_rpc_allowed
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
    agt_acm_ietf_rpc_allowed (xml_msg_hdr_t *msg,
                                const xmlChar *user,
                                const obj_template_t *rpcobj);


/********************************************************************
* FUNCTION agt_acm_ietf_notif_allowed
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
    agt_acm_ietf_notif_allowed (const xmlChar *user,
                                  const obj_template_t *notifobj);


/********************************************************************
* FUNCTION agt_acm_ietf_val_write_allowed
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
    agt_acm_ietf_val_write_allowed (xml_msg_hdr_t *msg,
                                    const xmlChar *user,
                                    val_value_t *newval,
                                    val_value_t *curval,
                                    op_editop_t editop);


/********************************************************************
* FUNCTION agt_acm_ietf_val_read_allowed
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
    agt_acm_ietf_val_read_allowed (xml_msg_hdr_t *msg,
                                   const xmlChar *user,
                                   val_value_t *val);


/********************************************************************
* FUNCTION agt_acm_ietf_init_msg_cache
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
    agt_acm_ietf_init_msg_cache (ses_cb_t *scb,
                                 xml_msg_hdr_t *msg);


/********************************************************************
* FUNCTION agt_acm_ietf_clear_session_cache
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
extern void agt_acm_ietf_clear_session_cache (ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_ietf_invalidate_session_cache
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
extern void agt_acm_ietf_invalidate_session_cache (ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_ietf_session_cache_valid
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
extern boolean agt_acm_ietf_session_cache_valid (const ses_cb_t *scb);


/********************************************************************
* FUNCTION agt_acm_ietf_clean_xpath_cache
*
* Clean any cached XPath results because the data rule results
* may not be valid anymore.
*
*********************************************************************/
extern void
    agt_acm_ietf_clean_xpath_cache (void);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_agt_acm_ietf */

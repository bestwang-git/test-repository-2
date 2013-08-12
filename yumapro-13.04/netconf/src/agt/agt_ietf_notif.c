
/* 
 * Copyright (c) 2008-2012, Andy Bierman, All Rights Reserved.
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *

*** Generated by yangdump-pro 12.09.29bd99a-M

    Combined SIL module
    module ietf-netconf-notifications
    revision 2012-02-06
    namespace urn:ietf:params:xml:ns:yang:ietf-netconf-notifications
    organization IETF NETCONF (Network Configuration Protocol) Working Group

 */

#include <xmlstring.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_cb.h"
#include "agt_cfg.h"
#include "agt_ietf_notif.h"
#include "agt_not.h"
#include "agt_timer.h"
#include "agt_util.h"
#include "cap.h"
#include "dlq.h"
#include "ncx.h"
#include "ncx_feature.h"
#include "ncxmod.h"
#include "ncxtypes.h"
#include "ses.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xml_util.h"


/* module static variables */
static ncx_module_t *ietf_netconf_notifications_mod;
static obj_template_t *netconf_config_change_obj;
static obj_template_t *netconf_capability_change_obj;
static obj_template_t *netconf_session_start_obj;
static obj_template_t *netconf_session_end_obj;
static obj_template_t *netconf_confirmed_commit_obj;

/* put your static variables here */
boolean ietf_notif_enabled;


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_init_static_vars
* 
* initialize module static variables
* 
********************************************************************/
static void y_ietf_netconf_notifications_init_static_vars (void)
{
    ietf_netconf_notifications_mod = NULL;
    netconf_config_change_obj = NULL;
    netconf_capability_change_obj = NULL;
    netconf_session_start_obj = NULL;
    netconf_session_end_obj = NULL;
    netconf_confirmed_commit_obj = NULL;

    /* init your static variables here */
    ietf_notif_enabled = FALSE;

} /* y_ietf_netconf_notifications_init_static_vars */


/********************************************************************
* FUNCTION add_std_common_session_parms
* 
* Add the common parms to the system notification
* 
* INPUTS:
*    notif == notification to add nodes to
*    parentobj == parent object of common session parms
*    parent == value node to add nodes to instead
*           == NULL if not used
*    username == username leaf value (NULL to skip)
*    session_id == session-id leaf value (0 to skip)
*    source_host == source-host leaf value (NULL to skip)
********************************************************************/
static void 
    add_std_common_session_parms (agt_not_msg_t *notif,
                                  obj_template_t *parentobj,
                                  val_value_t *parent,
                                  const xmlChar *username,
                                  uint32 session_id,
                                  const xmlChar *source_host)
{
    /* add username to payload */
    val_value_t *parmval;
    status_t res = NO_ERR;

    /* handle <server /> variant of changed-by container */
    if (session_id == 0) {
        parmval = agt_make_leaf(parentobj, 
                                y_ietf_netconf_notifications_N_server,
                                NULL, &res);
        if (parmval) {
            if (parent) {
                val_add_child(parmval, parent);
            } else {
                agt_not_add_to_payload(notif, parmval);
            }
            return;
        } else if (res == ERR_NCX_DEF_NOT_FOUND) {
            ;  // try the 3 parameters below */
        } else {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<server> parameter", get_error_string(res));
            return;
        }
    }

    /* handle 3 separate common leafs; start with username */
    if (username) {
        parmval = agt_make_leaf(parentobj,
                                y_ietf_netconf_notifications_N_username,
                                username, &res);
        if (parmval == NULL) {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<username> parameter\n", get_error_string(res));
        } else if (parent) {
            val_add_child(parmval, parent);
        } else {
            agt_not_add_to_payload(notif, parmval);
        }
    }
    
    /* add session_id to payload */
    if (session_id) {
        parmval = agt_make_uint_leaf(parentobj,
                                     y_ietf_netconf_notifications_N_session_id,
                                     session_id, &res);
        if (parmval == NULL) {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<session-id> parameter", get_error_string(res));
        } else if (parent) {
            val_add_child(parmval, parent);
        } else {
            agt_not_add_to_payload(notif, parmval);
        }
    }
    
    /* add source_host to payload */
    if (source_host) {
        parmval = agt_make_leaf(parentobj,
                                y_ietf_netconf_notifications_N_source_host,
                                source_host, &res);
        if (parmval == NULL) {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<source-host> parameter", get_error_string(res));
        } else if (parent) {
            val_add_child(parmval, parent);
        } else {
            agt_not_add_to_payload(notif, parmval);
        }
    }
    
} /* add_std_common_session_parms */


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_netconf_config_change_send
* 
* Send a y_ietf_netconf_notifications_netconf_config_change notification
* Called by your code when notification event occurs
* 
********************************************************************/
void y_ietf_netconf_notifications_netconf_config_change_send 
    (const xmlChar *username,
     uint32 session_id,
     const xmlChar *source_host,
     const xmlChar *datastore,
     dlq_hdr_t *auditrecQ)
{
    if (!ietf_notif_enabled) {
        log_info("\nGenerate <netconf-config-change> dropped: disabled");
        return;
    }

    if (LOGDEBUG) {
        log_debug("\nGenerating <netconf-config-change> notification");
    }

    agt_not_msg_t *notif = 
        agt_not_new_notification(netconf_config_change_obj);
    if (notif == NULL) {
        log_error("\nError: malloc failed, cannot send "
                  "<netconf-config-change> notification");
        return;
    }
    
    /* add container changed-by to payload */
    status_t res = NO_ERR;
    val_value_t *changedby_val = 
        agt_make_object(netconf_config_change_obj,
                        y_ietf_netconf_notifications_N_changed_by,
                        &res);
    if (changedby_val) {
        agt_not_add_to_payload(notif, changedby_val);
        add_std_common_session_parms(notif, changedby_val->obj,
                                     changedby_val, username, 
                                     session_id, source_host);
    } else {
        log_error("\nError: make container failed (%s), cannot send "
                  "<changed-by> parameter", get_error_string(res));
        res = NO_ERR;
    }

    /* add datastore to payload */
    val_value_t *parmval = 
        agt_make_leaf(netconf_config_change_obj,
                      y_ietf_netconf_notifications_N_datastore,
                      datastore, &res);
    if (parmval == NULL) {
        log_error("\nError: make leaf failed (%s), cannot send "
                  "<datastore> parameter", get_error_string(res));
    } else {
        agt_not_add_to_payload(notif, parmval);
    }

    /* add edit list node for each auditrec entry */
    agt_cfg_audit_rec_t *auditrec;
    for (auditrec = (agt_cfg_audit_rec_t *)dlq_firstEntry(auditrecQ);
         auditrec != NULL;
         auditrec = (agt_cfg_audit_rec_t *)dlq_nextEntry(auditrec)) {

        /* add netconf-config-change/edit container */
        val_value_t *listval = 
            agt_make_object(netconf_config_change_obj,
                            y_ietf_netconf_notifications_N_edit,
                            &res);
        if (listval == NULL) {
            log_error("\nError: make list failed (%s), cannot send "
                      "<edit> parameter", get_error_string(res));
            continue;
        }

        /* pass off listval malloc here */
        agt_not_add_to_payload(notif, listval);

        /* add netconf-config-change/edit/target */
        val_value_t *leafval = 
            agt_make_leaf(listval->obj, 
                          y_ietf_netconf_notifications_N_target,
                          auditrec->target, &res);
        if (leafval) {
            val_add_child(leafval, listval);
        } else {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<target> parameter", get_error_string(res));
        }

        /* add netconf-config-change/edit/operation */
        leafval = agt_make_leaf(listval->obj,
                                y_ietf_netconf_notifications_N_operation,
                                op_editop_name(auditrec->editop),
                                &res);
        if (leafval) {
            val_add_child(leafval, listval);
        } else {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<operation> parameter", get_error_string(res));
        }
    }

    agt_not_queue_notification(notif);
    
} /* y_ietf_netconf_notifications_netconf_config_change_send */


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_netconf_capability_change_send
* 
* Send a y_ietf_netconf_notifications_netconf_capability_change notification
* Called by your code when notification event occurs
* 
* !!! Limitation: server will only send ADD or DELETE for 1
* !!! module at a time
********************************************************************/
void y_ietf_netconf_notifications_netconf_capability_change_send
    (const xmlChar *username,
     uint32 session_id,
     const xmlChar *source_host,
     cap_change_t cap_change,
     const xmlChar *capstr)
{
    if (!ietf_notif_enabled) {
        log_info("\nGenerate <netconf-capability-change> dropped: disabled");
        return;
    }

    if (LOGDEBUG) {
        log_debug("\nGenerating <netconf-capability-change> notification");
    }

    agt_not_msg_t *notif = 
        agt_not_new_notification(netconf_capability_change_obj);
    if (notif == NULL) {
        log_error("\nError: malloc failed, cannot send "
        "<netconf-capability-change> notification");
        return;
    }
    
    /* add container changed-by to payload */
    status_t res = NO_ERR;
    val_value_t *changedby_val = 
        agt_make_object(netconf_capability_change_obj,
                        y_ietf_netconf_notifications_N_changed_by,
                        &res);
    if (changedby_val) {
        agt_not_add_to_payload(notif, changedby_val);
        add_std_common_session_parms(notif, changedby_val->obj,
                                     changedby_val, username, 
                                     session_id, source_host);
    } else {
        log_error("\nError: make container failed (%s), cannot send "
                  "<changed-by> parameter", get_error_string(res));
        res = NO_ERR;
    }

    const xmlChar *objname = NULL;
    switch (cap_change) {
    case CAP_CHANGE_ADD:
        objname = y_ietf_netconf_notifications_N_added_capability;
        break;
    case CAP_CHANGE_DELETE:
        objname = y_ietf_netconf_notifications_N_deleted_capability;
        break;
    case CAP_CHANGE_MODIFY:
        objname = y_ietf_netconf_notifications_N_modified_capability;
        break;
    case CAP_CHANGE_NONE:
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (objname) {
        /* add the URI for the capability change */
        val_value_t *leafval = 
            agt_make_leaf(netconf_capability_change_obj,
                          objname, capstr, &res);
        if (leafval) {
            agt_not_add_to_payload(notif, leafval);
        } else {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<%s> parameter", objname, get_error_string(res));
            res = NO_ERR;
        }
    }

    agt_not_queue_notification(notif);
    
} /* y_ietf_netconf_notifications_netconf_capability_change_send */


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_netconf_session_start_send
* 
* Send a y_ietf_netconf_notifications_netconf_session_start notification
* Called by your code when notification event occurs
* 
********************************************************************/
void y_ietf_netconf_notifications_netconf_session_start_send (
    const xmlChar *username,
    uint32 session_id,
    const xmlChar *source_host)
{
    if (!ietf_notif_enabled) {
        log_info("\nGenerate <netconf-session-start> dropped: disabled");
        return;
    }

    if (LOGDEBUG) {
        log_debug("\nGenerating <netconf-session-start> notification");
    }

    agt_not_msg_t *notif =     
        agt_not_new_notification(netconf_session_start_obj);
    if (notif == NULL) {
        log_error("\nError: malloc failed, cannot send "
        "<netconf-session-start> notification");
        return;
    }
    
    add_std_common_session_parms(notif, netconf_session_start_obj, NULL,
                                 username, session_id, source_host);

    agt_not_queue_notification(notif);
    
} /* y_ietf_netconf_notifications_netconf_session_start_send */


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_netconf_session_end_send
* 
* Send a y_ietf_netconf_notifications_netconf_session_end notification
* Called by your code when notification event occurs
* 
********************************************************************/
void y_ietf_netconf_notifications_netconf_session_end_send (
    const xmlChar *username,
    uint32 session_id,
    const xmlChar *source_host,
    uint32 killed_by,
    const xmlChar *termination_reason)
{
    agt_not_msg_t *notif;
    val_value_t *parmval;
    status_t res = NO_ERR;

    if (!ietf_notif_enabled) {
        log_info("\nGenerate <netconf-session-end> dropped: disabled");
        return;
    }

    if (LOGDEBUG) {
        log_debug("\nGenerating <netconf-session-end> notification");
    }
    
    notif = agt_not_new_notification(netconf_session_end_obj);
    if (notif == NULL) {
        log_error("\nError: malloc failed, cannot send "
        "<netconf-session-end> notification");
        return;
    }

    add_std_common_session_parms(notif, netconf_session_end_obj, NULL,
                                 username, session_id, source_host);
    
    /* add killed_by to payload */
    if (killed_by) {
        parmval = 
            agt_make_uint_leaf(netconf_session_end_obj,
                               y_ietf_netconf_notifications_N_killed_by,
                               killed_by, &res);
        if (parmval == NULL) {
            log_error(
                      "\nError: make leaf failed (%s), cannot send "
                      "<netconf-session-end> notification",
                      get_error_string(res));
        } else {
            agt_not_add_to_payload(notif, parmval);
        }
    }
    
    /* add termination_reason to payload */
    if (termination_reason) {
        if (!xml_strcmp(termination_reason, (const xmlChar *)"bad-start")) {
            /* the IETF version of the module combined bad-start with other */
            termination_reason = (const xmlChar *)"other";
        }
        parmval = 
            agt_make_leaf(netconf_session_end_obj,
                          y_ietf_netconf_notifications_N_termination_reason,
                          termination_reason, &res);
        if (parmval == NULL) {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<netconf-session-end> notification",
                      get_error_string(res));
        } else {
            agt_not_add_to_payload(notif, parmval);
        }
    }    

    agt_not_queue_notification(notif);

} /* y_ietf_netconf_notifications_netconf_session_end_send */


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_netconf_confirmed_commit_send
* 
* Send a y_ietf_netconf_notifications_netconf_confirmed_commit notification
* Called by your code when notification event occurs
* 
********************************************************************/
void y_ietf_netconf_notifications_netconf_confirmed_commit_send (
    const xmlChar *username,
    uint32 session_id,
    const xmlChar *source_host,
    const xmlChar *confirm_event,
    ncx_confirm_event_t event,
    uint32 timeout)
{
    if (!ietf_notif_enabled) {
        log_info("\nGenerate <netconf-confirmed-commit> dropped: disabled");
        return;
    }

    if (LOGDEBUG) {
        log_debug("\nGenerating <netconf-confirmed-commit> notification");
    }

    agt_not_msg_t *notif = 
        agt_not_new_notification(netconf_confirmed_commit_obj);
    if (notif == NULL) {
        log_error("\nError: malloc failed, cannot send "
        "<netconf-confirmed-commit> notification");
        return;
    }

    add_std_common_session_parms(notif, netconf_confirmed_commit_obj,
                                 NULL, username, session_id, source_host);
    
    /* add confirm_event to payload */
    status_t res = NO_ERR;
    val_value_t *parmval = 
        agt_make_leaf(netconf_confirmed_commit_obj,
                      y_ietf_netconf_notifications_N_confirm_event,
                      confirm_event, &res);
    if (parmval == NULL) {
        log_error("\nError: make leaf failed (%s), cannot send "
                  "<confirm-event> parameter", get_error_string(res));
        res = NO_ERR;
    } else {
        agt_not_add_to_payload(notif, parmval);
    }
    
    /* add timeout to payload */
    if (event == NCX_CC_EVENT_START || event == NCX_CC_EVENT_EXTEND) {
        parmval = 
            agt_make_uint_leaf(netconf_confirmed_commit_obj,
                               y_ietf_netconf_notifications_N_timeout,
                               timeout, &res);
        if (parmval == NULL) {
            log_error("\nError: make leaf failed (%s), cannot send "
                      "<timeout> parameter", get_error_string(res));
        } else {
            agt_not_add_to_payload(notif, parmval);
        }
    }

    agt_not_queue_notification(notif);
    
} /* y_ietf_netconf_notifications_netconf_confirmed_commit_send */


/********************************************************************
* FUNCTION y_ietf_netconf_notifications_init
* 
* initialize the ietf-netconf-notifications server instrumentation library
* 
* INPUTS:
*      none
* RETURNS:
*     error status
********************************************************************/
status_t y_ietf_netconf_notifications_init (void)
{
    status_t res = NO_ERR;
    agt_profile_t *agt_profile = agt_get_profile();

    y_ietf_netconf_notifications_init_static_vars();

    if (!agt_profile->agt_use_notifications) {
        ietf_notif_enabled = FALSE;
        return NO_ERR;
    }

    if (!agt_profile->agt_ietf_system_notifs) {
        ietf_notif_enabled = FALSE;
        return NO_ERR;
    }

    res = ncxmod_load_module(
        y_ietf_netconf_notifications_M_ietf_netconf_notifications,
        y_ietf_netconf_notifications_R_ietf_netconf_notifications,
        &agt_profile->agt_savedevQ,
        &ietf_netconf_notifications_mod);
    if (res != NO_ERR) {
        return res;
    }

    netconf_config_change_obj = ncx_find_object(
        ietf_netconf_notifications_mod,
        y_ietf_netconf_notifications_N_netconf_config_change);
    if (ietf_netconf_notifications_mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }
    netconf_capability_change_obj = ncx_find_object(
        ietf_netconf_notifications_mod,
        y_ietf_netconf_notifications_N_netconf_capability_change);
    if (ietf_netconf_notifications_mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }
    netconf_session_start_obj = ncx_find_object(
        ietf_netconf_notifications_mod,
        y_ietf_netconf_notifications_N_netconf_session_start);
    if (ietf_netconf_notifications_mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }
    netconf_session_end_obj = ncx_find_object(
        ietf_netconf_notifications_mod,
        y_ietf_netconf_notifications_N_netconf_session_end);
    if (ietf_netconf_notifications_mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }
    netconf_confirmed_commit_obj = ncx_find_object(
        ietf_netconf_notifications_mod,
        y_ietf_netconf_notifications_N_netconf_confirmed_commit);
    if (ietf_netconf_notifications_mod == NULL) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }
    /* put your module initialization code here */
    ietf_notif_enabled = TRUE;

    return res;
} /* y_ietf_netconf_notifications_init */

/********************************************************************
* FUNCTION y_ietf_netconf_notifications_init2
* 
* SIL init phase 2: non-config data structures
* Called after running config is loaded
* 
* RETURNS:
*     error status
********************************************************************/
status_t y_ietf_netconf_notifications_init2 (void)
{
    status_t res = NO_ERR;

    /* put your init2 code here */

    return res;
} /* y_ietf_netconf_notifications_init2 */

/********************************************************************
* FUNCTION y_ietf_netconf_notifications_cleanup
*    cleanup the server instrumentation library
* 
********************************************************************/
void y_ietf_netconf_notifications_cleanup (void)
{
    /* put your cleanup code here */
    ietf_notif_enabled = FALSE;
    
} /* y_ietf_netconf_notifications_cleanup */

/* END ietf_netconf_notifications.c */
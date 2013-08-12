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
/*  FILE: agt_acm.c

  Wrapper for 3 different ACM models.
  Each model needs to maintain its own message and/or session cache

   
                agt_acm

               /    |     \
              /     |      \

           IETF   Yuma   External (vendor provided)
        RFC 6536  NACM   ???


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
04feb06      abb      begun
01aug08      abb      convert from NCX PSD to YANG OBJ design
20feb10      abb      add enable-nacm leaf and notification-rules
                      change indexing to user-ordered rule-name
                      instead of allowed-rights bits field

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
#include "agt_acm_ietf.h"
#include "agt_acm_yuma.h"
#include "agt_cb.h"
#include "agt_not.h"
#include "agt_ses.h"
#include "agt_util.h"
#include "agt_val.h"
#include "def_reg.h"
#include "dlq.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncx_list.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "xpath.h"
#include "xpath1.h"


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
static boolean agt_acm_init_done = FALSE;

static const xmlChar *superuser;

static agt_acmode_t   acmode;

static uint32         deniedRpcCount;

static uint32         deniedDataWriteCount;

static uint32         deniedNotificationCount;

static boolean log_reads;

static boolean log_writes;


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION agt_acm_init
* 
* Initialize the NCX Agent access control module
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
status_t 
    agt_acm_init (void)
{
    if (agt_acm_init_done) {
        return SET_ERROR(ERR_INTERNAL_INIT_SEQ);
    }

    log_debug2("\nagt: Loading NETCONF Access Control module");

    status_t  res = NO_ERR;
    agt_profile_t  *agt_profile = agt_get_profile();

    superuser = NULL;
    acmode = AGT_ACMOD_ENFORCING;
    deniedRpcCount = 0;
    deniedDataWriteCount = 0;
    deniedNotificationCount = 0;
    agt_acm_init_done = TRUE;
    log_reads = FALSE;
    log_writes = TRUE;

    switch (agt_profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        log_debug2("\nmodel: none !!! ACM disabled !!!");
        break;
    case AGT_ACM_MODEL_IETF_NACM:
        res = agt_acm_ietf_init1();
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        res = agt_acm_yuma_init1();
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        log_debug2("\nmodel: external");
        res = agt_acm_extern_init1();
        break;
    default:
        log_error("\nError: invalid access control model '%d'", 
                  agt_profile->agt_acm_model);
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}  /* agt_acm_init */


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
status_t 
    agt_acm_init2 (void)
{
    const agt_profile_t   *profile = agt_get_profile();
    status_t               res = NO_ERR;

    if (!agt_acm_init_done) {
        return SET_ERROR(ERR_INTERNAL_INIT_SEQ);
    }

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        log_debug2("\nSkipping agt_acm_init2 -- no access control model");
        return NO_ERR;
    case AGT_ACM_MODEL_IETF_NACM:
        res = agt_acm_ietf_init2();
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        res = agt_acm_yuma_init2();
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        return agt_acm_extern_init2();
    default:
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    superuser = profile->agt_superuser;

    if (profile->agt_accesscontrol_enum != AGT_ACMOD_NONE) {
        acmode = profile->agt_accesscontrol_enum;
    }

    log_reads = profile->agt_log_acm_reads;
    log_writes = profile->agt_log_acm_writes;
    // no controls for RPC; notification treated as a read

    return res;

}  /* agt_acm_init2 */


/********************************************************************
* FUNCTION agt_acm_cleanup
*
* Cleanup the Server access control module
* 
*********************************************************************/
void
    agt_acm_cleanup (void)
{
    if (!agt_acm_init_done) {
        return;
    }

    agt_profile_t *profile = agt_get_profile();
    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        break;
    case AGT_ACM_MODEL_IETF_NACM:
        agt_acm_ietf_cleanup();
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        agt_acm_yuma_cleanup();
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        agt_acm_extern_cleanup();
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    superuser = NULL;
    agt_acm_init_done = FALSE;

}   /* agt_acm_cleanup */


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
boolean 
    agt_acm_rpc_allowed (xml_msg_hdr_t *msg,
                         const xmlChar *user,
                         const obj_template_t *rpcobj)
{
    agt_profile_t           *profile = agt_get_profile();
    boolean                  retval;

    assert( msg && "msg is NULL!" );
    assert( user && "user is NULL!" );
    assert( rpcobj && "rpcobj is NULL!" );

    log_debug2("\nagt_acm: check <%s> RPC allowed for user '%s'",
               obj_get_name(rpcobj), user);

    if (acmode == AGT_ACMOD_DISABLED) {
        log_debug2("\nagt_acm: PERMIT (NACM disabled)");
        return TRUE;
    }

    /* everybody is allowed to close their own session */
    if (obj_get_nsid(rpcobj) == xmlns_nc_id() &&
        !xml_strcmp(obj_get_name(rpcobj), NCX_EL_CLOSE_SESSION)) {
        log_debug2("\nagt_acm: PERMIT (close-session)");
        return TRUE;
    }

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        log_debug2("\nagt_acm: PERMIT (no-model)");
        return TRUE;
    case AGT_ACM_MODEL_IETF_NACM:
    case AGT_ACM_MODEL_YUMA_NACM:
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        retval = agt_acm_extern_rpc_allowed(msg, user, rpcobj);
        if (!retval) {
            deniedRpcCount++;
        }
        return retval;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        deniedRpcCount++;
        return FALSE;
    }

    /* super user is allowed to access anything */
    if (agt_acm_is_superuser(user)) {
        log_debug2("\nagt_acm: PERMIT (superuser)");
        return TRUE;
    }

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_IETF_NACM:
        retval = agt_acm_ietf_rpc_allowed(msg, user, rpcobj);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        retval = agt_acm_yuma_rpc_allowed(msg, user, rpcobj);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        retval = FALSE;
    }

    if (!retval) {
        deniedRpcCount++;
    }

    return retval;

}   /* agt_acm_rpc_allowed */


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
boolean 
    agt_acm_notif_allowed (const xmlChar *user,
                           const obj_template_t *notifobj)
{
    assert( user && "user is NULL!" );
    assert( notifobj && "notifobj is NULL!" );

    logfn_t logfn = (log_reads) ? log_debug2 : log_noop;

    (*logfn)("\nagt_acm: check <%s> Notification allowed for user '%s'",
             obj_get_name(notifobj), user);
    if (acmode == AGT_ACMOD_DISABLED) {
        (*logfn)("\nagt_acm: PERMIT (NACM disabled)");
        return TRUE;
    }

    boolean retval = TRUE;
    agt_profile_t *profile = agt_get_profile();
    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        log_debug2("\nagt_acm: PERMIT (no-model)");
        return TRUE;
    case AGT_ACM_MODEL_IETF_NACM:
    case AGT_ACM_MODEL_YUMA_NACM:
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        retval = agt_acm_extern_notif_allowed(user, notifobj);
        if (!retval) {
            deniedNotificationCount++;
        }
        return retval;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return FALSE;
    }

    /* do not block a replayComplete or notificationComplete event */
    if (agt_not_is_replay_event(notifobj)) {
        (*logfn)("\nagt_acm: PERMIT (replay event)");
        return TRUE;
    }

    /* super user is allowed to access anything */
    if (agt_acm_is_superuser(user)) {
        (*logfn)("\nagt_acm: PERMIT (superuser)");
        return TRUE;
    }

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_IETF_NACM:
        retval = agt_acm_ietf_notif_allowed(user, notifobj);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        retval = agt_acm_yuma_notif_allowed(user, notifobj);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        retval = FALSE;
    }

    if (!retval) {
        deniedNotificationCount++;
    }

    return retval;

}   /* agt_acm_notif_allowed */


/********************************************************************
* FUNCTION agt_acm_val_write_allowed
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
*   editop == requested CRUD operation
*
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
boolean 
    agt_acm_val_write_allowed (xml_msg_hdr_t *msg,
			       const xmlChar *user,
			       val_value_t *newval,
			       val_value_t *curval,
                               op_editop_t editop)
{
    logfn_t logfn = (log_writes) ? log_debug2 : log_noop;
    val_value_t *val = (newval) ? newval : curval;

    (*logfn)("\nagt_acm: check write <%s> allowed for user '%s'",
             val->name, user);

    /* do not check writes during the bootup process
     * cannot compare 'superuser' name in case it is
     * disabled or changed from the default
     */
    if (editop == OP_EDITOP_LOAD) {
        (*logfn)("\nagt_acm: PERMIT (OP_EDITOP_LOAD)");
        return TRUE;
    }

    /* defer check if no edit op requested on this node */
    if (editop == OP_EDITOP_NONE) {
        (*logfn)("\nagt_acm: PERMIT (OP_EDITOP_NONE)");
        return TRUE;
    }

    assert( msg && "msg is NULL!" );
    assert( user && "user is NULL!" );
    assert( val && "val is NULL!" );

    agt_profile_t *profile = agt_get_profile();
    if ((profile->agt_acm_model == AGT_ACM_MODEL_IETF_NACM ||
         profile->agt_acm_model == AGT_ACM_MODEL_YUMA_NACM) &&         
        msg->acm_cache == NULL) {
        /* this is a rollback operation so just allow it */
        (*logfn)("\nagt_acm: PERMIT (rollback)");
        return TRUE;
    }

    boolean retval;

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        log_debug2("\nagt_acm: PERMIT (no-model)");
        return TRUE;
    case AGT_ACM_MODEL_IETF_NACM:
        retval = agt_acm_ietf_val_write_allowed(msg, user, newval,
                                                curval, editop);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        retval = agt_acm_yuma_val_write_allowed(msg, user, newval,
                                                curval, editop);
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        retval = agt_acm_extern_val_write_allowed(msg, user, newval,
                                                  curval, editop);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        retval = FALSE;
    }

    if (!retval) {
        deniedDataWriteCount++;
    }

    return retval;

}   /* agt_acm_val_write_allowed */


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
boolean 
    agt_acm_val_read_allowed (xml_msg_hdr_t *msg,
                              const xmlChar *user,
                              val_value_t *val)
{
    assert( msg && "msg is NULL!" );
    //assert( msg->acm_cache && "cache is NULL!" );
    assert( user && "user is NULL!" );
    assert( val && "val is NULL!" );

    if (log_reads) {
        log_debug4("\nagt_acm: check read on <%s> allowed for user '%s'",
                   val->name, user);
    }

    boolean retval;
    agt_profile_t *profile = agt_get_profile();
    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        if (log_reads) {
            log_debug4("\nagt_acm: PERMIT (no-model)");
        }
        return TRUE;
    case AGT_ACM_MODEL_IETF_NACM:
        retval = agt_acm_ietf_val_read_allowed(msg, user, val);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        retval = agt_acm_yuma_val_read_allowed(msg, user, val);
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        retval = agt_acm_extern_val_read_allowed(msg, user, val);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        retval = FALSE;
    }

    return retval;

}   /* agt_acm_val_read_allowed */


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
status_t
    agt_acm_init_msg_cache (ses_cb_t *scb,
                            xml_msg_hdr_t *msg)
{
    assert( scb && "scb is NULL!" );
    assert( msg && "msg is NULL!" );

    if (msg->acm_cache) {
        SET_ERROR(ERR_INTERNAL_INIT_SEQ);
        agt_acm_clear_msg_cache(msg);
    }

    status_t res = NO_ERR;
    agt_profile_t *profile = agt_get_profile();
    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        break;
    case AGT_ACM_MODEL_IETF_NACM:
        res = agt_acm_ietf_init_msg_cache(scb, msg);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        res = agt_acm_yuma_init_msg_cache(scb, msg);
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        res = agt_acm_extern_init_msg_cache(scb, msg);
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }
    return res;

} /* agt_acm_init_msg_cache */


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
void agt_acm_clear_msg_cache (xml_msg_hdr_t *msg)
{
    assert( msg && "msg is NULL!" );
    msg->acm_cbfn = NULL;
    msg->acm_cache = NULL;

} /* agt_acm_clear_msg_cache */


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
void agt_acm_clear_session_cache (ses_cb_t *scb)
{
    assert( scb && "scb is NULL!" );
    agt_profile_t *profile = agt_get_profile();

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        break;
    case AGT_ACM_MODEL_IETF_NACM:
        agt_acm_ietf_clear_session_cache(scb);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        agt_acm_yuma_clear_session_cache(scb);
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

} /* agt_acm_clear_session_cache */


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
void agt_acm_invalidate_session_cache (ses_cb_t *scb)
{
    assert( scb && "scb is NULL!" );
    agt_profile_t *profile = agt_get_profile();

    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        break;
    case AGT_ACM_MODEL_IETF_NACM:
        agt_acm_ietf_invalidate_session_cache(scb);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        agt_acm_yuma_invalidate_session_cache(scb);
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

} /* agt_acm_invalidate_session_cache */


/********************************************************************
* FUNCTION agt_acm_session_cache_valid
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
boolean agt_acm_session_cache_valid (const ses_cb_t *scb)
{
    assert( scb && "scb is NULL!" );
    agt_profile_t *profile = agt_get_profile();
    boolean retval = FALSE;
    
    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        break;
    case AGT_ACM_MODEL_IETF_NACM:
        retval = agt_acm_ietf_session_cache_valid(scb);
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        retval = agt_acm_yuma_session_cache_valid(scb);
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    return retval;

} /* agt_acm_session_cache_valid */


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
boolean
    agt_acm_session_is_superuser (const ses_cb_t *scb)
{
    assert( scb && "scb is NULL!" );
    return agt_acm_is_superuser(scb->username);

}  /* agt_acm_session_is_superuser */


/********************************************************************
* FUNCTION agt_acm_get_acmode
*
* Get the --access-control mode
*
* RETURNS:
*   acmode
*********************************************************************/
agt_acmode_t
    agt_acm_get_acmode (void)
{
    return acmode;

}  /* agt_acm_get_acmode */


/********************************************************************
* FUNCTION agt_acm_set_acmode
*
* Set the --access-control mode
*
* INPUTS:
*   newmode == new enum to use for acmode
*********************************************************************/
void
    agt_acm_set_acmode (agt_acmode_t newmode)
{
    acmode = newmode;

}  /* agt_acm_set_acmode */


/********************************************************************
* FUNCTION agt_acm_get_log_writes
*
* Get the log_writes flag
*
* RETURNS:
*   log_writes value
*********************************************************************/
boolean
    agt_acm_get_log_writes (void)
{
    return log_writes;

}  /* agt_acm_get_log_writes */


/********************************************************************
* FUNCTION agt_acm_get_log_reads
*
* Get the log_reads flag
*
* RETURNS:
*   log_reads value
*********************************************************************/
boolean
    agt_acm_get_log_reads (void)
{
    return log_reads;

}  /* agt_acm_get_log_reads */


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
boolean
    agt_acm_is_superuser (const xmlChar *username)
{
    if (!superuser || !*superuser) {
        return FALSE;
    }
    if (!username || !*username) {
        return FALSE;
    }
    return (xml_strcmp(superuser, username)) ? FALSE : TRUE;

}  /* agt_acm_is_superuser */


/********************************************************************
* FUNCTION agt_acm_get_deniedRpcs
*
* Get the deniedRpcs counter
*
* RETURNS:
*   counter value
*********************************************************************/
uint32
    agt_acm_get_deniedRpcs (void)
{
    return deniedRpcCount;

}  /* agt_acm_get_deniedRpcs */


/********************************************************************
* FUNCTION agt_acm_get_deniedDataWrites
*
* Get the deniedDataWrites counter
*
* RETURNS:
*   counter value
*********************************************************************/
uint32
    agt_acm_get_deniedDataWrites (void)
{
    return deniedDataWriteCount;

}  /* agt_acm_get_deniedDataWrites */


/********************************************************************
* FUNCTION agt_acm_get_deniedNotifications
*
* Get the deniedNotification counter
*
* RETURNS:
*   counter value
*********************************************************************/
uint32
    agt_acm_get_deniedNotifications (void)
{
    return deniedNotificationCount;

}  /* agt_acm_get_deniedNotifications */



/********************************************************************
* FUNCTION agt_acm_clean_xpath_cache
*
* Clean any cached XPath results because the data rule results
* may not be valid anymore.
*********************************************************************/
void
    agt_acm_clean_xpath_cache (void)
{
    agt_profile_t *profile = agt_get_profile();
    switch (profile->agt_acm_model) {
    case AGT_ACM_MODEL_NONE:
        return;
    case AGT_ACM_MODEL_IETF_NACM:
        agt_acm_ietf_clean_xpath_cache();
        break;
    case AGT_ACM_MODEL_YUMA_NACM:
        //agt_acm_yuma_clean_xpath_cache();
        break;
    case AGT_ACM_MODEL_EXTERNAL:
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

}   /* agt_acm_clean_xpath_cache */


/* END file agt_acm.c */

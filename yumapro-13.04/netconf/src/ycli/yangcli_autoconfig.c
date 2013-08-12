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
/*  FILE: yangcli_autoconfig.c

   NETCONF YANG-based CLI Tool

   autoconfig mode support

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
08-dec-12    abb      begun; split from yangcli_config.c

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
#include "cap.h"
#include "log.h"
#include "mgr.h"
#include "mgr_not.h"
#include "mgr_ses.h"
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
#include "yangcli_autoconfig.h"
#include "yangcli_cmd.h"
#include "yangcli_notif.h"
#include "yangcli_util.h"


/********************************************************************
 * FUNCTION config_change_handler
 * 
 *  callback function for the IETF <netconf-config-change>
 *  and yuma <sysConfigChange> event types
 *
 * INPUTS:
 *   session_cb == session control block that received the notif
 *   modname == module defining the notification
 *   event_name == notification event name
 *   event_time == notification timestamp
 *   event == pointer to value 
 *
 *********************************************************************/
static void
    config_change_handler (session_cb_t *session_cb,
                           const xmlChar *modname,
                           const xmlChar *event_name,
                           const xmlChar *event_time,
                           val_value_t *event)
{
    (void)modname;
    (void)event_name;
    (void)event_time;
    (void)event;

    session_cb->config_tree_dirty = TRUE;

} /* config_change_handler */


/********************************************************************
* FUNCTION config_change_supported
* 
* Check if the NETCONF server supports config change notifications
*
* INPUTS:
*   scb == session control block to use
*
* RETURNS:
*    TRUE if config change notifications supported; FALSE if not
*********************************************************************/
static boolean
    config_change_supported (ses_cb_t *scb)
{
    if (!notifications_supported(scb)) {
        return FALSE;
    }

    const mgr_scb_t *mscb = (const mgr_scb_t *)scb->mgrcb;
    if (mscb == NULL) {
        return FALSE;  // should not happen
    }

    if (cap_find_modcap(&mscb->caplist, NC_CONFIG_CHANGE_MOD)) {
        return TRUE;
    }

    if (cap_find_modcap(&mscb->caplist, YUMA_CONFIG_CHANGE_MOD)) {
        return TRUE;
    }
    return FALSE;

}  /* config_change_supported */


/********************************************************************
* FUNCTION send_get_config_pdu_to_server
* 
* Send a <get-config> operation to the server
*
* INPUTS:
*   server_cb == server control block to use
*   session_cb == session control block to use
*   config_name == name of datastore to get
* RETURNS:
*    status
*********************************************************************/
status_t
    send_get_config_pdu_to_server (server_cb_t *server_cb,
                                   session_cb_t *session_cb,
                                   const xmlChar *config_name)
{
    log_debug("\nSending <get-config> request for running config\n");

    obj_template_t *rpc =
        ncx_find_object(get_netconf_mod(server_cb), NCX_EL_GET_CONFIG);
    if (!rpc) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    xmlns_id_t obj_nsid = obj_get_nsid(rpc);

    /* construct a method node */
    val_value_t *reqdata = xml_val_new_struct(obj_get_name(rpc), obj_nsid);
    if (!reqdata) {
        log_error("\nError allocating a new RPC request\n");
        return ERR_INTERNAL_MEM;
    }

    /* set the [un]lock/input/target node XML namespace */
    val_value_t *sourceval = xml_val_new_struct(NCX_EL_SOURCE, obj_nsid);
    if (!sourceval) {
        log_error("\nError allocating a new RPC request\n");
        val_free_value(reqdata);
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(sourceval, reqdata);
    }

    val_value_t *parmval = xml_val_new_flag(config_name, obj_nsid);
    if (!parmval) {
        log_error("\nError allocating a new RPC request\n");
        val_free_value(reqdata);
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(parmval, sourceval);
    }

    status_t res = send_request_to_server(session_cb, rpc, reqdata);

    return res;

} /* send_get_config_pdu_to_server */


/********************************************************************
* FUNCTION start_autoconfig_retrieval
* 
* Send a full <get-config> operation to the server
* This is the first get-config, so the session_cb->config_tree
* will be deleted if it is present, once the reply is received
*
* INPUTS:
*   server_cb == server control block to use
*   session_cb == session control block to use
*   config_name == name of datastore to get
* RETURNS:
*    status
*********************************************************************/
status_t
    start_autoconfig_retrieval (server_cb_t *server_cb,
                                session_cb_t *session_cb)
{
    log_debug("\nStarting autoconfig <get-config> of the running config");

    boolean try_notifs = FALSE;

    ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (scb && scb->state != SES_ST_SHUTDOWN_REQ) {
        if (notifications_supported(scb)) {
            if (LOGDEBUG2) {
                log_debug2("\nNotifications supported; checking config change");
            }
            if (config_change_supported(scb)) {
                if (LOGDEBUG2) {
                    log_debug2("\nconfig change notifications supported");
                }
                try_notifs = TRUE;
            } else {
                log_warn("\nWarning: config change notifications not "
                         "supported by this session\n"
                         "Manual autoconfig updates are required\n");
            }
        } else {
            log_warn("\nWarning: notifications not supported by this session\n"
                     "Manual autoconfig updates are required\n");
        }
    } else {
        log_error("\nError: session terminated; Skipping autoconfig\n");
        return ERR_NCX_OPERATION_FAILED;
    }

    session_cb->config_update_supported = try_notifs;

    // !!! add autonotifs parameter and module and start subscription
    // !!! so config-change events can be received

    status_t res = 
        send_get_config_pdu_to_server(server_cb, session_cb, NCX_EL_RUNNING);
    if (res == NO_ERR) {
        session_cb->config_full = TRUE;
        session_cb->command_mode = CMD_MODE_AUTOCONFIG;
    }
    return res;

}  /* start_autoconfig_retrieval */


/********************************************************************
 * FUNCTION autoconfig_init
 * 
 *  Initialize the autoconfig module
 *
 * RETURNS:
 *  status
 *********************************************************************/
status_t autoconfig_init (void)
{
    /* register event handler for <netconf-config-change> */
    status_t res =
        register_notif_event_handler(NC_CONFIG_CHANGE_MOD,
                                     NC_CONFIG_CHANGE_EVENT,
                                     config_change_handler);
    if (res != NO_ERR) {
        return res;
    }

    res = register_notif_event_handler(YUMA_CONFIG_CHANGE_MOD,
                                       YUMA_CONFIG_CHANGE_EVENT,
                                       config_change_handler);
    if (res != NO_ERR) {
        return res;
    }

    return NO_ERR;

}  /* autoconfig_init */


/********************************************************************
 * FUNCTION autoconfig_cleanup
 * 
 *  Cleanup the autoconfig module
 *
 *********************************************************************/
void autoconfig_cleanup (void)
{
    unregister_notif_event_handler(NC_CONFIG_CHANGE_MOD,
                                   NC_CONFIG_CHANGE_EVENT,
                                   config_change_handler);
    unregister_notif_event_handler(YUMA_CONFIG_CHANGE_MOD,
                                   YUMA_CONFIG_CHANGE_EVENT,
                                   config_change_handler);

}  /* autoconfig_cleanup */


/********************************************************************
 * FUNCTION do_update_config (local RPC)
 * 
 * update-config
 *
 * Update the configuration cache for the current session
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_update_config (server_cb_t *server_cb,
                      obj_template_t *rpc,
                      const xmlChar *line,
                      uint32  len)
{
    status_t res = NO_ERR;
    val_value_t *valset = get_valset(server_cb, rpc, &line[len], &res);

    if (res == NO_ERR || res == ERR_NCX_SKIPPED) {
        session_cb_t *session_cb = server_cb->cur_session_cb;
        if (!session_cb->autoconfig) {
            const xmlChar *name = (session_cb->session_cfg) ?
                session_cb->session_cfg->name : NCX_EL_DEFAULT;
            log_error("\nError: auto-configure mode disabled for "
                      "session '%s'\n", name);
            res = ERR_NCX_OPERATION_FAILED;
        } else {
            res = start_update_config(server_cb, session_cb, 
                                      NCX_CFGID_RUNNING);
        }
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}  /* do_update_config */


/********************************************************************
 * FUNCTION start_update_config
 * 
 * perform a config update; not a CLI access function
 *
 * Update the configuration cache for the current session
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 *    cfg_id == config to start get-config for
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    start_update_config (server_cb_t *server_cb,
                         session_cb_t *session_cb,
                         ncx_cfg_t cfg_id)
{
    status_t res = NO_ERR;

    if (session_cb->autoconfig) {
        const xmlChar *name = cfg_get_config_name(cfg_id);
        if (name) {
            res = send_get_config_pdu_to_server(server_cb, session_cb, name);
        } else {
            res = ERR_NCX_INVALID_VALUE;
        }
        if (res == NO_ERR) {
            session_cb->config_full = TRUE;
            session_cb->command_mode = CMD_MODE_AUTOCONFIG;
        }
    } else {
        res = ERR_NCX_OPERATION_FAILED;
    }

    return res;

}  /* start_update_config */


/* END yangcli_autoconfig.c */

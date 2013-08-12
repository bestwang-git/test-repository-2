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
/*  FILE: yangcli_save.c

   NETCONF YANG-based CLI Tool

   save command

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
13-aug-09    abb      begun; started from yangcli_cmd.c

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
#include "libtecla.h"

#include "procdefs.h"
#include "log.h"
#include "mgr.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "op.h"
#include "rpc.h"
#include "rpc_err.h"
#include "status.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_save.h"
#include "yangcli_util.h"


/********************************************************************
* FUNCTION send_copy_config_to_server
* 
* Send a <copy-config> operation to the server to support
* the save operation
*
* INPUTS:
*    server_cb == server control block to use
*
* OUTPUTS:
*    state may be changed or other action taken
*    config_content is consumed -- freed or transfered
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_copy_config_to_server (server_cb_t *server_cb)
{
    session_cb_t *session_cb = server_cb->cur_session_cb;
    status_t res = NO_ERR;

    /* get the <copy-config> template */
    obj_template_t *rpc =
        ncx_find_object(get_netconf_mod(server_cb), NCX_EL_COPY_CONFIG);
    if (!rpc) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* get the 'input' section container */
    obj_template_t *input = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (!input) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* construct a method + parameter tree */
    val_value_t *reqdata = xml_val_new_struct(obj_get_name(rpc), 
                                              obj_get_nsid(rpc));
    if (!reqdata) {
        log_error("\nError allocating a new RPC request");
        return ERR_INTERNAL_MEM;
    }

    /* set the edit-config/input/target node to the default_target */
    obj_template_t *child = obj_find_child(input, NC_MODULE, NCX_EL_TARGET);
    val_value_t *parm = val_new_value();
    if (!parm) {
        val_free_value(reqdata);
        return ERR_INTERNAL_MEM;
    }
    val_init_from_template(parm, child);
    val_add_child(parm, reqdata);

    val_value_t *target = xml_val_new_flag((const xmlChar *)"startup",
                              obj_get_nsid(child));
    if (!target) {
        val_free_value(reqdata);
        return ERR_INTERNAL_MEM;
    }
    val_add_child(target, parm);

    /* set the edit-config/input/default-operation node to 'none' */
    child = obj_find_child(input, NC_MODULE, NCX_EL_SOURCE);
    parm = val_new_value();
    if (!parm) {
        val_free_value(reqdata);
        return ERR_INTERNAL_MEM;
    }
    val_init_from_template(parm, child);
    val_add_child(parm, reqdata);

    val_value_t *source = xml_val_new_flag((const xmlChar *)"running",
                                           obj_get_nsid(child));
    if (!source) {
        val_free_value(reqdata);
        return ERR_INTERNAL_MEM;
    }
    val_add_child(source, parm);

    /* allocate an RPC request and send it */
    mgr_rpc_req_t *req = NULL;
    ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (!scb) {
        res = SET_ERROR(ERR_INTERNAL_PTR);
    } else {
        req = mgr_rpc_new_request(scb);
        if (!req) {
            res = ERR_INTERNAL_MEM;
            log_error("\nError allocating a new RPC request");
        } else {
            req->data = reqdata;
            req->rpc = rpc;
            req->timeout = session_cb->timeout;
        }
    }
        
    if (res == NO_ERR) {
        if (LOGDEBUG2) {
            log_debug2("\nabout to send RPC request with reqdata:");
            val_dump_value_max(reqdata, 0, session_cb->defindent,
                               DUMP_VAL_LOG, session_cb->display_mode,
                               FALSE, FALSE);
        }

        /* the request will be stored if this returns NO_ERR */
        res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
    }

    if (res != NO_ERR) {
        if (req) {
            mgr_rpc_free_request(req);
        } else if (reqdata) {
            val_free_value(reqdata);
        }
    } else {
        session_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_copy_config_to_server */


/********************************************************************
 * FUNCTION do_save
 * 
 * INPUTS:
 *    server_cb == server control block to use
 *
 * OUTPUTS:
 *   copy-config and/or commit operation will be sent to server
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_save (server_cb_t *server_cb)
{
    session_cb_t *session_cb = server_cb->cur_session_cb;
    status_t res = do_save_ex(server_cb, session_cb);
    if (res == ERR_NCX_SKIPPED) {
        res = NO_ERR;
    }
    return res;
}


/********************************************************************
 * FUNCTION do_save_ex
 * 
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 * OUTPUTS:
 *   copy-config and/or commit operation will be sent to server
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_save_ex (server_cb_t *server_cb,
                session_cb_t *session_cb)
{

    status_t res = NO_ERR;
    xmlChar *line = NULL;

    boolean use_debug =
        (session_cb->config_mode ||
         session_cb->command_mode == CMD_MODE_AUTOTEST);

    /* get the session info */
    const ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (!scb) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }
    const mgr_scb_t *mscb = (const mgr_scb_t *)scb->mgrcb;
    const xmlChar *emsg = (const xmlChar *)
        "\nSaving configuration to non-volative storage";
    if (use_debug) {
        log_debug("%s", emsg);
    } else {
        log_info("%s", emsg);
    }

    /* determine which commands to send */
    switch (mscb->targtyp) {
    case NCX_AGT_TARG_NONE:
        log_warn("\nWarning: No writable targets supported on this server");
        res = ERR_NCX_SKIPPED;
        break;
    case NCX_AGT_TARG_CAND_RUNNING:
        if (!xml_strcmp(session_cb->default_target, NCX_EL_CANDIDATE)) {
            line = xml_strdup(NCX_EL_COMMIT);
            if (line) {
                res = conn_command(server_cb, line, FALSE, TRUE);
                m__free(line);
            } else {
                res = ERR_INTERNAL_MEM;
                log_error("\nError: Malloc failed");
            }
            if (res == NO_ERR &&
                mscb->starttyp == NCX_AGT_START_DISTINCT) {
                /* need 2 operations so set the command mode and the 
                 * reply handler will initiate the 2nd command
                 * if the first one worked
                 */
                session_cb->return_command_mode = session_cb->command_mode;
                session_cb->command_mode = CMD_MODE_SAVE;
            }
        } else {
            if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
                res = send_copy_config_to_server(server_cb);
                if (res != NO_ERR) {
                    log_error("\nError: send copy-config failed (%s)",
                              get_error_string(res));
                }
            } else {
                const xmlChar *msg = (const xmlChar *)
                    "\nNo distinct save operation needed for this server";
                if (use_debug) {
                    log_debug("%s", msg);
                } else {
                    log_info("%s", msg);
                }
                res = ERR_NCX_SKIPPED;
            }
        }
        break;
    case NCX_AGT_TARG_CANDIDATE:
        line = xml_strdup(NCX_EL_COMMIT);
        if (line) {
            res = conn_command(server_cb, line, FALSE, TRUE);
            m__free(line);
        } else {
            res = ERR_INTERNAL_MEM;
            log_error("\nError: Malloc failed");
        }
        /* if auto-test mode then do not use startup, wait until end */
        if (res == NO_ERR && 
            session_cb->command_mode != CMD_MODE_AUTOTEST &&
            mscb->starttyp == NCX_AGT_START_DISTINCT) {
            /* need 2 operations so set the command mode and the 
             * reply handler will initiate the 2nd command
             * if the first one worked
             */
            session_cb->return_command_mode = session_cb->command_mode;
            session_cb->command_mode = CMD_MODE_SAVE;
        }
        break;
    case NCX_AGT_TARG_RUNNING:
        if (mscb->starttyp == NCX_AGT_START_DISTINCT &&
            session_cb->command_mode != CMD_MODE_AUTOTEST) {
            res = send_copy_config_to_server(server_cb);
            if (res != NO_ERR) {
                log_error("\nError: send copy-config failed (%s)",
                          get_error_string(res));
            }
        } else {
            const xmlChar *msg = (const xmlChar *)
                "\nNo distinct save operation needed for this server";
            if (use_debug) {
                log_debug("%s", msg);
            } else {
                log_info("%s", msg);
            }
            res = ERR_NCX_SKIPPED;
        }
        break;
    case NCX_AGT_TARG_LOCAL:
        log_error("Error: Local URL target not supported");
        res = ERR_NCX_SKIPPED;
        break;
    case NCX_AGT_TARG_REMOTE:
        log_error("Error: Local URL target not supported");
        res = ERR_NCX_SKIPPED;
        break;
    default:
        log_error("Error: Internal target not set");
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}  /* do_save_ex */


/********************************************************************
 * FUNCTION finish_save
 * 
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 * OUTPUTS:
 *   copy-config will be sent to server
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    finish_save (server_cb_t *server_cb,
                 session_cb_t *session_cb)
{
    status_t res = NO_ERR;

    /* get the session info */
    const ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (!scb) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }
    const mgr_scb_t *mscb = (const mgr_scb_t *)scb->mgrcb;
    if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
        const xmlChar *str = (const xmlChar *)
            "\nFinal step saving configuration to non-volative storage";
        if (session_cb->config_mode ||
            session_cb->command_mode != CMD_MODE_NORMAL) {
            log_debug("%s", str);
        } else {
            log_info("%s", str);
        }

        res = send_copy_config_to_server(server_cb);
        if (res != NO_ERR) {
            log_error("\nError: send copy-config failed (%s)",
                      get_error_string(res));
        }
    } else {
        log_debug("\nNo distinct save operation needed "
                 "for this server");
    }

    return res;

}  /* finish_save */


/* END yangcli_save.c */

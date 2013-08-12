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
/*  FILE: yangcli_autonotif.c

   NETCONF YANG-based CLI Tool

   autonotifs mode support

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
#include "yangcli_autonotif.h"
#include "yangcli_cmd.h"
#include "yangcli_notif.h"
#include "yangcli_util.h"


/********************************************************************
* FUNCTION send_create_subscription_to_server
* 
* Send a <create-subscription> operation to the server
*
* INPUTS:
*   server_cb == server control block to use
*   session_cb == session control block to use
*   stream_name == name of stream for subscription (NULL == NETCONF)
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_create_subscription_to_server (server_cb_t *server_cb,
                                        session_cb_t *session_cb,
                                        const xmlChar *stream_name)
{
    if (LOGDEBUG) {
        log_debug("\nSending <create-subscription> request for '%s' stream\n",
                  stream_name ? stream_name : NCX_DEF_STREAM_NAME);
    }

    ncx_module_t *notmod = get_notif_mod(server_cb);
    if (notmod == NULL) {
        log_error("\nError finding notifications module for this session\n");
        return ERR_NCX_MOD_NOT_FOUND;
    }

    obj_template_t *rpc =
        ncx_find_object(notmod, NCX_EL_CREATE_SUBSCRIPTION);
    if (!rpc) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    xmlns_id_t obj_nsid = obj_get_nsid(rpc);
    val_value_t *reqdata = NULL;

    /* construct a method node */

    if (stream_name && xml_strcmp(stream_name, NCX_DEF_STREAM_NAME)) {
        reqdata = xml_val_new_struct(obj_get_name(rpc), obj_nsid);
        if (!reqdata) {
            log_error("\nError allocating a new RPC request\n");
            return ERR_INTERNAL_MEM;
        }

        /* add a stream name parameter to the request */
        val_value_t *stream =
            xml_val_new_cstring(NCX_EL_STREAM, obj_nsid, stream_name);
        if (stream == NULL) {
            log_error("\nError allocating stream parameter for RPC request\n");
            val_free_value(reqdata);
            return ERR_INTERNAL_MEM;
        }
        val_add_child(stream, reqdata);
    } else {
        reqdata = xml_val_new_flag(obj_get_name(rpc), obj_nsid);
        if (!reqdata) {
            log_error("\nError allocating a new RPC request\n");
            return ERR_INTERNAL_MEM;
        }
    }

    status_t res = send_request_to_server(session_cb, rpc, reqdata);

    return res;

} /* send_create_subscription_to_server */


/********************************************************************
* FUNCTION start_autonotif_mode
* 
* Send a <create-subscription> operation to the server, only
* if the server indicates it supports notifications and interleave
*
* INPUTS:
*   server_cb == server control block to use
*   session_cb == session control block to use
*   stream_name == name of notification stream to use (NULL = NETCONF)
*
* RETURNS:
*    status
*********************************************************************/
status_t
    start_autonotif_mode (server_cb_t *server_cb,
                          session_cb_t *session_cb,
                          const xmlChar *stream_name)
{
    ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (scb && scb->state != SES_ST_SHUTDOWN_REQ) {
        if (!notifications_supported(scb)) {
            log_debug("\nNotifications not supported by this session; "
                      "Skipping autonotif mode\n");
            return ERR_NCX_SKIPPED;
        }
    } else {
        log_error("\nError: session terminated; Skipping autonotif\n");
        return ERR_NCX_OPERATION_FAILED;
    }

    status_t res = 
        send_create_subscription_to_server(server_cb, session_cb,
                                           stream_name);
    if (res == NO_ERR) {
        session_cb->command_mode = CMD_MODE_AUTONOTIF;
    }
    return res;

}  /* start_autonotif_mode */


/* END yangcli_autonotif.c */

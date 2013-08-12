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
#ifndef _H_yangcli_autoconfig
#define _H_yangcli_autoconfig

/*  FILE: yangcli_autoconfig.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   NETCONF YANG-based CLI Tool

   autoconfig mode support
  
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
08-dec-12    abb      Begun

*/

#include <xmlstring.h>

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define NC_CONFIG_CHANGE_MOD (const xmlChar *)"ietf-netconf-notifications"
#define NC_CONFIG_CHANGE_EVENT (const xmlChar *)"netconf-config-change"

#define YUMA_CONFIG_CHANGE_MOD (const xmlChar *)"yuma-system"
#define YUMA_CONFIG_CHANGE_EVENT (const xmlChar *)"sysConfigChange"


/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/


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
extern status_t
    send_get_config_pdu_to_server (server_cb_t *server_cb,
                                   session_cb_t *session_cb,
                                   const xmlChar *config_name);


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
extern status_t
    start_autoconfig_retrieval (server_cb_t *server_cb,
                                session_cb_t *session_cb);


/********************************************************************
 * FUNCTION autoconfig_init
 * 
 *  Initialize the autoconfig module
 *
 * RETURNS:
 *  status
 *********************************************************************/
extern status_t autoconfig_init (void);


/********************************************************************
 * FUNCTION autoconfig_cleanup
 * 
 *  Cleanup the autoconfig module
 *
 *********************************************************************/
extern void autoconfig_cleanup (void);


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
extern status_t
    do_update_config (server_cb_t *server_cb,
                      obj_template_t *rpc,
                      const xmlChar *line,
                      uint32  len);

/********************************************************************
 * FUNCTION start_update_config
 * 
 * start a config update; not a CLI access function
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
extern status_t
    start_update_config (server_cb_t *server_cb,
                         session_cb_t *session_cb,
                         ncx_cfg_t cfg_id);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_autoconfig */
